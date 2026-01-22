/**
 * @file uart_send.c
 * @brief UART Send Layer Implementation
 * @version 1.00
 * @date 2025-12-30
 * @author
 *   Donzel
 *
 * Implements UART transmission with synchronous and asynchronous modes.
 * Uses OS and UART abstraction interfaces. Handles thread creation,
 * synchronization, and TX completion callbacks via ISR and thread context.
 */

#include "uart_send.h"
#include <stdbool.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/*                          Convenience Macros                                */
/* -------------------------------------------------------------------------- */

/** OS interface accessor */
#define OS_INTERFACE(p)     ((p)->uart_tx_input_arg->os_interface)

/** UART hardware interface accessor */
#define UART_INTERFACE(p)   ((p)->uart_tx_input_arg->tx_uart_ops)

/** Private data accessor */
#define PRIV_DATA(p)        ((p)->uart_tx_priv_data)

/** TX buffer pointer */
#define SEND_BUF(p)         ((p)->uart_tx_input_arg->send_buf_att->send_buf)

/** TX buffer size */
#define SEND_BUF_SIZE(p)    ((p)->uart_tx_input_arg->send_buf_att->buffer_size)

#if (CUSTOM_TX_THREAD_ATT)
/** Thread attributes accessor */
#   define THREAD_ATT(p)           ((p)->uart_tx_input_arg->thread_att)

/** Async thread attributes */
#   define ASY_SEND_THREAD_ATT(p)  ((p)->uart_tx_input_arg->thread_att->tx_asy_thread_att)

/** Completion thread attributes */
#   define TX_CPL_THREAD_ATT(p)    ((p)->uart_tx_input_arg->thread_att->tx_cpl_thread_att)
#endif  

#if(UART_TX_RESOURCE_TIMEOUT_TICK)  
/** Start TX resource timeout timer (if enabled) */
#define UART_TX_CREATE_TIMEOUT_TIMER(p) \
    OS_INTERFACE(p)->pf_timer_create(&PRIV_DATA(p)->timer, "tx_timer",\
                                        UART_TX_RESOURCE_TIMEOUT_TICK, 0, timer_cb, p)
#define UART_TX_START_TIMEOUT_TIMER(p) \
    do { \
        if (OS_INTERFACE(p)->pf_timer_start(PRIV_DATA(p)->timer, 0) != 0) \
            US_DEBUG_ERR("Timer start fail!\r\n"); \
    } while (0)
/** Stop TX resource timeout timer (if enabled) */
#define UART_TX_STOP_TIMEOUT_TIMER(p) \
    do { \
        OS_INTERFACE(p)->pf_timer_stop(PRIV_DATA(p)->timer, 0); \
    } while (0)        
#else
/** No-op if timeout timer is disabled */
#define UART_TX_CREATE_TIMEOUT_TIMER(p)
#define UART_TX_START_TIMEOUT_TIMER(p)
#define UART_TX_STOP_TIMEOUT_TIMER(p)
#endif

/* -------------------------------------------------------------------------- */
/*                     Synchronization Helper Macros (API)                    */
/* -------------------------------------------------------------------------- */
/**
 * @brief Specific semaphore/mutex operations.
 *
 * Depending on RESOURCE_SYN_MODE_DEFAULT, these map to OS semaphore
 * or mutex primitives for create, acquire, and release.
 */
#if (RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_SEMA)

#   define TX_RESOURCE_CREATE(p)   \
        do {                                                                  \
            OS_INTERFACE(p)->pf_os_sema_create(&PRIV_DATA(p)->tx_resource_sema_handle); \
            OS_INTERFACE(p)->pf_os_sema_release(PRIV_DATA(p)->tx_resource_sema_handle); \
        } while (0)
#   define TX_RESOURCE_RELEASE_FIRST_HALF(p)                                           \
        OS_INTERFACE(p)->pf_os_sema_release(PRIV_DATA(p)->tx_resource_sema_handle); \
#   define TX_RESOURCE_RELEASE_SECOND_HALF(p) 
#   define TX_RESOURCE_ACQUIRE(p, timeout) \
        OS_INTERFACE(p)->pf_os_sema_acquire(PRIV_DATA(p)->tx_resource_sema_handle, timeout)
        
#elif (RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX)

