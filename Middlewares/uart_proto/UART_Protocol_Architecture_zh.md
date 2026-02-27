# UART 协议层架构图

## 1. 概述

UART 协议层是一个可配置的异步串口协议处理框架，支持三种工作模式：
- **功能码模式**：基于功能码的帧解析和回调分发
- **透传模式**：原始数据透明传输
- **双策略模式**：运行时动态切换上述两种模式

## 2. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        UART 协议层实例                            │
│                      (uart_proto_t)                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐    │
│  │   输入参数    │  │   私有数据    │  │   公共API函数      │    │
│  │  input_arg   │  │  priv_data   │  │pf_subscribe()     │    │
│  └──────────────┘  └──────────────┘  │pf_unsubscribe()   │    │
│                                      │pf_strategy_algo() │    │
│                                      └───────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │ OS接口层  │   │ UART接口 │   │ 解析算法  │
        └──────────┘   └──────────┘   └──────────┘
```

## 3. 核心组件

### 3.1 输入参数 (uart_proto_input_arg_t)

```
uart_proto_input_arg_t
├── frame_parse_att         // 帧解析属性
│   ├── recv_buf_att        // 接收缓冲区配置
│   │   ├── recv_buf        // 缓冲区指针
│   │   └── buffer_size     // 缓冲区大小
│   └── parse_algo          // 解析算法
│       ├── algo_type       // 算法类型（双模式）
│       └── u               // 算法联合体
│           ├── funcoude_algo     // 功能码算法
│           └── transparent_algo  // 透传算法
├── uart_ops                // 硬件操作接口
│   ├── pf_uart_init()
│   ├── pf_get_counter()
│   └── pf_set_counter()
├── os_interface            // 操作系统接口
│   ├── pf_os_thread_create()
│   ├── pf_os_queue_create()
│   ├── pf_os_enter_critical()
│   └── pf_os_exit_critical()
├── thread_att              // 线程属性（可选）
└── uart_proto_config       // 协议配置（可选）
```

### 3.2 私有数据 (uart_proto_priv_data_t)

```
uart_proto_priv_data_t
├── is_inited                    // 初始化标志
├── num_notify_isr_cb_call       // ISR通知次数
├── parse_fail_cnt               // 解析失败计数
├── queue_handle                 // 队列句柄
├── data_counter                 // 数据计数器
├── header                       // 头部索引
├── tail                         // 尾部索引
├── parse_buf                    // 解析缓冲区
└── funcode_sentinel             // 功能码链表哨兵（功能码模式）
```

## 4. 工作模式

### 4.1 功能码模式 (UART_PROTO_MODE_FUNCTION_CODE)

```
接收数据 → 帧解析 → 提取功能码 → 查找订阅者 → 回调通知
```

**特点**：
- 支持多个订阅者订阅不同功能码
- 使用有序链表管理订阅关系
- 自动多帧解析直到buffer耗尽

**订阅机制**：
```
订阅者 A (功能码 0x01)  ───┐
订阅者 B (功能码 0x03)  ───┼──→ 有序链表
订阅者 C (功能码 0x05)  ───┘
```

### 4.2 透传模式 (UART_PROTO_MODE_TRANSPARENT)

```
接收数据 → 直接入队 → 透传回调
```

**特点**：
- 不解析帧结构
- 原始数据直接传递
- 适用于自定义协议或流式数据

### 4.3 双策略模式 (UART_PROTO_MODE_DUAL_STRATEGY)

```
接收数据 → 检查当前算法类型 → 功能码解析 / 透传处理
                            ↑
                      运行时可切换
```

**特点**：
- 运行时动态切换算法
- 通过 `pf_strategy_algo()` 接口切换
- 同时包含功能码订阅机制和透传接口

## 5. 数据流图

### 5.1 接收流程

```
              ╔═══════════════╗
              ║  UART DMA 中断 ║
              ╚═══════════════╝
                      │
                      ▼
         ┌────────────────────────┐
         │   notify_isr_cb()      │ ?─── 从ISR上下文调用
         │  1. 计算环形缓冲区范围   │
         │  2. 处理回绕情况         │
         │  3. 调用解析函数         │
         └────────────────────────┘
                      │
         ┌────────────┴────────────┐
         │                         │
         ▼                         ▼
  ┏━━━━━━━━━━━━┓          ┏━━━━━━━━━━━━┓
  ┃ 功能码解析  ┃          ┃   透传解析  ┃
  ┗━━━━━━━━━━━━┛          ┗━━━━━━━━━━━━┛
         │                         │
         └────────────┬────────────┘
                      ▼
              ╔═══════════════╗
              ║   消息队列     ║
              ╚═══════════════╝
                      │
                      ▼
         ┌────────────────────────┐
         │   parse_thread()       │ ?─── 后台线程
         │  1. 从队列取数据         │
         │  2. 调用订阅者回调       │
         └────────────────────────┘
                      │
                      ▼
              ┌──────────────┐
              │ 用户回调函数   │
              └──────────────┘
```

### 5.2 环形缓冲区管理

```
DMA 接收缓冲区（环形）:
┌───────────────────────────────────────┐
│  #########-------########             │
│  ↑        ↑       ↑                   │
│  已使用   tail    DMA写入位置          │
└───────────────────────────────────────┘
  
  header: 总接收字节数（累加）
  tail:   已处理字节数（累加）
  data_counter: DMA上次停止位置
