# UART Send Layer Architecture

## 1. Overview

The UART Send Layer is a high-reliability serial transmission management framework providing both synchronous and asynchronous send modes, supporting multiple resource synchronization strategies, and featuring comprehensive error recovery mechanisms.

**Core Features**:
- **Synchronous Send**: Blocking transmission with completion callback support
- **Asynchronous Send**: Non-blocking transmission with data buffering
- **Resource Synchronization**: Support for both semaphore and mutex modes
- **Timeout Protection**: Configurable timeout mechanism to prevent deadlock
- **Priority Inversion**: Mutex mode solves priority inversion issues

## 2. Overall Architecture

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦                    UART Send Layer Instance                      ©¦
©¦                   (uart_tx_handler_t)                           ©¦
©À©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©È
©¦                                                                 ©¦
©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´    ©¦
©¦  ©¦ Input Args   ©¦  ©¦ Private Data ©¦  ©¦  Public APIs       ©¦    ©¦
©¦  ©¦  input_arg   ©¦  ©¦  priv_data   ©¦  ©¦pf_send_syn()      ©¦    ©¦
©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¦pf_send_asy()      ©¦    ©¦
©¦                                      ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼    ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                              ©¦
                ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                ©¦             ©¦             ©¦
                ¨‹             ¨‹             ¨‹
        ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
        ©¦    OS    ©¦   ©¦   UART   ©¦   ©¦   Send   ©¦
        ©¦Interface ©¦   ©¦Interface ©¦   ©¦  Buffer  ©¦
        ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

## 3. Core Components

### 3.1 Input Arguments (uart_tx_input_arg_t)

```
uart_tx_input_arg_t
©À©¤©¤ send_buf_att            // Send buffer configuration
©¦   ©À©¤©¤ send_buf            // Buffer pointer
©¦   ©¸©¤©¤ buffer_size         // Buffer size
©À©¤©¤ tx_uart_ops             // Hardware operations interface
©¦   ©À©¤©¤ pf_uart_init()
©¦   ©À©¤©¤ pf_uart_deinit()
©¦   ©¸©¤©¤ pf_uart_write_dma()
©À©¤©¤ os_interface            // Operating system interface
©¦   ©À©¤©¤ pf_os_thread_create()
©¦   ©À©¤©¤ pf_os_sema_create()
©¦   ©À©¤©¤ pf_os_mutex_create()  (Mutex mode)
©¦   ©À©¤©¤ pf_os_queue_create()
©¦   ©À©¤©¤ pf_os_enter_critical()
©¦   ©À©¤©¤ pf_os_exit_critical()
©¦   ©¸©¤©¤ pf_timer_create()     (Timeout mode)
©À©¤©¤ thread_att              // Thread attributes (optional)
©¦   ©À©¤©¤ tx_asy_thread_att   // Async send thread
©¦   ©¸©¤©¤ tx_cpl_thread_att   // Completion callback thread
©¸©¤©¤ uart_tx_cfg             // TX configuration (optional)
```

### 3.2 Private Data (uart_tx_priv_data_t)

```
uart_tx_priv_data_t
©À©¤©¤ is_inited                       // Initialization flag
©À©¤©¤ tx_resource_sema_handle         // TX resource semaphore (semaphore mode)
©À©¤©¤ tx_resource_mutex_handle        // TX resource mutex (mutex mode)
©À©¤©¤ tx_resource_syn_sema_handle     // Sync semaphore (mutex mode)
©À©¤©¤ tx_cpl_ctx_queue_handle         // Completion callback queue
©À©¤©¤ asy_tx_sema                     // Async send semaphore
©À©¤©¤ thread_cpl_ctx_queue_handle     // Thread callback queue
©À©¤©¤ write_offset                    // Write offset
©À©¤©¤ timer                           // Timeout timer (optional)
©¸©¤©¤ is_sending                      // Sending flag
```

## 4. Operation Modes

### 4.1 Synchronous Send Mode

```
Call pf_send_syn() ¡ú Acquire TX Resource ¡ú Start DMA ¡ú Wait Complete ¡ú Release Resource ¡ú Return
                                              ¡ý
                                       DMA Complete IRQ
                                              ¡ý
                                       tx_cpl_isr_cb()
                                              ¡ý
                                  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ø©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                                  ¨‹                       ¨‹
                             ISR Callback            Thread Callback
                           (Interrupt Context)      (Enqueued to Thread)
```