#   define TX_RESOURCE_CREATE(p)                                             \
        do {                                                                  \
            OS_INTERFACE(p)->pf_os_mutex_create(&PRIV_DATA(p)->tx_resource_mutex_handle); \
            OS_INTERFACE(p)->pf_os_sema_create(&PRIV_DATA(p)->tx_resource_syn_sema_handle); \
            OS_INTERFACE(p)->pf_os_sema_acquire(PRIV_DATA(p)->tx_resource_syn_sema_handle, 0); \
        } while (0)
#   define TX_RESOURCE_RELEASE_FIRST_HALF(p)                                            \
        OS_INTERFACE(p)->pf_os_sema_release(PRIV_DATA(p)->tx_resource_syn_sema_handle)
#   define TX_RESOURCE_RELEASE_SECOND_HALF(p)                                            \
        do {                                                                  \
            OS_INTERFACE(p)->pf_os_sema_acquire(PRIV_DATA(p)->tx_resource_syn_sema_handle, TX_OS_DELAY_MAX);\
            OS_INTERFACE(p)->pf_os_mutex_release(PRIV_DATA(p)->tx_resource_mutex_handle); \
        } while (0)
#   define TX_RESOURCE_ACQUIRE(p, timeout)                                            \
        OS_INTERFACE(p)->pf_os_mutex_acquire(PRIV_DATA(p)->tx_resource_mutex_handle, timeout)
#endif

/* -------------------------------------------------------------------------- */
/*                          Private Data Structure                            */
/* -------------------------------------------------------------------------- */

/** Private runtime data for UART TX handler */
typedef struct uart_tx_priv_data 
{
    bool is_inited;
#if(RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_SEMA)    
    void *tx_resource_sema_handle;
#elif(RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX)
    void *tx_resource_syn_sema_handle;
    void *tx_resource_mutex_handle;
#endif
    void *tx_cpl_ctx_queue_handle;  
    void *asy_tx_sema;
#if (IS_ENABLE_CPL_THREAD)
    void *thread_cpl_ctx_queue_handle;
#endif
    volatile uint32_t write_offset;
#if(UART_TX_RESOURCE_TIMEOUT_TICK)
    void *timer;
#endif
    volatile bool is_sending;
} uart_tx_priv_data_t;

/* 1 is busy else is 0 */
static inline bool check_tx_busy(uart_tx_handler_t *const self)
{
    if (PRIV_DATA(self)->is_sending)
    {
        US_DEBUG_ERR("uart send is busy!\r\n");
        PRIV_DATA(self)->is_sending = false;
        TX_RESOURCE_RELEASE_FIRST_HALF(self);
        TX_RESOURCE_RELEASE_SECOND_HALF(self);
        return true;
    }
    return false;
}

/**
 * Send data synchronously (blocks until TX resource is available)
 * @note Data stability requirement depends on resource sync mode.
 */
static uart_tx_status_t send_syn(struct uart_tx *const self,
                                 uint8_t *const data,
                                 uint16_t length,
                                 uart_tx_cpl_ctx_t *uart_tx_cpl_ctx)
{
    if (!self) 
        return UART_TX_ERR_PARAM_INVALID;
    if (!PRIV_DATA(self)->is_inited) 
        return UART_TX_ERR_HANDLER_NOT_READY;

    TX_RESOURCE_ACQUIRE(self, TX_OS_DELAY_MAX);
    if(check_tx_busy(self))
        return UART_TX_ERR_OTHERS; 

    PRIORITY_INVERSION_TEST_FUNC();
    OS_INTERFACE(self)->pf_os_enter_critical();

    if (uart_tx_cpl_ctx &&
        OS_INTERFACE(self)->pf_os_queue_put(PRIV_DATA(self)->tx_cpl_ctx_queue_handle,
                                            uart_tx_cpl_ctx, 0) != 0)
    {
        US_DEBUG_ERR("Queue put failed in sync send\r\n");
        TX_RESOURCE_RELEASE_FIRST_HALF(self);
        TX_RESOURCE_RELEASE_SECOND_HALF(self);
        OS_INTERFACE(self)->pf_os_exit_critical(0);
        return UART_TX_ERR_OTHERS;
    }

    PRIV_DATA(self)->is_sending = true;
    UART_TX_START_TIMEOUT_TIMER(self);
    UART_INTERFACE(self)->pf_uart_write_dma(data, length);

    OS_INTERFACE(self)->pf_os_exit_critical(0);
  
    TX_RESOURCE_RELEASE_SECOND_HALF(self); 

    return UART_TX_OK;
}

/* -------------------------------------------------------------------------- */
/*                         Asynchronous Send Function                          */
/* -------------------------------------------------------------------------- */