```

**回绕处理**：
- 如果数据跨越缓冲区边界，复制到 `parse_buf`
- 如果数据在连续区域，直接处理（`NON_COPY_WHEN_NON_WRAP`）

## 6. 线程模型

```
┌─────────────────┐
│  解析线程        │  优先级: PARSE_THREAD_PRIORITY (16)
│ parse_thread()  │  栈深度: PARSE_THREAD_STACK_DEPTH (0x200)
└─────────────────┘
        │
        │ 阻塞在消息队列
        ▼
    ┌────────┐
    │ 队列等待 │
    └────────┘
        │
        │ 收到解析信息
        ▼
    ┌────────┐
    │分发回调 │
    └────────┘
```

**线程安全**：
- ISR 与线程通过消息队列解耦
- 关键区保护共享变量（header、tail、counter）
- 订阅链表操作使用临界区保护

## 7. API 接口

### 7.1 初始化接口

```c
uart_proto_status_t uart_proto_inst(
    uart_proto_t *const self,
    uart_proto_input_arg_t *const args
);
```

**功能**：初始化协议层实例
**步骤**：
1. 参数验证
2. 分配私有数据
3. 初始化 UART 硬件
4. 创建消息队列
5. 创建解析线程
6. 初始化订阅链表（功能码模式）

### 7.2 ISR 回调接口

```c
void notify_isr_cb(uart_proto_t *const self);
```

**功能**：DMA 接收完成中断回调
**调用时机**：
- UART 空闲中断
- DMA 半满/全满中断
- 周期性定时器中断

### 7.3 订阅接口（功能码模式）

```c
uart_proto_status_t pf_subscribe(
    uart_proto_t *const self,
    subscribe_para_t *const para,
    void **const handle
);

uart_proto_status_t pf_unsubscribe(
    uart_proto_t *const self,
    void *const handle
);
```

### 7.4 算法切换接口（双策略模式）

```c
uart_proto_status_t pf_strategy_algo(
    uart_proto_t *const self,
    parse_algo_t *const algo
);
```

## 8. 错误处理

### 8.1 解析错误类型

| 错误类型 | 处理策略 |
|---------|---------|
| `ALGO_ING` | 解析未完成，等待更多数据 |
| `ALGO_ERR_LENGTH_INVALID` | 长度无效，跳过当前数据 |
| `ALGO_ERR_CRC` | CRC校验失败，跳过当前帧 |
| `ALGO_ERR_NOICE` | 噪声数据，继续解析 |
| `ALGO_ERR_OTHERS` | 严重错误，丢弃所有数据 |

### 8.2 状态重置

```c
void reset_rx_state(uart_proto_t *const self);
```

**使用场景**：
- DMA 错误
- 缓冲区溢出
- 连续解析失败

## 9. 配置选项

### 9.1 编译期配置

```c
// 工作模式选择
#define UART_PROTO_MODE_DEFAULT  UART_PROTO_MODE_FUNCTION_CODE

// 性能参数
#define NUM_NOTIFY_ISR_CB_CALL          3   // ISR回调重试次数
#define MAX_PARSE_NUM_ONCE_TRIGGER      10  // 单次触发最大解析帧数

// 线程配置
#define PARSE_THREAD_PRIORITY           16
#define PARSE_THREAD_STACK_DEPTH        0x200

// 可选特性
#define CUSTOM_RX_THREAD_ATT            1   // 自定义线程属性
#define CUSTOM_UART_PROTO_CONFIG        1   // 自定义协议配置
```

### 9.2 运行期配置

通过 `uart_proto_config_t` 结构体：
```c
typedef struct {
    uint8_t num_notify_isr_cb_call;
    uint8_t max_parse_num_once_trigger;
} uart_proto_config_t;
```

## 10. 典型使用场景

### 10.1 Modbus RTU 协议

**配置**：功能码模式
```c
// 订阅读保持寄存器 (0x03)
subscribe_para_t para = {
    .fun_code = 0x03,
    .cb = modbus_read_holding_register_cb,
    .arg = &my_context
};
self->pf_subscribe(self, &para, &handle);
```

### 10.2 AT 命令解析

**配置**：透传模式
```c
// 设置透传回调
transparent_algo_t algo = {
    .pf_transparent_parse = at_command_parser,
    .arg = &at_context
};
```

### 10.3 多协议兼容

**配置**：双策略模式
```c
// 运行时切换协议
if (detect_protocol_type() == MODBUS) {
    self->pf_strategy_algo(self, &modbus_algo);
} else {
    self->pf_strategy_algo(self, &transparent_algo);
}
```

## 11. 性能优化

### 11.1 零拷贝优化

```c
#define NON_COPY_WHEN_NON_WRAP
```
- 非回绕数据直接在DMA缓冲区解析
- 减少内存拷贝开销

### 11.2 批量解析

- 单次中断可解析多个完整帧
- 通过 `MAX_PARSE_NUM_ONCE_TRIGGER` 限制最大处理量
- 防止中断处理时间过长

### 11.3 有序链表

- 订阅链表按功能码排序
- 加快查找速度
- 支持二分查找优化（可扩展）

## 12. 依赖关系

```
uart_proto.c/h
    ├── t_list.h              (功能码模式)
    ├── os_interface          (线程、队列、临界区)
    ├── uart_ops              (硬件初始化、计数器读写)
    └── parse_algo            (帧解析算法)
```

## 13. 移植指南

### 13.1 必须实现的接口

1. **OS 接口**：
   - 线程创建/删除
   - 队列创建/收发
   - 临界区进入/退出

2. **UART 接口**：
   - 硬件初始化
   - DMA 计数器读写

3. **解析算法**：
   - 功能码解析函数
   - 或透传回调函数

### 13.2 移植步骤

1. 实现 `uart_rx_os_interface_t` 接口
2. 实现 `uart_ops_t` 接口
3. 编写帧解析算法
4. 在中断服务程序中调用 `notify_isr_cb()`
5. 初始化协议层实例

---

**版本**: 1.02  
**作者**: Donzel  
**日期**: 2025-12-30
