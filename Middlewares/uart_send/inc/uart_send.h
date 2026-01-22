/**
 * @file uart_send.h
 * @brief UART Send Layer Header
 * @version 1.00
 * @date 2025-12-30
 * @author
 *   Donzel
 * This header defines data structures, configuration macros, and APIs
 * for the UART transmission layer. It provides hardware? and OS?independent
 * interfaces for synchronous and asynchronous UART sending, completion
 * callbacks, and optional background threads.
 *
 * @par Version History
 * - V1.00 (2025-12-30): Initial version - basis
 */

#ifndef __UART_SEND_H__
#define __UART_SEND_H__

#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                          Configuration Macros                              */
/* -------------------------------------------------------------------------- */

/** Enable TX completion thread (1 = enable) */
#define IS_ENABLE_CPL_THREAD                1

/** Enable custom thread attributes (runtime override) */
#define CUSTOM_TX_THREAD_ATT                1

#if (IS_ENABLE_CPL_THREAD)
    /** Enable custom UART TX configuration */
    #define CUSTOM_UART_TX_CFG              0
#endif

/** Timeout tick for TX resource acquisition (0 = no timeout)
 *  Prevents deadlock when TX completion interrupt is lost.
 */
#define UART_TX_RESOURCE_TIMEOUT_TICK       1000

/** Asynchronous TX modes
 *  - UART_ASYNC_SEND_MODE_THREAD_ONLY:  Always send via async thread (data copied to buffer)
 *  - UART_ASYNC_SEND_MODE_DIRECT_FIRST: Try to send directly in caller thread if resource is free
 */
#define UART_ASYNC_SEND_MODE_THREAD_ONLY    (0)
#define UART_ASYNC_SEND_MODE_DIRECT_FIRST   (1)

/** Select active async send mode */
#define UART_ASYNC_SEND_MODE_DEFAULT        UART_ASYNC_SEND_MODE_THREAD_ONLY

#if (UART_ASYNC_SEND_MODE_DEFAULT != UART_ASYNC_SEND_MODE_THREAD_ONLY && \
     UART_ASYNC_SEND_MODE_DEFAULT != UART_ASYNC_SEND_MODE_DIRECT_FIRST)
#error "Invalid UART_ASYNC_SEND_MODE_DEFAULT value."
#endif

/* -------------------------------------------------------------------------- */
/*                        Resource Synchronization Mode                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Resource synchronization method
 *
 * Defines how TX resource access is protected between threads.
 * - RESOURCE_SYN_MODE_SEMA : Use semaphore for synchronization
 * - RESOURCE_SYN_MODE_MUTEX: Use mutex for synchronization (solves priority inversion)
 *
 * @note In RESOURCE_SYN_MODE_MUTEX, UART_ASYNC_SEND_MODE_DIRECT_FIRST is ineffective,
 *       because caller thread may block when acquiring resource.
 */
#define RESOURCE_SYN_MODE_SEMA              (0)
#define RESOURCE_SYN_MODE_MUTEX             (1)

/**
 * @brief Select the active synchronization mode
 */
#define RESOURCE_SYN_MODE_DEFAULT           RESOURCE_SYN_MODE_MUTEX

/**
 * @brief Compile?time validation of selected sync mode
 */
#if (RESOURCE_SYN_MODE_DEFAULT != RESOURCE_SYN_MODE_SEMA && \
     RESOURCE_SYN_MODE_DEFAULT != RESOURCE_SYN_MODE_MUTEX)
#error "Invalid RESOURCE_SYN_MODE_DEFAULT value. Must be SEMAPHORE or MUTEX."
#endif

#if (RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX && \
     UART_ASYNC_SEND_MODE_DEFAULT == UART_ASYNC_SEND_MODE_DIRECT_FIRST)
#warning "UART_ASYNC_SEND_MODE_DIRECT_FIRST is useless in RESOURCE_SYN_MODE_MUTEX: the thread has to exit immediately, cannot waiting the mutex release."
#endif

/* Default thread configuration */
#if (IS_ENABLE_CPL_THREAD)
#   define TX_CPL_THREAD_PRIORITY          14
#   define TX_CPL_THREAD_STACK_DEPTH       0x200
#   define MAX_NUM_CHACHE_TX_CPL_THREAD    10
#endif

#define TX_ASY_THREAD_PRIORITY             15
#define TX_ASY_THREAD_STACK_DEPTH          0x200
#define TX_OS_DELAY_MAX                    0xFFFFFFFFU