/**
 * Send data asynchronously (buffers data and returns immediately)
 * @note Data stability requirement depends on async send mode.
 */
static uart_tx_status_t send_asy(struct uart_tx *const self,
                                 uint8_t *const data, uint16_t length)
{
    if (!self) 
        return UART_TX_ERR_PARAM_INVALID;
    if (!PRIV_DATA(self)->is_inited) 
        return UART_TX_ERR_HANDLER_NOT_READY;

    OS_INTERFACE(self)->pf_os_enter_critical();
    uint32_t write_off = PRIV_DATA(self)->write_offset;
    uint32_t remain = SEND_BUF_SIZE(self) - write_off;

    if (remain < length)
    {
        OS_INTERFACE(self)->pf_os_exit_critical(0);
        US_DEBUG_ERR("TX buffer insufficient\r\n");
        return UART_TX_ERR_BUFFER_NOT_SUFFICIENT;
    }
/* Direct send is not allowed in mutex mode to prevent blocking the caller thread. */
#if (UART_ASYNC_SEND_MODE_DEFAULT == UART_ASYNC_SEND_MODE_DIRECT_FIRST && RESOURCE_SYN_MODE_DEFAULT != RESOURCE_SYN_MODE_MUTEX)
    if (0 == TX_RESOURCE_ACQUIRE(self, 0))
    {
        if(check_tx_busy(self))
            return UART_TX_ERR_OTHERS; 
        PRIV_DATA(self)->is_sending = true;
        UART_TX_START_TIMEOUT_TIMER(self);
        UART_INTERFACE(self)->pf_uart_write_dma(data, length);
        OS_INTERFACE(self)->pf_os_exit_critical(0);
        return UART_TX_OK;
    }
#endif
    
    PRIV_DATA(self)->write_offset += length;
    memcpy(SEND_BUF(self) + write_off, data, length);
    
    OS_INTERFACE(self)->pf_os_exit_critical(0);

    OS_INTERFACE(self)->pf_os_sema_release(PRIV_DATA(self)->asy_tx_sema);
    
    return UART_TX_OK;
}

/* -------------------------------------------------------------------------- */
/*                          Asynchronous Send Thread                          */
/* -------------------------------------------------------------------------- */

/**
 * Async send thread (sends buffered data when TX resource is available)
 */
static void asy_send_thread(void *arg)
{
    uart_tx_handler_t *self = (uart_tx_handler_t *)arg;
    if (!self || !PRIV_DATA(self)->is_inited)
    {
        US_DEBUG_ERR("asy send thread error\r\n");
        return;
    }    
    while (1)
    {
        OS_INTERFACE(self)->pf_os_sema_acquire(PRIV_DATA(self)->asy_tx_sema, TX_OS_DELAY_MAX);
        TX_RESOURCE_ACQUIRE(self, TX_OS_DELAY_MAX);
        if(check_tx_busy(self))
            return; 
        OS_INTERFACE(self)->pf_os_enter_critical();
        OS_INTERFACE(self)->pf_os_sema_acquire(PRIV_DATA(self)->asy_tx_sema, 0);/* Consume residual binary semaphore */
        US_DEBUG_OUT("write offset=%d\r\n", PRIV_DATA(self)->write_offset);
        if (PRIV_DATA(self)->write_offset)
        {
            PRIV_DATA(self)->is_sending = true;
            UART_TX_START_TIMEOUT_TIMER(self);
            UART_INTERFACE(self)->pf_uart_write_dma(SEND_BUF(self), PRIV_DATA(self)->write_offset);
            PRIV_DATA(self)->write_offset = 0;
        }
        else
        {
            US_DEBUG_ERR("Async send length = 0\r\n");
            TX_RESOURCE_RELEASE_FIRST_HALF(self);
        }        
        OS_INTERFACE(self)->pf_os_exit_critical(0);
  
        TX_RESOURCE_RELEASE_SECOND_HALF(self);    
    }
}

#if (IS_ENABLE_CPL_THREAD)
/* -------------------------------------------------------------------------- */
/*                         Completion Callback Thread                          */
/* -------------------------------------------------------------------------- */

/**
 * TX completion thread (calls thread context callbacks)
 */