**Features**:
- Blocking call until transmission completes
- Support completion callback (ISR or thread context)
- Suitable for scenarios requiring send confirmation

**Data Stability Requirements**:
- **Semaphore Mode**: Data must remain stable until transmission completes, cannot modify in callback
- **Mutex Mode**: No requirement (calling thread blocks until completion)

### 4.2 Asynchronous Send Mode

```
Call pf_send_asy() ¡ú Copy Data to Buffer ¡ú Return Immediately
                            ¡ý
                  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ø©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                  ¨‹                   ¨‹
            THREAD_ONLY         DIRECT_FIRST
         Always via thread      Try direct first
                  ©¦                   ©¦
                  ¨‹                   ¨‹
          asy_send_thread()    Resource Free?©¤©¤Yes¡ú Direct DMA Send
                                      ©¦
                                     No
                                      ¨‹
                              Send via thread
```

**Async Mode Selection**:

#### Mode 1: UART_ASYNC_SEND_MODE_THREAD_ONLY
- Data always copied to buffer first
- Handled by async send thread uniformly
- **Advantage**: No data stability requirement, can modify after call
- **Disadvantage**: Extra thread context switch delay

#### Mode 2: UART_ASYNC_SEND_MODE_DIRECT_FIRST
- If TX resource is free, send directly in calling thread
- If resource busy, fall back to Mode 1
- **Advantage**: Reduce thread switch, lower latency
- **Disadvantage**: Data must remain stable until transmission completes
- **Note**: Not recommended in mutex mode (would block calling thread)

### 4.3 Resource Synchronization Modes

#### Semaphore Mode (RESOURCE_SYN_MODE_SEMA)

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦TX Resource  ©¦  Initial: 1
©¦ Semaphore   ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©À©¤©¤¡ú Acquire Success ¡ú Send Data ¡ú Complete ¡ú Release Semaphore
      ©¦
      ©¸©¤©¤¡ú Acquire Fail ¡ú Block Wait
```

**Features**:
- Simple and efficient
- May have priority inversion issue
- Suitable for simple applications

#### Mutex Mode (RESOURCE_SYN_MODE_MUTEX)

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´     ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦TX Resource  ©¦     ©¦  Sync Helper ©¦
©¦   Mutex     ©¦     ©¦  Semaphore   ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼     ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦                     ©¦
      ©À©¤©¤¡ú Acquire Mutex    ©¦
      ©¦         ©¦           ©¦
      ©¦         ©¸©¤©¤¡ú Release Helper ¡ú Wait Helper ¡ú Release Mutex
      ©¦
      ©¸©¤©¤¡ú Priority inheritance automatically solves priority inversion
```

**Features**:
- Support priority inheritance
- Solve priority inversion issues
- Require additional sync semaphore
- Suitable for complex multi-priority systems

## 5. Data Flow Diagram

### 5.1 Synchronous Send Flow

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Call Thread ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©¦ pf_send_syn(data, len, cpl_ctx)
      ¨‹
©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
©§ Acquire TX   ©§ ?©¤©¤©¤ Semaphore/Mutex
©§  Resource    ©§
©»©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Check TX Status ©¦
©¦ check_tx_busy() ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Enqueue Completion©¦ ?©¤©¤©¤ tx_cpl_ctx_queue
©¦    Context      ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
¨U  Start DMA TX   ¨U
¨Upf_uart_write_dma¨U
¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Start Timeout   ©¦ (Optional)
©¦     Timer       ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©À©¤©¤©¤©¤ Semaphore Mode ©¤©¤¡ú Release second half (no-op)
      ©¦
      ©¸©¤©¤©¤©¤ Mutex Mode ©¤©¤¡ú Wait sync sema then release mutex
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦   Return   ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼

      ... Wait for DMA Complete ...

¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
¨U DMA Complete IRQ¨U
¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ tx_cpl_isr_cb() ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©À©¤©¤¡ú Stop timeout timer
      ©¦
      ©À©¤©¤¡ú Dequeue completion context
      ©¦
      ©À©¤©¤¡ú Execute ISR callback (if registered)
      ©¦
      ©À©¤©¤¡ú Enqueue thread callback (if registered)
      ©¦
      ©¸©¤©¤¡ú Release TX resource semaphore