#define PRIORITY_INVERSION_TEST_FUNC()     priority_inversion_test_func()       

/* -------------------------------------------------------------------------- */
/*                                Debug Macros                                */
/* -------------------------------------------------------------------------- */
#include "Debug.h"
#ifdef DEBUG_UART_SEND
#define US_DEBUG_OUT(fmt, ...)      DEBUG_OUT(fmt, ##__VA_ARGS__)
#define US_DEBUG_ERR(fmt, ...)      DEBUG_OUT_ERR(fmt, ##__VA_ARGS__)
#else
#define US_DEBUG_OUT(fmt, ...)    
#define US_DEBUG_ERR(fmt, ...)
#endif


/* -------------------------------------------------------------------------- */
/*                               Status Codes                                 */
/* -------------------------------------------------------------------------- */

/** Status returned by UART TX operations */
typedef enum
{
    UART_TX_OK = 0,
    UART_TX_ERR_PARAM_INVALID,
    UART_TX_ERR_ALREADY_INITED,
    UART_TX_ERR_HANDLER_NOT_READY,
    UART_TX_ERR_BUFFER_NOT_SUFFICIENT,
    UART_TX_ERR_OTHERS
} uart_tx_status_t;

/* -------------------------------------------------------------------------- */
/*                      Thread Attribute Configuration                        */
/* -------------------------------------------------------------------------- */
#if (CUSTOM_TX_THREAD_ATT)

/** Thread attributes (stack size + priority) */
typedef struct
{
    uint32_t stack_depth;
    uint32_t thread_priority;
} thread_attr_t;

/** TX thread attributes (async + completion) */
typedef struct
{
#if (IS_ENABLE_CPL_THREAD)
    thread_attr_t tx_cpl_thread_att;
#endif
    thread_attr_t tx_asy_thread_att;
} tx_thread_attr_t;
#endif

/* -------------------------------------------------------------------------- */
/*                      Optional UART TX Configuration                        */
/* -------------------------------------------------------------------------- */
#if (CUSTOM_UART_TX_CFG && IS_ENABLE_CPL_THREAD)

/** UART TX configuration (completion thread cache size) */
typedef struct
{
    uint8_t max_num_chache_tx_cpl_thread;
} uart_tx_cfg_t;
#endif

/* -------------------------------------------------------------------------- */
/*                          TX Completion Context                             */
/* -------------------------------------------------------------------------- */

/** TX completion callback context (user data + callback)
 *
 * @note For synchronous send (pf_send_syn):
 *       - In RESOURCE_SYN_MODE_SEMA: Ensure data stability during transmission.
 *         Update data in completion callback. If using thread callback, ensure its priority > caller thread.
 *         If using ISR callback, follow ISR "quick in/out" principle.
 *       - In RESOURCE_SYN_MODE_MUTEX: No data stability requirement (caller thread blocks until resource released).
 */
typedef struct
{
    void *arg;
    void (*pf_tx_cpl_cb)(void *arg);
} uart_tx_cpl_cb_ctx_t;

/* -------------------------------------------------------------------------- */
/*                           UART HW Interface                                */
/* -------------------------------------------------------------------------- */

/** UART hardware operations (init, deinit, DMA write) */
typedef struct
{
    void (*pf_uart_init)(void);
    void (*pf_uart_deinit)(void);
    void (*pf_uart_write_dma)(uint8_t *const p_data, uint16_t len);
} tx_uart_ops_t;

/* -------------------------------------------------------------------------- */
/*                           Buffer Attributes                                */
/* -------------------------------------------------------------------------- */

/** TX buffer attributes (buffer pointer + size) */
typedef struct
{
    uint8_t  *send_buf;
    uint16_t  buffer_size;
} send_buf_att_t;

/* -------------------------------------------------------------------------- */
/*                            OS Abstraction Layer                            */
/* -------------------------------------------------------------------------- */