static void tx_cpl_thread(void *arg)
{
    uart_tx_handler_t *self = (uart_tx_handler_t *)arg;
    if (!self || !PRIV_DATA(self)->is_inited)
    {
        US_DEBUG_ERR("tx complete thread error\r\n");
        return;
    }
        
    while (1)
    {
        uart_tx_cpl_cb_ctx_t ctx;
        OS_INTERFACE(self)->pf_os_queue_get(PRIV_DATA(self)->thread_cpl_ctx_queue_handle,
                                            &ctx, TX_OS_DELAY_MAX);
        if (ctx.pf_tx_cpl_cb)
            ctx.pf_tx_cpl_cb(ctx.arg);
    }
}
#endif

#if(UART_TX_RESOURCE_TIMEOUT_TICK)
/* -------------------------------------------------------------------------- */
/*                         Timeout Callback Function                          */
/* -------------------------------------------------------------------------- */

/**
 * TX resource timeout callback (resets TX state)
 */
static void timer_cb(void *timer_handle, void *arg)
{
    if (!timer_handle || !arg)
    {
        US_DEBUG_ERR("uart send timeout error\r\n"); 
        return;   
    } 
        

    uart_tx_handler_t *self = (uart_tx_handler_t *)arg;
    if (timer_handle != PRIV_DATA(self)->timer) 
    {
        US_DEBUG_ERR("uart send timeout handler error\r\n"); 
        return;   
    } 

    US_DEBUG_ERR("TX resource timeout!\r\n");
    reset_tx_state(self);
}
#endif

/* -------------------------------------------------------------------------- */
/*                         Initialization Function                             */
/* -------------------------------------------------------------------------- */

/**
 * Initialize UART TX handler
 */
uart_tx_status_t uart_tx_inst(uart_tx_handler_t *const self,
                              uart_tx_input_arg_t *const args)
{
    if (!self || !args || !args->send_buf_att ||
        !args->tx_uart_ops || !args->os_interface)
        return UART_TX_ERR_PARAM_INVALID;

    if(self->uart_tx_priv_data && self->uart_tx_priv_data->is_inited)
    {
        return UART_TX_ERR_ALREADY_INITED;
    } 
        
    self->uart_tx_input_arg = args;

    if (!SEND_BUF(self) || !SEND_BUF_SIZE(self))
        return UART_TX_ERR_PARAM_INVALID;

    if (!UART_INTERFACE(self)->pf_uart_init ||
        !UART_INTERFACE(self)->pf_uart_deinit ||
        !UART_INTERFACE(self)->pf_uart_write_dma)
        return UART_TX_ERR_PARAM_INVALID;
    
    if (!OS_INTERFACE(self)->pf_os_thread_create ||
        !OS_INTERFACE(self)->pf_os_thread_delete ||
        !OS_INTERFACE(self)->pf_os_sema_create ||
        !OS_INTERFACE(self)->pf_os_sema_delete ||
        !OS_INTERFACE(self)->pf_os_sema_acquire ||
        !OS_INTERFACE(self)->pf_os_sema_release ||
        !OS_INTERFACE(self)->pf_os_queue_create ||
        !OS_INTERFACE(self)->pf_os_queue_put ||
        !OS_INTERFACE(self)->pf_os_queue_get ||
#if (UART_TX_RESOURCE_TIMEOUT_TICK)
        !OS_INTERFACE(self)->pf_timer_create ||
        !OS_INTERFACE(self)->pf_timer_start ||
        !OS_INTERFACE(self)->pf_timer_stop ||
#endif
#if(RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX)
        !OS_INTERFACE(self)->pf_os_mutex_create ||
        !OS_INTERFACE(self)->pf_os_mutex_delete ||
        !OS_INTERFACE(self)->pf_os_mutex_acquire ||
        !OS_INTERFACE(self)->pf_os_mutex_release ||
#endif
        !OS_INTERFACE(self)->pf_os_enter_critical ||
        !OS_INTERFACE(self)->pf_os_exit_critical)
        return UART_TX_ERR_PARAM_INVALID;   

    self->uart_tx_priv_data = malloc(sizeof(uart_tx_priv_data_t));
    if (!PRIV_DATA(self))
        return UART_TX_ERR_OTHERS;

    PRIV_DATA(self)->is_inited = false;
    PRIV_DATA(self)->write_offset = 0;
    PRIV_DATA(self)->is_sending = false;

    /* Async send thread */
    uint32_t stack = TX_ASY_THREAD_STACK_DEPTH;
    uint32_t prio = TX_ASY_THREAD_PRIORITY;
#if (CUSTOM_TX_THREAD_ATT)
    if (THREAD_ATT(self))
    {
        stack = ASY_SEND_THREAD_ATT(self).stack_depth;
        prio  = ASY_SEND_THREAD_ATT(self).thread_priority;
    }
#endif
    OS_INTERFACE(self)->pf_os_thread_create("asy_send_thread",
                                            asy_send_thread, stack, prio, NULL, self);

#if (IS_ENABLE_CPL_THREAD)
    /* TX completion thread */
    stack = TX_CPL_THREAD_STACK_DEPTH;
    prio  = TX_CPL_THREAD_PRIORITY;
#if (CUSTOM_TX_THREAD_ATT)
    if (THREAD_ATT(self))
    {
        stack = TX_CPL_THREAD_ATT(self).stack_depth;
        prio  = TX_CPL_THREAD_ATT(self).thread_priority;
    }
#endif
    OS_INTERFACE(self)->pf_os_thread_create("tx_cpl_thread",
                                            tx_cpl_thread, stack, prio, NULL, self);
    uint8_t cpl_cache_num = MAX_NUM_CHACHE_TX_CPL_THREAD;
#if (CUSTOM_UART_TX_CFG)
    if (self->uart_tx_input_arg->uart_tx_cfg)
        cpl_cache_num = self->uart_tx_input_arg->uart_tx_cfg->max_num_chache_tx_cpl_thread;
#endif
    OS_INTERFACE(self)->pf_os_queue_create(cpl_cache_num, sizeof(uart_tx_cpl_cb_ctx_t),
                                           &PRIV_DATA(self)->thread_cpl_ctx_queue_handle);
#endif
    
    OS_INTERFACE(self)->pf_os_sema_create(&PRIV_DATA(self)->asy_tx_sema);
    OS_INTERFACE(self)->pf_os_sema_acquire(PRIV_DATA(self)->asy_tx_sema, 0);

    OS_INTERFACE(self)->pf_os_queue_create(1, sizeof(uart_tx_cpl_ctx_t),
                                           &PRIV_DATA(self)->tx_cpl_ctx_queue_handle);

    UART_TX_CREATE_TIMEOUT_TIMER(self);

    /* Resource initialization */
    TX_RESOURCE_CREATE(self);    

    self->pf_send_syn = send_syn;
    self->pf_send_asy = send_asy;
    PRIV_DATA(self)->is_inited = true;
    US_DEBUG_OUT("uart send instance init success!\r\n");

    return UART_TX_OK;
}

