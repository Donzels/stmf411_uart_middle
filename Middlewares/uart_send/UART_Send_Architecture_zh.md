# UART 发送层架构图

## 1. 概述

UART 发送层是一个高可靠性的串口发送管理框架，提供同步和异步两种发送模式，支持多种资源同步策略，并具备完善的错误恢复机制。

**核心特性**：
- **同步发送**：阻塞式发送，支持发送完成回调
- **异步发送**：非阻塞式发送，数据缓冲后立即返回
- **资源同步**：支持信号量和互斥量两种模式
- **超时保护**：可配置的超时机制防止死锁
- **优先级反转**：互斥量模式解决优先级反转问题

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      UART 发送层实例                              │
│                   (uart_tx_handler_t)                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐    │
│  │   输入参数    │  │   私有数据    │  │   公共API函数      │    │
│  │  input_arg   │  │  priv_data   │  │pf_send_syn()      │    │
│  └──────────────┘  └──────────────┘  │pf_send_asy()      │    │
│                                      └───────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │ OS接口层  │   │ UART接口 │   │发送缓冲区 │
        └──────────┘   └──────────┘   └──────────┘
```

## 3. 核心组件

### 3.1 输入参数 (uart_tx_input_arg_t)

```
uart_tx_input_arg_t
├── send_buf_att            // 发送缓冲区配置
│   ├── send_buf            // 缓冲区指针
│   └── buffer_size         // 缓冲区大小
├── tx_uart_ops             // 硬件操作接口
│   ├── pf_uart_init()
│   ├── pf_uart_deinit()
│   └── pf_uart_write_dma()
├── os_interface            // 操作系统接口
│   ├── pf_os_thread_create()
│   ├── pf_os_sema_create()
│   ├── pf_os_mutex_create()  (互斥量模式)
│   ├── pf_os_queue_create()
│   ├── pf_os_enter_critical()
│   ├── pf_os_exit_critical()
│   └── pf_timer_create()     (超时模式)
├── thread_att              // 线程属性（可选）
│   ├── tx_asy_thread_att   // 异步发送线程
│   └── tx_cpl_thread_att   // 完成回调线程
└── uart_tx_cfg             // 发送配置（可选）
```

### 3.2 私有数据 (uart_tx_priv_data_t)

```
uart_tx_priv_data_t
├── is_inited                       // 初始化标志
├── tx_resource_sema_handle         // TX资源信号量（信号量模式）
├── tx_resource_mutex_handle        // TX资源互斥量（互斥量模式）
├── tx_resource_syn_sema_handle     // 同步信号量（互斥量模式）
├── tx_cpl_ctx_queue_handle         // 完成回调队列
├── asy_tx_sema                     // 异步发送信号量
├── thread_cpl_ctx_queue_handle     // 线程回调队列
├── write_offset                    // 写入偏移量
├── timer                           // 超时定时器（可选）
└── is_sending                      // 发送中标志
```

## 4. 工作模式

### 4.1 同步发送模式

```
调用 pf_send_syn() → 获取TX资源 → 启动DMA传输 → 等待完成 → 释放资源 → 返回
                                        ↓
                                   DMA完成中断
                                        ↓
                                   tx_cpl_isr_cb()
                                        ↓
                              ┌─────────┴─────────┐
                              ▼                   ▼
                         ISR回调              线程回调
                        (中断上下文)         (入队到线程)
```

**特点**：
- 阻塞式调用，直到发送完成
- 支持发送完成回调（ISR上下文或线程上下文）
- 适合需要确认发送完成的场景

**数据稳定性要求**：
- **信号量模式**：数据必须在发送完成前保持稳定，不能在回调中修改
- **互斥量模式**：无要求，调用线程会阻塞直到发送完成

### 4.2 异步发送模式

```
调用 pf_send_asy() → 数据拷贝到缓冲区 → 立即返回
                            ↓
                  ┌─────────┴─────────┐
                  ▼                   ▼
            THREAD_ONLY         DIRECT_FIRST
            总是通过线程发送      优先直接发送
                  │                   │
                  ▼                   ▼
          asy_send_thread()     资源空闲？──Yes→ 直接DMA发送
                                      │
                                     No
                                      ▼
                              通过线程发送
