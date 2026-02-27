# UART Send Layer Architecture

## 1. Overview

The UART send layer is a high-reliability serial transmission management framework that provides both synchronous and asynchronous send modes, supports multiple resource synchronization strategies, and has a comprehensive error recovery mechanism.

**Core Features**:
- **Synchronous Send**: Blocking send with completion callback support
- **Asynchronous Send**: Non-blocking send, data buffered and returns immediately
- **Resource Synchronization**: Supports both semaphore and mutex modes
- **Timeout Protection**: Configurable timeout mechanism to prevent deadlock
- **Priority Inversion**: Mutex mode resolves priority inversion issues

## 2. Overall Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     UART Send Layer Instance                     │
│                   (uart_tx_handler_t)                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐    │
│  │ Input Args   │  │ Private Data │  │  Public API       │    │
│  │  input_arg   │  │  priv_data   │  │pf_send_syn()      │    │
│  └──────────────┘  └──────────────┘  │pf_send_asy()      │    │
│                                      └───────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┼─────────────┐
                │             │             │
                ▼             ▼             ▼
        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │OS Interface│  │UART Ops  │   │Send Buffer│
        └──────────┘   └──────────┘   └──────────┘
```

## 3. Core Components

### 3.1 Input Arguments (uart_tx_input_arg_t)

```
uart_tx_input_arg_t
├── send_buf_att            // Send buffer configuration
│   ├── send_buf            // Buffer pointer
│   └── buffer_size         // Buffer size
├── tx_uart_ops             // Hardware operation interface
│   ├── pf_uart_init()
│   ├── pf_uart_deinit()
│   └── pf_uart_write_dma()
├── os_interface            // Operating system interface
│   ├── pf_os_thread_create()
│   ├── pf_os_sema_create()
│   ├── pf_os_mutex_create()  (Mutex mode)
│   ├── pf_os_queue_create()
│   ├── pf_os_enter_critical()
│   ├── pf_os_exit_critical()
│   └── pf_timer_create()     (Timeout mode)
├── thread_att              // Thread attributes (optional)
│   ├── tx_asy_thread_att   // Async send thread
│   └── tx_cpl_thread_att   // Completion callback thread
└── uart_tx_cfg             // Send configuration (optional)
```

### 3.2 Private Data (uart_tx_priv_data_t)

```
uart_tx_priv_data_t
├── is_inited                       // Initialization flag
├── tx_resource_sema_handle         // TX resource semaphore (semaphore mode)
├── tx_resource_mutex_handle        // TX resource mutex (mutex mode)
├── tx_resource_syn_sema_handle     // Sync semaphore (mutex mode)
├── tx_cpl_ctx_queue_handle         // Completion callback queue
├── asy_tx_sema                     // Async send semaphore
├── thread_cpl_ctx_queue_handle     // Thread callback queue
├── write_offset                    // Write offset
├── timer                           // Timeout timer (optional)
└── is_sending                      // Sending flag
```

## 4. Operating Modes

### 4.1 Synchronous Send Mode

```
Call pf_send_syn() → Acquire TX Resource → Start DMA Transfer → Wait Complete → Release Resource → Return
                                        ↓
                                  DMA Complete IRQ
                                        ↓
                                   tx_cpl_isr_cb()
                                        ↓
                              ┌─────────┴─────────┐
                              ▼                   ▼
                         ISR Callback         Thread Callback
                        (IRQ Context)         (Enqueued to thread)
```

**Features**:
- Blocking call until send completes
- Supports completion callback (ISR context or thread context)
- Suitable for scenarios requiring send confirmation

**Data Stability Requirements**:
- **Semaphore Mode**: Data must remain stable until send completes, cannot be modified in callback
- **Mutex Mode**: No requirement, calling thread blocks until send completes

### 4.2 Asynchronous Send Mode

```
Call pf_send_asy() → Copy Data to Buffer → Return Immediately
                            ↓
                  ┌─────────┴─────────┐
                  ▼                   ▼
            THREAD_ONLY         DIRECT_FIRST
            Always send via thread    Direct send preferred
                  │                   │
                  ▼                   ▼
          asy_send_thread()     Resource Free?──Yes→ Direct DMA Send
                                      │
                                     No
                                      ▼
                              Send via thread
