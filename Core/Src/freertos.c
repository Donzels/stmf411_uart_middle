/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "osal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "uart_proto.h"
#include "queue.h"
#include <stdbool.h>
#include <stdio.h>

#define PRINT_BUF_SIZE  512
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
static int8_t my_parse_funcode(uint8_t *const p_data,
                               uint16_t data_len, frame_info_t *const frame_info);

static void funcode_cb(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb07(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb08(void *arg, uint8_t *const payload, uint16_t payload_len);

static void funcode_cb0a(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb0b(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb0c(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb0d(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb0e(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb0f(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb10(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb11(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb12(void *arg, uint8_t *const payload, uint16_t payload_len);
static void funcode_cb13(void *arg, uint8_t *const payload, uint16_t payload_len);
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#include "Debug.h"
#ifdef DEBUG_USER_APP
#define TASK_DEBUG_OUT(fmt, ...)      DEBUG_OUT(fmt, ##__VA_ARGS__)
#define TASK_DEBUG_ERR(fmt, ...)      DEBUG_OUT_ERR(fmt, ##__VA_ARGS__)
#else
#define TASK_DEBUG_OUT(fmt, ...)    
#define TASK_DEBUG_ERR(fmt, ...)
#endif
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* uart proto */
static parse_algo_t parse_algo =
    {.u.funcoude_algo.pf_parse_funcode = my_parse_funcode};

static frame_parse_att_t frame_parse_att =
    {
        .parse_algo = &parse_algo,
        .recv_buf_att = &g_recv_buf};

static uart_rx_os_interface_t os_interface =
    {
        .pf_os_thread_create = osal_task_create,
        .pf_os_thread_delete = osal_task_delete,
        .pf_os_queue_create = osal_queue_create,
        .pf_os_queue_put = osal_queue_send,
        .pf_os_queue_get = osal_queue_receive,
        .pf_os_enter_critical = osal_enter_critical,
        .pf_os_exit_critical = osal_exit_critical,
};

static uart_proto_input_arg_t uart_proto_input_arg =
    {
        .frame_parse_att = &frame_parse_att,
        .os_interface = &os_interface,
        .uart_ops = &g_uart_ops};

uart_proto_t g_uart_proto;
/* uart proto */

/* uart send */
static uart_tx_os_interface_t tx_os_interface =
    {
        .pf_os_thread_create = osal_task_create,
        .pf_os_thread_delete = osal_task_delete,
        .pf_os_queue_create = osal_queue_create,
        .pf_os_queue_put = osal_queue_send,
        .pf_os_queue_get = osal_queue_receive,
        .pf_os_sema_create = osal_sema_binary_create,
        .pf_os_sema_delete = osal_sema_delete,
        .pf_os_sema_acquire = osal_sema_take,
        .pf_os_sema_release = osal_sema_give,
        .pf_os_enter_critical = osal_enter_critical,
        .pf_os_exit_critical = osal_exit_critical,
        .pf_timer_create = osal_timer_create,
        .pf_timer_start = osal_timer_start,
        .pf_timer_stop = osal_timer_stop,
#if(RESOURCE_SYN_MODE_DEFAULT == RESOURCE_SYN_MODE_MUTEX)        
        .pf_os_mutex_create = osal_mutex_create,
        .pf_os_mutex_delete = osal_mutex_delete,
        .pf_os_mutex_acquire = osal_mutex_take,
        .pf_os_mutex_release = osal_mutex_give
#endif
};

static uart_tx_input_arg_t uart_tx_input_arg =
    {
        .send_buf_att = &send_buf_att,
        .tx_uart_ops = &tx_uart_ops,
        .os_interface = &tx_os_interface,
};

static uint8_t printf_buf[PRINT_BUF_SIZE] = {0};

uart_tx_handler_t g_uart_tx_handler;

/* uart send */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
const osThreadAttr_t test_task_att1 = {
    .name = "test_task_handle",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)11,
};

const osThreadAttr_t test_task_att2 = {
    .name = "test_task2_handle",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)13,
};

osThreadId_t test_task3_handle;
const osThreadAttr_t test_task_att3 = {
    .name = "test_task3_handle",
    .stack_size = 128 * 4,
    .priority = (osPriority_t)10,
};

static void test_task1(void *arg);
static void test_task2(void *arg);
static void test_task3(void *arg);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* creation of defaultTask */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    osThreadNew(test_task1, NULL, &test_task_att1);
    osThreadNew(test_task2, NULL, &test_task_att2);
    test_task3_handle = osThreadNew(test_task3, NULL, &test_task_att3);
    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
extern UART_HandleTypeDef huart1;
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
    /* USER CODE BEGIN StartDefaultTask */
    TASK_DEBUG_OUT("default task begin\r\n");

    uart_proto_status_t ret;
    ret = uart_proto_inst(&g_uart_proto, &uart_proto_input_arg);
    if (UART_PROTO_OK != ret)
        TASK_DEBUG_OUT("uart proto inst error");
    else
    {
        subscribe_para_t subscribe_para =
            {
                .arg = NULL,
                .cb = funcode_cb07,
                .fun_code = 0x07};
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb08;
        subscribe_para.fun_code = 0x08;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);

        subscribe_para.cb = funcode_cb0a;
        subscribe_para.fun_code = 0x0A;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb0c;
        subscribe_para.fun_code = 0x0C;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb0b;
        subscribe_para.fun_code = 0x0B;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb0f;
        subscribe_para.fun_code = 0x0F;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb0e;
        subscribe_para.fun_code = 0x0E;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb12;
        subscribe_para.fun_code = 0x12;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb13;
        subscribe_para.fun_code = 0x13;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb10;
        subscribe_para.fun_code = 0x10;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb11;
        subscribe_para.fun_code = 0x11;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
        subscribe_para.cb = funcode_cb0d;
        subscribe_para.fun_code = 0x0D;
        g_uart_proto.pf_subscribe(&g_uart_proto, &subscribe_para, NULL);
    }
    /* uart send */
    uart_tx_status_t sta = uart_tx_inst(&g_uart_tx_handler, &uart_tx_input_arg);
    if (UART_TX_OK != sta)
        TASK_DEBUG_OUT("uart send inst error");
    osThreadExit();
    /* Infinite loop */
    for (;;)
    {
        osDelay(1);
    }
    /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
#define TEST_SEND_BUF_SIZE 128
typedef struct 
{
    uint8_t *i;
    uint8_t *send_buf;
}thread_ctx_t;


static void isr_tx_cpl_cb(void *arg)
{
    uint8_t i= *(uint8_t *)arg;
    TASK_DEBUG_OUT("isr_0x%x\r\n", i);
}

static void thread_tx_cpl_cb(void *arg)
{
    thread_ctx_t *thread_ctx= (thread_ctx_t *)arg;
    uint8_t i = *thread_ctx->i;
    memset(thread_ctx->send_buf, i, TEST_SEND_BUF_SIZE);/* correct: update next send data */
    TASK_DEBUG_OUT("thread_0x%x\r\n", i);    
}

static void test_task1(void *arg)
{
    uint8_t send_buf[TEST_SEND_BUF_SIZE] = {0};
    TASK_DEBUG_OUT("task1 begin\r\n");
    
    while (1)
    {       
        static uint8_t i=0;
        thread_ctx_t thread_ctx = {.i = &i, .send_buf = send_buf};
        // memset(send_buf, i, TEST_SEND_BUF_SIZE);/* error: directly change send data */
        uart_tx_cpl_ctx_t uart_tx_cpl_ctx = 
        {
            .isr_ctx = {.arg = &i, .pf_tx_cpl_cb = isr_tx_cpl_cb},
            .thread_ctx = {.arg = &thread_ctx, .pf_tx_cpl_cb = thread_tx_cpl_cb}
        };
        g_uart_tx_handler.pf_send_syn(&g_uart_tx_handler, send_buf, TEST_SEND_BUF_SIZE, &uart_tx_cpl_ctx);
        i++;
    }   
}

static void test_task2(void *arg)
{
    TASK_DEBUG_OUT("task2 begin\r\n");
    const uint8_t cnt = 17;
    uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xAC, 0xCA, 0x77};
    uint8_t send_buf[sizeof(pattern) * cnt] = {0};
    uint8_t send_buf2[] = {0xAB, 0xCD};
    for (int i = 0; i < cnt; i++) 
    {
        memcpy(send_buf + sizeof(pattern) * i, pattern, sizeof(pattern));
    }
    osDelay(3000);
    while (1)
    {       
        g_uart_tx_handler.pf_send_asy(&g_uart_tx_handler, send_buf, sizeof(send_buf));
        g_uart_tx_handler.pf_send_asy(&g_uart_tx_handler, send_buf2, sizeof(send_buf2));
        osDelay(100);
    }   
}

static void test_task3(void *arg)
{
    TASK_DEBUG_OUT("task3 begin\r\n");
    while (1)
    {       
        /* occupy CPU */
    }   
}

void priority_inversion_test_func(void)
{
    // osThreadSetPriority(test_task3_handle, (osPriority_t)12);/* middle priority */
    // osThreadYield();
}

static void funcode_cb(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    TASK_DEBUG_OUT("pay_len=%d, is:  ", payload_len);
    size_t len = 0;
    for (uint16_t i = 0; i < payload_len; i++) 
    {
        len += snprintf((char*)printf_buf + len, PRINT_BUF_SIZE - len, "%02x ", payload[i]);        
    }
    TASK_DEBUG_OUT("%s", printf_buf);
    TASK_DEBUG_OUT("\r\n");
}

static void funcode_cb07(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    TASK_DEBUG_OUT(" 07 fun, ");
    funcode_cb(arg, payload, payload_len);
}

static void funcode_cb08(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    TASK_DEBUG_OUT(" 08 fun, ");
    funcode_cb(arg, payload, payload_len);
}

static void funcode_cb0a(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0A fun, cntA = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb0b(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0B fun, cntB = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb0c(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0C fun, cntC = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb0d(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0D fun, cntD = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb0e(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0E fun, cntE = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb0f(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 0F fun, cntF = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb10(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 10 fun, cnt10 = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb11(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 11 fun, cnt11 = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb12(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 12 fun, cnt12 = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}
static void funcode_cb13(void *arg, uint8_t *const payload, uint16_t payload_len)
{
    static uint32_t cnt = 0;
    cnt++;
    TASK_DEBUG_OUT(" 13 fun, cnt13 = %d. ", cnt);
    funcode_cb(arg, payload, payload_len);
}

static uint16_t modbus_crc16(const uint8_t *p_data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    uint8_t i, j;
    if (p_data == NULL || len == 0)
    {
        TASK_DEBUG_OUT("crc error");
        return crc;
    }
    for (i = 0; i < len; i++)
    {
        crc ^= p_data[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

#if 1

static uint16_t find_possible_header_tail(const uint8_t *p_data,
                                          uint16_t data_len,
                                          const uint8_t *header,
                                          uint16_t header_len)
{
    /* search for possible partial header remains*/
    uint16_t possible_len = 0;
    if (0 == data_len)
        return 0;

    for (uint16_t i = 1; i < header_len; i++)
    {
        if (0 == memcmp(&p_data[data_len - i], header, i))
        {
            possible_len = i;
        }
    }
    return possible_len;
}

static algo_status_t my_parse_funcode(uint8_t *const p_data,
                                      uint16_t data_len,
                                      frame_info_t *const frame_info)
{
    if (!p_data || !frame_info)
        return ALGO_ERR_OTHERS;

    const uint8_t header[] = {0xCC, 0xAA};
    const uint16_t min_frame_len = sizeof(header) + 1 /*fun_code*/ + 2 /*len*/ + 2 /*CRC*/ + 1 /*minimum payload*/;
    const uint16_t start_2_payload_len = sizeof(header) + 1 /*fun_code*/ + 2 /*len*/;
    const uint16_t payload_2_end_len = 2 /*CRC*/;

    if (data_len < min_frame_len)
        return ALGO_ING;
    /* Search for frame header */
    uint16_t index = 0;
    while (index + sizeof(header) <= data_len)
    {
        if (0 == memcmp(&p_data[index], header, sizeof(header)))
        {
            /* Header found */
            if (data_len - index < min_frame_len)
            {
                /* Not enough data for a full frame; keep the incomplete fragment for next time */
                frame_info->payload_len = 0;
                frame_info->pre_payload_len = index; // Noise before header
                frame_info->post_payload_len = 0;
                if (index)
                    return ALGO_ERR_NOICE;
                return ALGO_ING;
            }

            uint8_t fun_code = p_data[index + sizeof(header)];
            uint16_t payload_len = p_data[index + sizeof(header) + 1] |
                                   (p_data[index + sizeof(header) + 2] << 8); // LSB

            /* Check validity of payload length */
            if (payload_len >= g_recv_buf.buffer_size / 2 || 0 == payload_len)
            {
                /* Invalid length, skip to next possible header */
                TASK_DEBUG_ERR("data length illegal! len=0x%x\r\n", payload_len);
                uint16_t new_index = index + sizeof(header);
                bool found = false;
                while (new_index + sizeof(header) <= data_len)
                {
                    if (0 == memcmp(&p_data[new_index], header, sizeof(header)))
                    {
                        found = true;
                        break;
                    }
                    new_index++;
                }
                /* If a complete frame header is not found, then detect the possible partial frame header at the end. */
                if (found)
                {
                    frame_info->pre_payload_len = new_index;
                }
                else
                {
                    uint16_t possible_header_len = find_possible_header_tail(p_data, data_len, header, sizeof(header));
                    frame_info->pre_payload_len = data_len - possible_header_len;
                }
                frame_info->payload_len = 0;
                frame_info->post_payload_len = 0;
                return ALGO_ERR_LENGTH_INVALID;
            }

            /* Check if there is enough data for CRC */
            uint16_t total_frame_len = start_2_payload_len + payload_len + payload_2_end_len;
            if (data_len - index < total_frame_len)
            {
                /* Data incomplete; wait for the rest of the frame */
                return ALGO_ING;
            }

            /* CRC verification */
            uint16_t crc_recv = *(uint16_t *)&p_data[index + start_2_payload_len + payload_len]; // LSB
            uint16_t crc_calc = modbus_crc16(&p_data[index], start_2_payload_len + payload_len);

            /* Successfully parsed */
            if (crc_recv == crc_calc)
            {
                frame_info->fun_code = fun_code;
                frame_info->payload_len = payload_len;
                frame_info->pre_payload_len = index + start_2_payload_len;
                frame_info->post_payload_len = payload_2_end_len;
                return ALGO_OK;
            }

            /* CRC error, try to find next header */
            TASK_DEBUG_ERR("crc error, len=0x%x\r\n", payload_len);
            uint16_t search_start = index + 1; // Move forward one byte to avoid infinite loop
            uint16_t new_index = search_start;
            bool found = false;
            while (new_index + sizeof(header) <= data_len)
            {
                if (0 == memcmp(&p_data[new_index], header, sizeof(header)))
                {
                    found = true;
                    break;
                }
                new_index++;
            }
            if (found)
            {
                frame_info->pre_payload_len = new_index;
            }
            else
            {
                uint16_t possible_header_len = find_possible_header_tail(p_data, data_len, header, sizeof(header));
                frame_info->pre_payload_len = data_len - possible_header_len;
            }
            frame_info->payload_len = 0;
            frame_info->post_payload_len = 0;
            return ALGO_ERR_CRC;
        }
        index++;
    }

    /* No full header found, possible partial header remains */
    uint16_t possible_header_len = find_possible_header_tail(p_data, data_len, header, sizeof(header));
    frame_info->payload_len = 0;
    frame_info->pre_payload_len = data_len - possible_header_len;
    frame_info->post_payload_len = 0;

    if (frame_info->pre_payload_len)
        return ALGO_ERR_NOICE;

    return ALGO_ING;
}
#endif

/* USER CODE END Application */