```

### 5.2 Asynchronous Send Flow

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Call Thread ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©¦ pf_send_asy(data, len)
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Check Buffer    ©¦
©¦     Space       ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
#if DIRECT_FIRST && !MUTEX
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Try Acquire      ©¦ (Non-blocking)
©¦   Resource      ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©À©¤©¤¡ú Success ¡ú Direct DMA Send ¡ú Return
      ©¦
      ©¸©¤©¤¡ú Fail ¡ý
#endif
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Copy Data to    ©¦
©¦     Buffer      ©¦
©¦write_offset+=len©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Release Async    ©¦
©¦Send Semaphore   ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Return Now  ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼

================================================

©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦asy_send_thread  ©¦ ?©¤©¤©¤ Background Thread
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©¦ Block wait async send semaphore
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Acquire TX      ©¦
©¦   Resource      ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ Read Data from  ©¦
©¦     Buffer      ©¦
©¦  read_offset    ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
¨U  Start DMA TX   ¨U
¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Wait for Complete©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Loop Wait Next   ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

### 5.3 Completion Callback Flow

```
¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
¨U DMA Complete IRQ¨U
¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
      ©¦
      ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Dequeue Context  ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
      ©¦
      ©À©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
      ©¦                  ©¦
      ¨‹                  ¨‹
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´      ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦ISR       ©¦      ©¦Thread       ©¦
©¦Callback  ©¦      ©¦Callback     ©¦
©¦Execute   ©¦      ©¦Enqueued     ©¦
©¦Now       ©¦      ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼            ©¦
                        ¨‹
                  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                  ©¦tx_cpl_thread©¦ ?©¤©¤©¤ Background Thread
                  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                        ©¦
                        ©¦ Dequeue
                        ¨‹
                  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                  ©¦Execute User ©¦
                  ©¦  Callback   ©¦
                  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

## 6. Thread Model

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Async Send Thread    ©¦  Priority: TX_ASY_THREAD_PRIORITY (15)
©¦asy_send_thread()   ©¦  Stack: TX_ASY_THREAD_STACK_DEPTH (0x200)
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
        ©¦
        ©¦ Wait asy_tx_sema
        ¨‹
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦Acquire ©¦
    ©¦Resource©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¼
        ©¦
        ¨‹
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦DMA Send©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¼

©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦Completion Thread    ©¦  Priority: TX_CPL_THREAD_PRIORITY (14)
©¦tx_cpl_thread()      ©¦  Stack: TX_CPL_THREAD_STACK_DEPTH (0x200)
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
        ©¦
        ©¦ Wait thread_cpl_ctx_queue
        ¨‹
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦Execute ©¦
    ©¦Callback©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

**Thread Priority Recommendation**:
- **Async Send Thread** > **Completion Thread** > **Application Threads**
- Prevent async send from being blocked
- Ensure transmission real-time performance

## 7. API Interfaces

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
5. Create sync resources (semaphore/mutex)
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
- `UART_TX_OK`: Send success
- `UART_TX_ERR_PARAM_INVALID`: Invalid parameter
- `UART_TX_ERR_HANDLER_NOT_READY`: Not initialized
- `UART_TX_ERR_OTHERS`: Other errors

**Data Stability**:
- **Semaphore Mode**: Data must remain stable until transmission completes
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
- `UART_TX_ERR_BUFFER_NOT_SUFFICIENT`: Insufficient buffer space

**Data Stability**:
- **THREAD_ONLY Mode**: No requirement (data copied)
- **DIRECT_FIRST Mode**: Data must remain stable (may send directly)

### 7.4 State Reset Interface

```c
void reset_tx_state(uart_tx_handler_t *const self);
```

**Function**: Reset transmission state
**Usage Scenarios**:
- Timeout recovery
- Error handling
- Manual clear of send flag

### 7.5 Completion Callback Interface

```c
void tx_cpl_isr_cb(uart_tx_handler_t *const self);
```

**Function**: DMA transmission complete interrupt callback
**Call Location**: UART DMA transfer complete interrupt service routine

## 8. Error Handling

### 8.1 Timeout Protection Mechanism

```c
#define UART_TX_RESOURCE_TIMEOUT_TICK  1000  // Timeout (ms)
```

**Working Principle**:
1. Start timer when DMA transmission starts
2. Stop timer on normal completion
3. Call `timer_cb()` to reset state on timeout

**Problems Prevented**:
- DMA completion interrupt lost
- Hardware failure causing stuck transmission
- Software exception causing unreleased semaphore

### 8.2 Send Busy Detection

```c
static inline bool check_tx_busy(uart_tx_handler_t *const self)
```