```

**Async Mode Selection**:

#### Mode 1: UART_ASYNC_SEND_MODE_THREAD_ONLY
- Data always copied to buffer first
- Handled uniformly by async send thread
- **Pros**: No data stability requirement, can modify after calling
- **Cons**: Extra thread switching delay

#### Mode 2: UART_ASYNC_SEND_MODE_DIRECT_FIRST
- If TX resource is free, send directly in calling thread
- If resource is busy, fall back to mode 1
- **Pros**: Reduces thread switching, lowers latency
- **Cons**: Data must remain stable until send completes
- **Note**: Not recommended with mutex mode (causes calling thread to block)

### 4.3 Resource Synchronization Modes

#### Semaphore Mode (RESOURCE_SYN_MODE_SEMA)

```
┌─────────────┐
│ TX Resource │  Initial Value: 1
│  Semaphore  │
└─────────────┘
      │
      ├──→ Acquire Success → Send Data → Send Complete → Release Semaphore
      │
      └──→ Acquire Fail → Block Wait
```

**Features**:
- Simple and efficient
- Possible priority inversion issue
- Suitable for simple application scenarios

#### Mutex Mode (RESOURCE_SYN_MODE_MUTEX)

```
┌─────────────┐     ┌──────────────┐
│ TX Resource │     │  Sync Helper │
│   Mutex     │     │  Semaphore   │
└─────────────┘     └──────────────┘
      │                     │
      ├──→ Acquire Mutex    │
      │         │           │
      │         └──→ Release Helper Sema → Wait Helper Sema → Release Mutex
      │
      └──→ Priority Inheritance mechanism automatically solves priority inversion
```

**Features**:
- Supports priority inheritance
- Solves priority inversion issues
- Requires additional sync semaphore cooperation
- Suitable for complex multi-priority systems

## 5. Data Flow Diagram

### 5.1 Synchronous Send Flow

```
┌────────────┐
│Calling Thread│
└────────────┘
      │
      │ pf_send_syn(data, len, cpl_ctx)
      ▼
┏━━━━━━━━━━━━━━┓
┃ Acquire TX   ┃ ←─── Semaphore/Mutex
┃  Resource    ┃
┗━━━━━━━━━━━━━━┛
      │
      ▼
┌─────────────────┐
│ Check TX State  │
│ check_tx_busy() │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Enqueue Completion│ ←─── tx_cpl_ctx_queue
│    Context      │
└─────────────────┘
      │
      ▼
╔═════════════════╗
║ Start DMA       ║
║ pf_uart_write_dma()║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ Start Timeout   │ (Optional)
│     Timer       │
└─────────────────┘
      │
      ├──── Semaphore Mode ──→ Release sync semaphore second half (no-op)
      │
      └──── Mutex Mode ──→ Wait sync semaphore then release mutex
      │
      ▼
┌────────────┐
│   Return   │
└────────────┘

      ... Wait for DMA Complete ...

╔═════════════════╗
║ DMA Complete IRQ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ tx_cpl_isr_cb() │
└─────────────────┘
      │
      ├──→ Stop timeout timer
      │
      ├──→ Dequeue completion context
      │
      ├──→ Execute ISR callback (if registered)
      │
      ├──→ Enqueue thread callback (if registered)
      │
      └──→ Release TX resource semaphore
```

### 5.2 Asynchronous Send Flow

```
┌────────────┐
│Calling Thread│
└────────────┘
      │
      │ pf_send_asy(data, len)
      ▼
┌─────────────────┐
│ Check Buffer    │
│     Space       │
└─────────────────┘
      │
      ▼
#if DIRECT_FIRST && !MUTEX
┌─────────────────┐
│ Try Acquire     │ (Non-blocking)
│   Resource      │
└─────────────────┘
      │
      ├──→ Success → Direct DMA Send → Return
      │
      └──→ Fail ↓
#endif
      ▼
┌─────────────────┐
│ Copy Data to    │
│     Buffer      │
│ write_offset += len│
└─────────────────┘
      │
      ▼
┌─────────────────┐
│Release Async Send│
│   Semaphore     │
└─────────────────┘
      │
      ▼
┌────────────┐
│Return Immediately│
└────────────┘

================================================

┌─────────────────┐
│ asy_send_thread │ ←─── Background Thread
└─────────────────┘
      │
      │ Block wait async send semaphore
      ▼
┌─────────────────┐
│ Acquire TX      │
│   Resource      │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Read Data from  │
│     Buffer      │
│ read_offset     │
└─────────────────┘
      │
      ▼
╔═════════════════╗
║ Start DMA       ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ Wait for Send   │
│   Complete      │
└─────────────────┘
      │
      ▼