```

**异步模式选择**：

#### 模式 1: UART_ASYNC_SEND_MODE_THREAD_ONLY
- 数据总是先拷贝到缓冲区
- 由异步发送线程统一处理
- **优点**：数据无稳定性要求，调用后可立即修改
- **缺点**：多一次线程切换延迟

#### 模式 2: UART_ASYNC_SEND_MODE_DIRECT_FIRST
- 如果TX资源空闲，直接在调用线程中发送
- 如果资源忙，退回到模式1
- **优点**：减少线程切换，降低延迟
- **缺点**：数据必须在发送完成前保持稳定
- **注意**：互斥量模式下不建议使用（会导致调用线程阻塞）

### 4.3 资源同步模式

#### 信号量模式 (RESOURCE_SYN_MODE_SEMA)

```
┌─────────────┐
│ TX资源信号量 │  初始值: 1
└─────────────┘
      │
      ├──→ 获取成功 → 发送数据 → 发送完成 → 释放信号量
      │
      └──→ 获取失败 → 阻塞等待
```

**特点**：
- 简单高效
- 可能存在优先级反转问题
- 适合简单应用场景

#### 互斥量模式 (RESOURCE_SYN_MODE_MUTEX)

```
┌─────────────┐     ┌──────────────┐
│ TX资源互斥量 │     │ 同步辅助信号量│
└─────────────┘     └──────────────┘
      │                     │
      ├──→ 获取互斥量        │
      │         │           │
      │         └──→ 释放辅助信号量 → 等待辅助信号量 → 释放互斥量
      │
      └──→ 优先级继承机制自动解决优先级反转
```

**特点**：
- 支持优先级继承
- 解决优先级反转问题
- 需要额外的同步信号量配合
- 适合复杂多优先级系统

## 5. 数据流图

### 5.1 同步发送流程

```
┌────────────┐
│ 调用线程    │
└────────────┘
      │
      │ pf_send_syn(data, len, cpl_ctx)
      ▼
┏━━━━━━━━━━━━━━┓
┃ 获取TX资源    ┃ ?─── 信号量/互斥量
┗━━━━━━━━━━━━━━┛
      │
      ▼
┌─────────────────┐
│ 检查发送状态     │
│ check_tx_busy() │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ 入队完成回调上下文│ ?─── tx_cpl_ctx_queue
└─────────────────┘
      │
      ▼
╔═════════════════╗
║ 启动DMA传输      ║
║ pf_uart_write_dma()║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ 启动超时定时器   │ (可选)
└─────────────────┘
      │
      ├──── 信号量模式 ──→ 释放同步信号量的后半部分（无操作）
      │
      └──── 互斥量模式 ──→ 等待同步信号量后释放互斥量
      │
      ▼
┌────────────┐
│ 返回调用者  │
└────────────┘

      ... 等待DMA完成 ...

╔═════════════════╗
║  DMA完成中断     ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ tx_cpl_isr_cb() │
└─────────────────┘
      │
      ├──→ 停止超时定时器
      │
      ├──→ 从队列取出完成上下文
      │
      ├──→ 执行ISR回调 (如果已注册)
      │
      ├──→ 将线程回调入队 (如果已注册)
      │
      └──→ 释放TX资源信号量
```

### 5.2 异步发送流程

```
┌────────────┐
│ 调用线程    │
└────────────┘
      │
      │ pf_send_asy(data, len)
      ▼
┌─────────────────┐
│ 检查缓冲区空间  │
└─────────────────┘
      │
      ▼
#if DIRECT_FIRST && !MUTEX
┌─────────────────┐
│ 尝试直接获取资源│ (非阻塞)
└─────────────────┘
      │
      ├──→ 成功 → 直接DMA发送 → 返回
      │
      └──→ 失败 ↓
#endif
      ▼
┌─────────────────┐
│ 拷贝数据到缓冲区│
│ write_offset += len│
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ 释放异步发送信号量│
└─────────────────┘
      │
      ▼
┌────────────┐
│ 立即返回    │
└────────────┘

================================================

┌─────────────────┐
│ asy_send_thread │ ?─── 后台线程
└─────────────────┘
      │
      │ 阻塞等待异步发送信号量
      ▼
┌─────────────────┐
│ 获取TX资源       │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ 从缓冲区读取数据│
│ read_offset     │
└─────────────────┘
      │
      ▼
╔═════════════════╗
║ 启动DMA传输      ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ 等待发送完成     │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ 循环等待下次发送│
└─────────────────┘
```

### 5.3 完成回调流程

```
╔═════════════════╗
║  DMA完成中断     ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ 从队列获取上下文 │
└─────────────────┘
      │
      ├──────────────────┐
      │                  │
      ▼                  ▼