**Function**: Detect and fix abnormal send state
**Scenarios**:
- `is_sending` flag inconsistent with resource state
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
- Prevent overwriting unsent data

## 9. Configuration Options

### 9.1 Compile-Time Configuration

```c
// Completion callback thread switch
#define IS_ENABLE_CPL_THREAD              1

// Async send mode
#define UART_ASYNC_SEND_MODE_DEFAULT      UART_ASYNC_SEND_MODE_THREAD_ONLY

// Resource sync mode
#define RESOURCE_SYN_MODE_DEFAULT         RESOURCE_SYN_MODE_MUTEX

// Timeout protection (0 = disable)
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

#### Thread Attribute Configuration
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
// Periodic sensor data transmission
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
// Send command response with confirmation
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
High Priority Task (H) ©¤©¤©´
                         ©¦ Wait for TX Resource
                         ¨‹
Medium Priority Task (M) ©¤©¤©¤©¤¡ú Preempt ©¤©¤©¤©¤¡ú Low Priority Task Delayed
                                           ©¦
Low Priority Task (L) ©¤©¤©¤¡ú Hold TX Resource ©¤©¤©¼

Result: High priority task indirectly blocked by medium priority task
```

### 11.2 Solutions

#### Solution 1: Use Mutex (Recommended)

```c
#define RESOURCE_SYN_MODE_DEFAULT  RESOURCE_SYN_MODE_MUTEX
```

**Principle**:
- Mutex supports priority inheritance
- Low priority task temporarily promoted when holding resource
- Avoid preemption by medium priority tasks

#### Solution 2: Adjust Priority Design

```
Async send thread priority > All application threads
```

**Principle**:
- Async send thread quickly releases resource
- Reduce resource holding time
- Lower priority inversion risk

#### Solution 3: Test Point Injection

```c
#define PRIORITY_INVERSION_TEST_FUNC()  priority_inversion_test_func()

void priority_inversion_test_func(void) {
    // Inject test code to verify priority inversion issue
}
```

## 12. Performance Optimization

### 12.1 Reduce Thread Context Switches

**Optimization Strategy**:
- Use `UART_ASYNC_SEND_MODE_DIRECT_FIRST`
- Send directly in calling thread when resource free
- Avoid unnecessary thread wakeups

**Applicable Scenarios**:
- Low send frequency
- Reasonable application thread priorities
- Not using mutex mode

### 12.2 Buffer Size Design

**Principle**:
```
Buffer Size >= Max Single Send Size ¡Á 2
```

**Considerations**:
- Send frequency
- Single send data size
- Transmission time
- System real-time requirements

### 12.3 Callback Execution Optimization

**ISR Callback**:
- Keep as short as possible
- Do not call blocking functions
- Do not execute time-consuming operations

**Thread Callback**:
- Can execute complex logic
- Can call blocking functions
- Pay attention to thread priority configuration

## 13. Dependencies

```
uart_send.c/h
    ©À©¤©¤ os_interface    (Thread, semaphore, mutex, queue, timer)
    ©À©¤©¤ tx_uart_ops     (HW init, DMA write)
    ©¸©¤©¤ send_buf_att    (Send buffer)
```

## 14. Porting Guide

### 14.1 Required Interfaces

1. **OS Interface**:
   - Thread create/delete
   - Semaphore create/acquire/release
   - Mutex create/acquire/release (mutex mode)
   - Queue create/put/get
   - Critical section enter/exit
   - Timer create/start/stop (timeout mode)

2. **UART Interface**:
   - Hardware init/deinit
   - DMA write

### 14.2 Porting Steps

1. Implement `uart_tx_os_interface_t` interface according to RTOS type
2. Implement `tx_uart_ops_t` UART hardware interface
3. Configure operation modes (sync mode, async mode, resource sync mode)
4. Call `tx_cpl_isr_cb()` in DMA transmission complete interrupt
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

| Issue | Possible Cause | Investigation Method |
|-------|---------------|---------------------|
| Send stuck | Interrupt not triggered | Check interrupt config, enable timeout protection |
| Data loss | Buffer overflow | Increase buffer size, check send frequency |
| Priority inversion | Using semaphore | Switch to mutex mode |
| High send latency | Low thread priority | Adjust async send thread priority |
| Callback not executed | Queue full | Increase callback queue depth |

---

**Version**: 1.00  
**Author**: Donzel  
**Date**: 2025-12-30