┌─────────────────┐
│ Loop Wait Next  │
│      Send       │
└─────────────────┘
```

### 5.3 Completion Callback Flow

```
╔═════════════════╗
║ DMA Complete IRQ║
╚═════════════════╝
      │
      ▼
┌─────────────────┐
│ Dequeue Context │
│   from Queue    │
└─────────────────┘
      │
      ├──────────────────┐
      │                  │
      ▼                  ▼
┌──────────┐      ┌─────────────┐
│ISR Callback│     │Thread Callback│
│Execute   │      │  Enqueue    │
│Immediately│      └─────────────┘
└──────────┘            │
                        ▼
                  ┌─────────────┐
                  │tx_cpl_thread│ ←─── Background Thread
                  └─────────────┘
                        │
                        │ Dequeue
                        ▼
                  ┌─────────────┐
                  │Execute User │
                  │  Callback   │
                  └─────────────┘
```

## 6. Thread Model

```
┌─────────────────────┐
│ Async Send Thread   │  Priority: TX_ASY_THREAD_PRIORITY (15)
│ asy_send_thread()   │  Stack Depth: TX_ASY_THREAD_STACK_DEPTH (0x200)
└─────────────────────┘
        │
        │ Wait asy_tx_sema
        ▼
    ┌────────┐
    │Acquire │
    │Resource│
    └────────┘
        │
        ▼
    ┌────────┐
    │DMA Send│
    └────────┘

┌─────────────────────┐
│ Completion Thread   │  Priority: TX_CPL_THREAD_PRIORITY (14)
│ tx_cpl_thread()     │  Stack Depth: TX_CPL_THREAD_STACK_DEPTH (0x200)
└─────────────────────┘
        │
        │ Wait thread_cpl_ctx_queue
        ▼
    ┌────────┐
    │Execute │
    │Callback│
    └────────┘
```

**Thread Priority Recommendations**:
- **Async Send Thread** > **Completion Callback Thread** > **Application Threads**
- Prevents async send from being blocked
- Ensures send real-time performance

## 7. API Interface

### 7.1 Initialization Interface

```c
uart_tx_status_t uart_tx_inst(
    uart_tx_handler_t *const self,
    uart_tx_input_arg_t *const args
);
```

**Function**: Initialize send layer instance
**Steps**:
1. Parameter validation
2. Allocate private data
3. Create async send thread
4. Create completion callback thread (optional)
5. Create synchronization resources (semaphore/mutex)
6. Create timeout timer (optional)
7. Register API function pointers

### 7.2 Synchronous Send Interface

```c
uart_tx_status_t pf_send_syn(
    uart_tx_handler_t *const self,
    uint8_t *const data,
    uint16_t length,
    uart_tx_cpl_ctx_t *uart_tx_cpl_ctx
);
```

**Parameters**:
- `data`: Data pointer to send
- `length`: Data length
- `uart_tx_cpl_ctx`: Completion callback context (can be NULL)

**Return Values**:
- `UART_TX_OK`: Send successful
- `UART_TX_ERR_PARAM_INVALID`: Invalid parameter
- `UART_TX_ERR_HANDLER_NOT_READY`: Not initialized
- `UART_TX_ERR_OTHERS`: Other errors

**Data Stability**:
- **Semaphore Mode**: Data must remain stable until send completes
- **Mutex Mode**: No requirement (function blocks internally)

### 7.3 Asynchronous Send Interface

```c
uart_tx_status_t pf_send_asy(
    uart_tx_handler_t *const self,
    uint8_t *const data,
    uint16_t length
);
```

**Parameters**:
- `data`: Data pointer to send
- `length`: Data length

**Return Values**:
- `UART_TX_OK`: Data enqueued
- `UART_TX_ERR_BUFFER_NOT_SUFFICIENT`: Buffer space insufficient

**Data Stability**:
- **THREAD_ONLY Mode**: No requirement (data already copied)
- **DIRECT_FIRST Mode**: Data must remain stable (may send directly)

### 7.4 State Reset Interface

```c
void reset_tx_state(uart_tx_handler_t *const self);
```

**Function**: Reset send state
**Usage Scenarios**:
- Timeout recovery
- Error handling
- Manual send flag clearing

### 7.5 Completion Callback Interface

```c
void tx_cpl_isr_cb(uart_tx_handler_t *const self);
```

**Function**: DMA send complete interrupt callback
**Invocation Location**: UART DMA transfer complete interrupt service routine

## 8. Error Handling

### 8.1 Timeout Protection Mechanism

```c
#define UART_TX_RESOURCE_TIMEOUT_TICK  1000  // Timeout (ms)
```

**Working Principle**:
1. Start timer when DMA send starts
2. Stop timer when normally completes
3. Call `timer_cb()` to reset state on timeout

**Prevents Issues**:
- Lost DMA complete interrupt
- Hardware fault causing send hang
- Software exception causing semaphore not released

### 8.2 Send Busy Detection

```c
static inline bool check_tx_busy(uart_tx_handler_t *const self)
```

**Function**: Detect and fix abnormal send states
**Scenarios**:
- Inconsistency between `is_sending` flag and resource state
- Interrupt handling exception
- Multi-thread race condition

### 8.3 Buffer Overflow Protection

```c
if (remain < length) {
    OS_INTERFACE(self)->pf_os_exit_critical(0);
    US_DEBUG_ERR("Buffer not sufficient!\r\n");
    return UART_TX_ERR_BUFFER_NOT_SUFFICIENT;
}
```

**Protection Mechanism**:
- Check remaining space before async send
- Atomic check and update in critical section
- Prevents overwriting unsent data

## 9. Configuration Options

### 9.1 Compile-Time Configuration

```c
// Completion callback thread switch
#define IS_ENABLE_CPL_THREAD              1