┌──────────┐      ┌─────────────┐
│ ISR回调   │      │ 线程回调     │
│ 立即执行  │      │ 入队列      │
└──────────┘      └─────────────┘
                        │
                        ▼
                  ┌─────────────┐
                  │tx_cpl_thread│ ?─── 后台线程
                  └─────────────┘
                        │
                        │ 从队列取出
                        ▼
                  ┌─────────────┐
                  │ 执行用户回调 │
                  └─────────────┘
```

## 6. 线程模型

```
┌─────────────────────┐
│  异步发送线程        │  优先级: TX_ASY_THREAD_PRIORITY (15)
│ asy_send_thread()   │  栈深度: TX_ASY_THREAD_STACK_DEPTH (0x200)
└─────────────────────┘
        │
        │ 等待 asy_tx_sema
        ▼
    ┌────────┐
    │ 获取资源│
    └────────┘
        │
        ▼
    ┌────────┐
    │DMA发送 │
    └────────┘

┌─────────────────────┐
│  完成回调线程        │  优先级: TX_CPL_THREAD_PRIORITY (14)
│ tx_cpl_thread()     │  栈深度: TX_CPL_THREAD_STACK_DEPTH (0x200)
└─────────────────────┘
        │
        │ 等待 thread_cpl_ctx_queue
        ▼
    ┌────────┐
    │执行回调│
    └────────┘
```

**线程优先级建议**：
- **异步发送线程** > **完成回调线程** > **应用业务线程**
- 防止异步发送被阻塞
- 保证发送实时性

## 7. API 接口

### 7.1 初始化接口

```c
uart_tx_status_t uart_tx_inst(
    uart_tx_handler_t *const self,
    uart_tx_input_arg_t *const args
);
```

**功能**：初始化发送层实例
**步骤**：
1. 参数验证
2. 分配私有数据
3. 创建异步发送线程
4. 创建完成回调线程（可选）
5. 创建同步资源（信号量/互斥量）
6. 创建超时定时器（可选）
7. 注册API函数指针

### 7.2 同步发送接口

```c
uart_tx_status_t pf_send_syn(
    uart_tx_handler_t *const self,
    uint8_t *const data,
    uint16_t length,
    uart_tx_cpl_ctx_t *uart_tx_cpl_ctx
);
```

**参数**：
- `data`: 待发送数据指针
- `length`: 数据长度
- `uart_tx_cpl_ctx`: 完成回调上下文（可为 NULL）

**返回值**：
- `UART_TX_OK`: 发送成功
- `UART_TX_ERR_PARAM_INVALID`: 参数无效
- `UART_TX_ERR_HANDLER_NOT_READY`: 未初始化
- `UART_TX_ERR_OTHERS`: 其他错误

**数据稳定性**：
- **信号量模式**：数据必须保持稳定直到发送完成
- **互斥量模式**：无要求（函数内部会阻塞）

### 7.3 异步发送接口

```c
uart_tx_status_t pf_send_asy(
    uart_tx_handler_t *const self,
    uint8_t *const data,
    uint16_t length
);
```

**参数**：
- `data`: 待发送数据指针
- `length`: 数据长度

**返回值**：
- `UART_TX_OK`: 数据已入队
- `UART_TX_ERR_BUFFER_NOT_SUFFICIENT`: 缓冲区空间不足

**数据稳定性**：
- **THREAD_ONLY 模式**：无要求（数据已拷贝）
- **DIRECT_FIRST 模式**：数据必须保持稳定（可能直接发送）

### 7.4 状态重置接口

```c
void reset_tx_state(uart_tx_handler_t *const self);
```

**功能**：重置发送状态
**使用场景**：
- 超时恢复
- 错误处理
- 手动清除发送标志

### 7.5 完成回调接口

```c
void tx_cpl_isr_cb(uart_tx_handler_t *const self);
```

**功能**：DMA发送完成中断回调
**调用位置**：UART DMA传输完成中断服务程序

## 8. 错误处理

### 8.1 超时保护机制

```c
#define UART_TX_RESOURCE_TIMEOUT_TICK  1000  // 超时时间(ms)
```

**工作原理**：
1. 启动DMA发送时启动定时器
2. 正常完成时停止定时器
3. 超时触发时调用 `timer_cb()` 重置状态

**防止的问题**：
- DMA完成中断丢失
- 硬件故障导致发送卡死
- 软件异常导致信号量未释放

### 8.2 发送繁忙检测

```c
static inline bool check_tx_busy(uart_tx_handler_t *const self)
```

**功能**：检测并修复异常的发送状态
**场景**：
- `is_sending` 标志与资源状态不一致
- 中断处理异常
- 多线程竞争条件

### 8.3 缓冲区溢出保护

```c
if (remain < length) {
    OS_INTERFACE(self)->pf_os_exit_critical(0);
    US_DEBUG_ERR("Buffer not sufficient!\r\n");
    return UART_TX_ERR_BUFFER_NOT_SUFFICIENT;
}
```

**保护机制**：
- 异步发送前检查剩余空间
- 关键区内原子性检查和更新
- 防止覆盖未发送数据

## 9. 配置选项

### 9.1 编译期配置

```c
// 完成回调线程开关
#define IS_ENABLE_CPL_THREAD              1