/** OS interface for threading, synchronization, and queues */
typedef struct
{
    int32_t (*pf_os_thread_create)(const char *task_name,
                                   void (*task_code)(void*),
                                   size_t stack_size,
                                   uint32_t priority,
                                   void **task_handle,
                                   void *argument);
    void    (*pf_os_thread_delete)(void *const thread_handle);

    int32_t (*pf_os_sema_create)(void **p_sema_handle);
    void    (*pf_os_sema_delete)(void *sema_handle);
    int32_t (*pf_os_sema_release)(void *sema_handle);
    int32_t (*pf_os_sema_acquire)(void *sema_handle, uint32_t timeout);

    int32_t (*pf_os_queue_create)(size_t item_num, size_t item_size, void **p_queue_handle);
    int32_t (*pf_os_queue_put)(void *queue_handle, const void *item, uint32_t timeout);
    int32_t (*pf_os_queue_get)(void *queue_handle, const void *item, uint32_t timeout);

    uint32_t (*pf_os_enter_critical)(void);
    void     (*pf_os_exit_critical)(uint32_t primask);

#if (UART_TX_RESOURCE_TIMEOUT_TICK)
    int32_t (*pf_timer_create)(void **p_timer_handle, const char *timer_name,
                               uint32_t timer_period, uint8_t auto_reload,
                               void (*timer_cb)(void *timer_handle, void *arg), void *arg);
    int32_t (*pf_timer_start)(void *timer_handle, uint32_t ticks_to_wait);
    int32_t (*pf_timer_stop)(void *timer_handle, uint32_t ticks_to_wait);
#endif
#if(RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX)
    int32_t (*pf_os_mutex_create)(void **p_sema_handle);
    void    (*pf_os_mutex_delete)(void *sema_handle);
    int32_t (*pf_os_mutex_release)(void *sema_handle);
    int32_t (*pf_os_mutex_acquire)(void *sema_handle, uint32_t timeout);    
#endif
} uart_tx_os_interface_t;

/* -------------------------------------------------------------------------- */
/*                       Initialization Input Structure                       */
/* -------------------------------------------------------------------------- */

/** UART TX initialization arguments */
typedef struct
{
    send_buf_att_t          *send_buf_att;
    tx_uart_ops_t           *tx_uart_ops;
    uart_tx_os_interface_t  *os_interface;
#if (CUSTOM_TX_THREAD_ATT)
    tx_thread_attr_t        *thread_att;
#endif
#if (CUSTOM_UART_TX_CFG && IS_ENABLE_CPL_THREAD)
    uart_tx_cfg_t           *uart_tx_cfg;
#endif
} uart_tx_input_arg_t;

/* -------------------------------------------------------------------------- */
/*                         TX Completion Contextes                            */
/* -------------------------------------------------------------------------- */

/** TX completion contexts (ISR + thread) */
typedef struct
{
    uart_tx_cpl_cb_ctx_t isr_ctx;
#if (IS_ENABLE_CPL_THREAD)
    uart_tx_cpl_cb_ctx_t thread_ctx;
#endif
} uart_tx_cpl_ctx_t;

/* -------------------------------------------------------------------------- */
/*                              TX Instance Handle                            */
/* -------------------------------------------------------------------------- */

/** Forward declaration of private data */
typedef struct uart_tx_priv_data uart_tx_priv_data_t;

/** UART TX handler (public API + private data) */
typedef struct uart_tx
{
    uart_tx_input_arg_t  *uart_tx_input_arg;
    uart_tx_priv_data_t  *uart_tx_priv_data;

    /** Synchronous send with completion callback
     *  @note Data stability depends on resource sync mode (see uart_tx_cpl_cb_ctx_t note).
     */
    uart_tx_status_t (*pf_send_syn)(struct uart_tx *const self,
                                    uint8_t *const data,
                                    uint16_t length,
                                    uart_tx_cpl_ctx_t *uart_tx_cpl_ctx);

    /** Asynchronous send (no completion callback)
     *  @note Data stability:
     *        - UART_ASYNC_SEND_MODE_DIRECT_FIRST: Ensure data stability during transmission.
     *        - UART_ASYNC_SEND_MODE_THREAD_ONLY: No requirement (data copied to buffer).
     */
    uart_tx_status_t (*pf_send_asy)(struct uart_tx *const self,
                                    uint8_t *const data,
                                    uint16_t length);
} uart_tx_handler_t;

/* -------------------------------------------------------------------------- */
/*                                 APIs                                       */
/* -------------------------------------------------------------------------- */

/** Initialize TX handler */
uart_tx_status_t uart_tx_inst(uart_tx_handler_t *const self,
                              uart_tx_input_arg_t *const p_input_args);

/** TX completion ISR callback */
void tx_cpl_isr_cb(uart_tx_handler_t *const self);

/** Reset TX state (e.g., after error) */
void reset_tx_state(uart_tx_handler_t *const self);

void priority_inversion_test_func(void);


#endif /* __UART_SEND_H__ */