// Async send mode
#define UART_ASYNC_SEND_MODE_DEFAULT      UART_ASYNC_SEND_MODE_THREAD_ONLY

// Resource sync mode
#define RESOURCE_SYN_MODE_DEFAULT         RESOURCE_SYN_MODE_MUTEX

// Timeout protection (0 = disabled)
#define UART_TX_RESOURCE_TIMEOUT_TICK     1000

// Thread configuration
#define TX_ASY_THREAD_PRIORITY            15
#define TX_ASY_THREAD_STACK_DEPTH         0x200
#define TX_CPL_THREAD_PRIORITY            14
#define TX_CPL_THREAD_STACK_DEPTH         0x200

// Completion callback queue depth
#define MAX_NUM_CHACHE_TX_CPL_THREAD      10

// Optional features
#define CUSTOM_TX_THREAD_ATT              1  // Custom thread attributes
#define CUSTOM_UART_TX_CFG                0  // Custom configuration
```

### 9.2 Runtime Configuration

#### Thread Attributes Configuration
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

#### UART TX Configuration
```c
uart_tx_cfg_t cfg = {
    .max_num_chache_tx_cpl_thread = 20  // Increase callback queue depth
};
```

## 10. Typical Use Cases

### 10.1 Real-Time Data Transmission (Sensor Data Upload)

**Recommended Configuration**:
- Async send mode: `UART_ASYNC_SEND_MODE_THREAD_ONLY`
- Resource sync mode: `RESOURCE_SYN_MODE_MUTEX`
- Completion callback: Not needed

```c
// Periodic sensor data send
void sensor_task(void *arg) {
    uint8_t data[64];
    while (1) {
        read_sensor_data(data);
        tx_handler->pf_send_asy(tx_handler, data, 64);
        os_delay(100);  // 100ms period
    }
}
```

### 10.2 Command Response (Requires Confirmation)

**Recommended Configuration**:
- Synchronous send mode
- Resource sync mode: `RESOURCE_SYN_MODE_MUTEX`
- Completion callback: Thread callback

```c
// Send command response and confirm
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
    // Response sent, update state
    update_state(STATE_RESPONSE_SENT);
}
```

### 10.3 Log Output (High Frequency)

**Recommended Configuration**:
- Async send mode: `UART_ASYNC_SEND_MODE_DIRECT_FIRST`
- Resource sync mode: `RESOURCE_SYN_MODE_SEMA` (log priorities usually consistent)
- Completion callback: Not needed

```c
// High frequency log output
void log_output(const char *msg) {
    tx_handler->pf_send_asy(tx_handler, 
                            (uint8_t*)msg, 
                            strlen(msg));
}
```

## 11. Priority Inversion Problem

### 11.1 Problem Description

```
High Priority Task (H) ──┐
                         │ Wait TX Resource
                         ▼
Mid Priority Task (M) ────→ Preempt ────→ Low Priority Task Execution Delayed
                                      │
Low Priority Task (L) ───→ Hold TX Resource ──┘