// 异步发送模式
#define UART_ASYNC_SEND_MODE_DEFAULT      UART_ASYNC_SEND_MODE_THREAD_ONLY

// 资源同步模式
#define RESOURCE_SYN_MODE_DEFAULT         RESOURCE_SYN_MODE_MUTEX

// 超时保护（0 = 禁用）
#define UART_TX_RESOURCE_TIMEOUT_TICK     1000

// 线程配置
#define TX_ASY_THREAD_PRIORITY            15
#define TX_ASY_THREAD_STACK_DEPTH         0x200
#define TX_CPL_THREAD_PRIORITY            14
#define TX_CPL_THREAD_STACK_DEPTH         0x200

// 完成回调队列深度
#define MAX_NUM_CHACHE_TX_CPL_THREAD      10

// 可选特性
#define CUSTOM_TX_THREAD_ATT              1  // 自定义线程属性
#define CUSTOM_UART_TX_CFG                0  // 自定义配置
```

### 9.2 运行期配置

#### 线程属性配置
```c
tx_thread_attr_t thread_att = {
    .tx_asy_thread_att = {
        .stack_depth = 0x300,
        .thread_priority = 12
    },
    .tx_cpl_thread_att = {
        .stack_depth = 0x200,
        .thread_priority = 11
    }
};
```

#### UART TX 配置
```c
uart_tx_cfg_t cfg = {
    .max_num_chache_tx_cpl_thread = 20  // 增加回调队列深度
};
```

## 10. 典型使用场景

### 10.1 实时数据传输（传感器数据上报）

**推荐配置**：
- 异步发送模式：`UART_ASYNC_SEND_MODE_THREAD_ONLY`
- 资源同步模式：`RESOURCE_SYN_MODE_MUTEX`
- 完成回调：不需要

```c
// 周期性发送传感器数据
void sensor_task(void *arg) {
    uint8_t data[64];
    while (1) {
        read_sensor_data(data);
        tx_handler->pf_send_asy(tx_handler, data, 64);
        os_delay(100);  // 100ms周期
    }
}
```

### 10.2 命令响应（需要确认）

**推荐配置**：
- 同步发送模式
- 资源同步模式：`RESOURCE_SYN_MODE_MUTEX`
- 完成回调：线程回调

```c
// 发送命令响应并确认
void send_response(uint8_t *resp, uint16_t len) {
    uart_tx_cpl_ctx_t cpl_ctx = {
        .thread_ctx = {
            .arg = &my_context,
            .pf_tx_cpl_cb = response_sent_callback
        }
    };
    tx_handler->pf_send_syn(tx_handler, resp, len, &cpl_ctx);
}

void response_sent_callback(void *arg) {
    // 响应已发送，更新状态
    update_state(STATE_RESPONSE_SENT);
}
```

### 10.3 日志输出（高频）

**推荐配置**：
- 异步发送模式：`UART_ASYNC_SEND_MODE_DIRECT_FIRST`
- 资源同步模式：`RESOURCE_SYN_MODE_SEMA`（日志优先级通常一致）
- 完成回调：不需要

```c
// 高频日志输出
void log_output(const char *msg) {
    tx_handler->pf_send_asy(tx_handler, 
                            (uint8_t*)msg, 
                            strlen(msg));
}
```

## 11. 优先级反转问题

### 11.1 问题描述

```
高优先级任务 (H) ──┐
                  │ 等待TX资源
                  ▼
中优先级任务 (M) ────→ 抢占 ────→ 低优先级任务执行被延迟
                              │
低优先级任务 (L) ───→ 持有TX资源 ──┘