/* -------------------------------------------------------------------------- */
/*                          ISR Completion Callback                            */
/* -------------------------------------------------------------------------- */

/**
 * TX completion ISR callback
 */
void tx_cpl_isr_cb(uart_tx_handler_t *const self)
{
    if (!self || !PRIV_DATA(self)->is_inited)
        return;

    if(!PRIV_DATA(self)->is_sending)    
    {
        US_DEBUG_ERR("send complete without send!\r\n");
        return;
    }
    uart_tx_cpl_ctx_t ctx;
    if (OS_INTERFACE(self)->pf_os_queue_get(PRIV_DATA(self)->tx_cpl_ctx_queue_handle,
                                            &ctx, 0) == 0)
    {
        if (ctx.isr_ctx.pf_tx_cpl_cb)
            ctx.isr_ctx.pf_tx_cpl_cb(ctx.isr_ctx.arg);
#if (IS_ENABLE_CPL_THREAD)
        if (ctx.thread_ctx.pf_tx_cpl_cb)
            OS_INTERFACE(self)->pf_os_queue_put(PRIV_DATA(self)->thread_cpl_ctx_queue_handle,
                                                &ctx.thread_ctx, 0);
#endif
    }

    PRIV_DATA(self)->is_sending = false;
    UART_TX_STOP_TIMEOUT_TIMER(self);
    TX_RESOURCE_RELEASE_FIRST_HALF(self);
}

/* -------------------------------------------------------------------------- */
/*                           Reset TX State Function                           */
/* -------------------------------------------------------------------------- */

/**
 * Reset TX state (release TX resource semaphore)
 */
void reset_tx_state(uart_tx_handler_t *const self)
{
    if (!self || !PRIV_DATA(self)->is_inited)
        return;

    PRIV_DATA(self)->is_sending = false;
    UART_TX_STOP_TIMEOUT_TIMER(self);
    TX_RESOURCE_RELEASE_FIRST_HALF(self);
}

__weak void priority_inversion_test_func(void)
{

}