Result: High priority task indirectly blocked by mid priority task
```

### 11.2 Solutions

#### Solution 1: Use Mutex (Recommended)

```c
#define RESOURCE_SYN_MODE_DEFAULT  RESOURCE_SYN_MODE_MUTEX
```

**Principle**:
- Mutex supports priority inheritance
- Low priority task temporarily promoted to high priority when holding resource
- Avoids being preempted by mid priority task

#### Solution 2: Adjust Priority Design

```
Async Send Thread Priority > All Application Threads
```

**Principle**:
- Async send thread quickly releases resource
- Reduces resource hold time
- Lowers priority inversion risk

#### Solution 3: Test Point Injection

```c
#define PRIORITY_INVERSION_TEST_FUNC()  priority_inversion_test_func()

void priority_inversion_test_func(void) {
    // Inject test code to verify priority inversion issue
}
```

## 12. Performance Optimization

### 12.1 Reduce Thread Switching

**Optimization Strategy**:
- Use `UART_ASYNC_SEND_MODE_DIRECT_FIRST`
- Send directly in calling thread when resource is free
- Avoid unnecessary thread wakeup

**Applicable Scenarios**:
- Low send frequency
- Reasonable application thread priorities
- Not using mutex mode

### 12.2 Buffer Size Design

**Principle**:
```
Buffer Size >= Maximum Single Send × 2
```

**Considerations**:
- Send frequency
- Single send data size
- Send duration
- System real-time requirements

### 12.3 Callback Execution Optimization

**ISR Callback**:
- Keep as short as possible
- Don't call blocking functions
- Don't execute time-consuming operations

**Thread Callback**:
- Can execute complex logic
- Can call blocking functions
- Pay attention to thread priority configuration

## 13. Dependencies

```
uart_send.c/h
    ├── os_interface          (Thread, semaphore, mutex, queue, timer)
    ├── tx_uart_ops           (Hardware init, DMA write)
    └── send_buf_att          (Send buffer)
```

## 14. Porting Guide

### 14.1 Required Interfaces

1. **OS Interface**:
   - Thread create/delete
   - Semaphore create/acquire/release
   - Mutex create/acquire/release (mutex mode)
   - Queue create/send/receive
   - Critical section enter/exit
   - Timer create/start/stop (timeout mode)

2. **UART Interface**:
   - Hardware init/deinit
   - DMA write

### 14.2 Porting Steps

1. Implement `uart_tx_os_interface_t` interface based on RTOS type
2. Implement `tx_uart_ops_t` UART hardware interface
3. Configure operating modes (sync mode, async mode, resource sync mode)
4. Call `tx_cpl_isr_cb()` in DMA send complete interrupt
5. Initialize send layer instance and register to system

### 14.3 FreeRTOS Porting Example

```c
// OS interface implementation
uart_tx_os_interface_t os_interface = {
    .pf_os_thread_create = freertos_thread_create,
    .pf_os_sema_create = freertos_sema_create,
    .pf_os_sema_acquire = freertos_sema_acquire,
    .pf_os_sema_release = freertos_sema_release,
    .pf_os_mutex_create = freertos_mutex_create,
    .pf_os_mutex_acquire = freertos_mutex_acquire,
    .pf_os_mutex_release = freertos_mutex_release,
    // ... other interfaces
};

// UART interface implementation
tx_uart_ops_t uart_ops = {
    .pf_uart_init = hal_uart_init,
    .pf_uart_deinit = hal_uart_deinit,
    .pf_uart_write_dma = hal_uart_dma_transmit
};
```

## 15. Debug Support

### 15.1 Debug Macros

```c
#define DEBUG_UART_SEND           // Enable debug output

#ifdef DEBUG_UART_SEND
#define US_DEBUG_OUT(fmt, ...)    DEBUG_OUT(fmt, ##__VA_ARGS__)
#define US_DEBUG_ERR(fmt, ...)    DEBUG_OUT_ERR(fmt, ##__VA_ARGS__)
#endif
```

### 15.2 Common Issue Troubleshooting

| Issue | Possible Cause | Troubleshooting Method |
|-------|---------------|----------------------|
| Send hangs | Interrupt not triggered | Check interrupt config, enable timeout protection |
| Data loss | Buffer overflow | Increase buffer size, check send frequency |
| Priority inversion | Using semaphore | Switch to mutex mode |
| High send latency | Low thread priority | Adjust async send thread priority |
| Callback not executed | Queue full | Increase callback queue depth |

---

**Version**: 1.00  
**Author**: Donzel  
**Date**: 2025-12-30