结果：高优先级任务被中优先级任务间接阻塞
```

### 11.2 解决方案

#### 方案 1: 使用互斥量（推荐）

```c
#define RESOURCE_SYN_MODE_DEFAULT  RESOURCE_SYN_MODE_MUTEX
```

**原理**：
- 互斥量支持优先级继承
- 低优先级任务持有资源时，临时提升至高优先级
- 避免被中优先级任务抢占

#### 方案 2: 调整优先级设计

```
异步发送线程优先级 > 所有应用线程
```

**原理**：
- 异步发送线程快速释放资源
- 减少资源持有时间
- 降低优先级反转风险

#### 方案 3: 测试点注入

```c
#define PRIORITY_INVERSION_TEST_FUNC()  priority_inversion_test_func()

void priority_inversion_test_func(void) {
    // 注入测试代码，验证优先级反转问题
}
```

## 12. 性能优化

### 12.1 减少线程切换

**优化策略**：
- 使用 `UART_ASYNC_SEND_MODE_DIRECT_FIRST`
- 资源空闲时直接在调用线程中发送
- 避免不必要的线程唤醒

**适用场景**：
- 发送频率低
- 应用线程优先级合理
- 不使用互斥量模式

### 12.2 缓冲区大小设计

**原则**：
```
缓冲区大小 >= 单次最大发送量 × 2
```

**考虑因素**：
- 发送频率
- 单次发送数据量
- 发送耗时
- 系统实时性要求

### 12.3 回调执行优化

**ISR 回调**：
- 尽量简短
- 不要调用阻塞函数
- 不要执行耗时操作

**线程回调**：
- 可以执行复杂逻辑
- 可以调用阻塞函数
- 注意线程优先级配置

## 13. 依赖关系

```
uart_send.c/h
    ├── os_interface          (线程、信号量、互斥量、队列、定时器)
    ├── tx_uart_ops           (硬件初始化、DMA写入)
    └── send_buf_att          (发送缓冲区)
```

## 14. 移植指南

### 14.1 必须实现的接口

1. **OS 接口**：
   - 线程创建/删除
   - 信号量创建/获取/释放
   - 互斥量创建/获取/释放（互斥量模式）
   - 队列创建/收发
   - 临界区进入/退出
   - 定时器创建/启动/停止（超时模式）

2. **UART 接口**：
   - 硬件初始化/反初始化
   - DMA 写入

### 14.2 移植步骤

1. 根据RTOS类型实现 `uart_tx_os_interface_t` 接口
2. 实现 `tx_uart_ops_t` UART硬件接口
3. 配置工作模式（同步模式、异步模式、资源同步模式）
4. 在 DMA 发送完成中断中调用 `tx_cpl_isr_cb()`
5. 初始化发送层实例并注册到系统

### 14.3 FreeRTOS 移植示例

```c
// OS接口实现
uart_tx_os_interface_t os_interface = {
    .pf_os_thread_create = freertos_thread_create,
    .pf_os_sema_create = freertos_sema_create,
    .pf_os_sema_acquire = freertos_sema_acquire,
    .pf_os_sema_release = freertos_sema_release,
    .pf_os_mutex_create = freertos_mutex_create,
    .pf_os_mutex_acquire = freertos_mutex_acquire,
    .pf_os_mutex_release = freertos_mutex_release,
    // ... 其他接口
};

// UART接口实现
tx_uart_ops_t uart_ops = {
    .pf_uart_init = hal_uart_init,
    .pf_uart_deinit = hal_uart_deinit,
    .pf_uart_write_dma = hal_uart_dma_transmit
};
```

## 15. 调试支持

### 15.1 调试宏

```c
#define DEBUG_UART_SEND           // 启用调试输出

#ifdef DEBUG_UART_SEND
#define US_DEBUG_OUT(fmt, ...)    DEBUG_OUT(fmt, ##__VA_ARGS__)
#define US_DEBUG_ERR(fmt, ...)    DEBUG_OUT_ERR(fmt, ##__VA_ARGS__)
#endif
```

### 15.2 常见问题排查

| 问题现象 | 可能原因 | 排查方法 |
|---------|---------|---------|
| 发送卡死 | 中断未触发 | 检查中断配置，启用超时保护 |
| 数据丢失 | 缓冲区溢出 | 增加缓冲区大小，检查发送频率 |
| 优先级反转 | 使用信号量 | 切换到互斥量模式 |
| 发送延迟大 | 线程优先级低 | 调整异步发送线程优先级 |
| 回调未执行 | 队列满 | 增加回调队列深度 |

---

**版本**: 1.00  
**作者**: Donzel  
**日期**: 2025-12-30
