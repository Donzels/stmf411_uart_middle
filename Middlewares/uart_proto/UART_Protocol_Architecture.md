# UART Protocol Layer Architecture

## 1. Overview

The UART Protocol Layer is a configurable asynchronous serial protocol processing framework supporting three operation modes:
- **Function Code Mode**: Frame parsing and callback dispatching based on function codes
- **Transparent Mode**: Raw data transparent transmission
- **Dual Strategy Mode**: Runtime dynamic switching between the above two modes

## 2. Overall Architecture

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦                    UART Protocol Instance                        ©¦
©¦                      (uart_proto_t)                             ©¦
©À©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©È
©¦                                                                 ©¦
©¦  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´  ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´    ©¦
©¦  ©¦ Input Args   ©¦  ©¦ Private Data ©¦  ©¦  Public APIs       ©¦    ©¦
©¦  ©¦  input_arg   ©¦  ©¦  priv_data   ©¦  ©¦pf_subscribe()     ©¦    ©¦
©¦  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼  ©¦pf_unsubscribe()   ©¦    ©¦
©¦                                      ©¦pf_strategy_algo() ©¦    ©¦
©¦                                      ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼    ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                              ©¦
                ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©à©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
                ©¦             ©¦             ©¦
                ¨‹             ¨‹             ¨‹
        ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´   ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
        ©¦    OS    ©¦   ©¦   UART   ©¦   ©¦  Parser  ©¦
        ©¦Interface ©¦   ©¦Interface ©¦   ©¦ Algorithm©¦
        ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼   ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

## 3. Core Components

### 3.1 Input Arguments (uart_proto_input_arg_t)

```
uart_proto_input_arg_t
©À©¤©¤ frame_parse_att         // Frame parsing attributes
©¦   ©À©¤©¤ recv_buf_att        // Receive buffer configuration
©¦   ©¦   ©À©¤©¤ recv_buf        // Buffer pointer
©¦   ©¦   ©¸©¤©¤ buffer_size     // Buffer size
©¦   ©¸©¤©¤ parse_algo          // Parsing algorithm
©¦       ©À©¤©¤ algo_type       // Algorithm type (dual mode)
©¦       ©¸©¤©¤ u               // Algorithm union
©¦           ©À©¤©¤ funcoude_algo     // Function code algorithm
©¦           ©¸©¤©¤ transparent_algo  // Transparent algorithm
©À©¤©¤ uart_ops                // Hardware operation interface
©¦   ©À©¤©¤ pf_uart_init()
©¦   ©À©¤©¤ pf_get_counter()
©¦   ©¸©¤©¤ pf_set_counter()
©À©¤©¤ os_interface            // Operating system interface
©¦   ©À©¤©¤ pf_os_thread_create()
©¦   ©À©¤©¤ pf_os_queue_create()
©¦   ©À©¤©¤ pf_os_enter_critical()
©¦   ©¸©¤©¤ pf_os_exit_critical()
©À©¤©¤ thread_att              // Thread attributes (optional)
©¸©¤©¤ uart_proto_config       // Protocol configuration (optional)
```

### 3.2 Private Data (uart_proto_priv_data_t)

```
uart_proto_priv_data_t
©À©¤©¤ is_inited                    // Initialization flag
©À©¤©¤ num_notify_isr_cb_call       // ISR notification count
©À©¤©¤ parse_fail_cnt               // Parse failure counter
©À©¤©¤ queue_handle                 // Queue handle
©À©¤©¤ data_counter                 // Data counter
©À©¤©¤ header                       // Header index
©À©¤©¤ tail                         // Tail index
©À©¤©¤ parse_buf                    // Parse buffer
©¸©¤©¤ funcode_sentinel             // Function code list sentinel (function code mode)
```

## 4. Operation Modes

### 4.1 Function Code Mode (UART_PROTO_MODE_FUNCTION_CODE)

```
Receive Data ¡ú Frame Parsing ¡ú Extract Function Code ¡ú Find Subscribers ¡ú Callback Notification
```

**Features**:
- Support multiple subscribers for different function codes
- Use sorted linked list to manage subscriptions
- Automatic multi-frame parsing until buffer exhausted

**Subscription Mechanism**:
```
Subscriber A (Function Code 0x01)  ©¤©¤©¤©´
Subscriber B (Function Code 0x03)  ©¤©¤©¤©à©¤©¤¡ú Sorted Linked List
Subscriber C (Function Code 0x05)  ©¤©¤©¤©¼
```

### 4.2 Transparent Mode (UART_PROTO_MODE_TRANSPARENT)

```
Receive Data ¡ú Directly Enqueue ¡ú Transparent Callback
```

**Features**:
- No frame structure parsing
- Raw data directly forwarded
- Suitable for custom protocols or streaming data

### 4.3 Dual Strategy Mode (UART_PROTO_MODE_DUAL_STRATEGY)

```
Receive Data ¡ú Check Current Algorithm Type ¡ú Function Code Parse / Transparent Handle
                            ¡ü
                   Runtime Switchable
```

**Features**:
- Runtime dynamic algorithm switching
- Switch via `pf_strategy_algo()` interface
- Contains both function code subscription mechanism and transparent interface

## 5. Data Flow Diagram

### 5.1 Receive Flow

```
              ¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
              ¨U UART DMA IRQ  ¨U
              ¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
                      ©¦
                      ¨‹
         ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
         ©¦   notify_isr_cb()      ©¦ ?©¤©¤©¤ Called from ISR context
         ©¦  1. Calculate ring     ©¦
         ©¦     buffer range       ©¦
         ©¦  2. Handle wrap cases  ©¦
         ©¦  3. Call parse function©¦
         ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                      ©¦
         ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ø©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
         ©¦                         ©¦
         ¨‹                         ¨‹
  ©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·        ©³©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©·
  ©§Function Code©§        ©§ Transparent ©§
  ©§   Parsing   ©§        ©§   Parsing   ©§
  ©»©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿        ©»©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¥©¿
         ©¦                         ©¦
         ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©Ð©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                      ¨‹
              ¨X¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨[
              ¨U Message Queue ¨U
              ¨^¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨a
                      ©¦
                      ¨‹
         ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
         ©¦   parse_thread()       ©¦ ?©¤©¤©¤ Background thread
         ©¦  1. Get data from queue©¦
         ©¦  2. Call subscriber    ©¦
         ©¦     callbacks          ©¦
         ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
                      ©¦
                      ¨‹
              ©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
              ©¦User Callbacks©¦
              ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

### 5.2 Ring Buffer Management

```
DMA Receive Buffer (Ring):
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦  #########-------########             ©¦
©¦  ¡ü        ¡ü       ¡ü                   ©¦
©¦  Used     tail    DMA write pos       ©¦
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
  
  header: Total received bytes (accumulated)
  tail:   Processed bytes (accumulated)
  data_counter: Last DMA stop position
```

**Wrap Handling**:
- If data crosses buffer boundary, copy to `parse_buf`
- If data in contiguous region, process directly (`NON_COPY_WHEN_NON_WRAP`)

## 6. Thread Model

```
©°©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©´
©¦  Parse Thread   ©¦  Priority: PARSE_THREAD_PRIORITY (16)
©¦ parse_thread()  ©¦  Stack: PARSE_THREAD_STACK_DEPTH (0x200)
©¸©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¼
        ©¦
        ©¦ Block on message queue
        ¨‹
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦  Wait  ©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¼
        ©¦
        ©¦ Receive parse info
        ¨‹
    ©°©¤©¤©¤©¤©¤©¤©¤©¤©´
    ©¦Dispatch©¦
    ©¸©¤©¤©¤©¤©¤©¤©¤©¤©¼
```

**Thread Safety**:
- ISR and thread decoupled via message queue
- Critical section protects shared variables (header, tail, counter)
- Subscription list operations use critical sections

## 7. API Interfaces

### 7.1 Initialization Interface

```c
uart_proto_status_t uart_proto_inst(
    uart_proto_t *const self,
    uart_proto_input_arg_t *const args
);
```

**Function**: Initialize protocol layer instance
**Steps**:
1. Parameter validation
2. Allocate private data
3. Initialize UART hardware
4. Create message queue
5. Create parse thread
6. Initialize subscription list (function code mode)

### 7.2 ISR Callback Interface

```c
void notify_isr_cb(uart_proto_t *const self);
```

**Function**: DMA receive complete interrupt callback
**Call Timing**:
- UART idle interrupt
- DMA half-full/full interrupt
- Periodic timer interrupt

### 7.3 Subscription Interface (Function Code Mode)

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

### 7.4 Algorithm Switch Interface (Dual Strategy Mode)

```c
uart_proto_status_t pf_strategy_algo(
    uart_proto_t *const self,
    parse_algo_t *const algo
);
```

## 8. Error Handling

### 8.1 Parse Error Types

| Error Type | Handling Strategy |
|-----------|------------------|
| `ALGO_ING` | Parsing incomplete, wait for more data |
| `ALGO_ERR_LENGTH_INVALID` | Invalid length, skip current data |
| `ALGO_ERR_CRC` | CRC check failed, skip current frame |
| `ALGO_ERR_NOICE` | Noise data, continue parsing |
| `ALGO_ERR_OTHERS` | Serious error, discard all data |

### 8.2 State Reset

```c
void reset_rx_state(uart_proto_t *const self);
```

**Usage Scenarios**:
- DMA error
- Buffer overflow
- Continuous parse failures

## 9. Configuration Options

### 9.1 Compile-Time Configuration

```c
// Mode selection
#define UART_PROTO_MODE_DEFAULT  UART_PROTO_MODE_FUNCTION_CODE

// Performance parameters
#define NUM_NOTIFY_ISR_CB_CALL          3   // ISR callback retry count
#define MAX_PARSE_NUM_ONCE_TRIGGER      10  // Max frames per trigger

// Thread configuration
#define PARSE_THREAD_PRIORITY           16
#define PARSE_THREAD_STACK_DEPTH        0x200

// Optional features
#define CUSTOM_RX_THREAD_ATT            1   // Custom thread attributes
#define CUSTOM_UART_PROTO_CONFIG        1   // Custom protocol config
```

### 9.2 Runtime Configuration

Via `uart_proto_config_t` structure:
```c
typedef struct {
    uint8_t num_notify_isr_cb_call;
    uint8_t max_parse_num_once_trigger;
} uart_proto_config_t;
```

## 10. Typical Use Cases

### 10.1 Modbus RTU Protocol

**Configuration**: Function Code Mode
```c
// Subscribe to Read Holding Registers (0x03)
subscribe_para_t para = {
    .fun_code = 0x03,
    .cb = modbus_read_holding_register_cb,
    .arg = &my_context
};
self->pf_subscribe(self, &para, &handle);
```

### 10.2 AT Command Parsing

**Configuration**: Transparent Mode
```c
// Set transparent callback
transparent_algo_t algo = {
    .pf_transparent_parse = at_command_parser,
    .arg = &at_context
};
```

### 10.3 Multi-Protocol Compatible

**Configuration**: Dual Strategy Mode
```c
// Runtime protocol switching
if (detect_protocol_type() == MODBUS) {
    self->pf_strategy_algo(self, &modbus_algo);
} else {
    self->pf_strategy_algo(self, &transparent_algo);
}
```

## 11. Performance Optimization

### 11.1 Zero-Copy Optimization

```c
#define NON_COPY_WHEN_NON_WRAP
```
- Non-wrapped data parsed directly in DMA buffer
- Reduce memory copy overhead

### 11.2 Batch Parsing

- Single interrupt can parse multiple complete frames
- Limited by `MAX_PARSE_NUM_ONCE_TRIGGER`
- Prevent excessive interrupt processing time

### 11.3 Sorted List

- Subscription list sorted by function code
- Speed up search
- Support binary search optimization (extensible)

## 12. Dependencies

```
uart_proto.c/h
    ©À©¤©¤ t_list.h              (Function code mode)
    ©À©¤©¤ os_interface          (Thread, queue, critical section)
    ©À©¤©¤ uart_ops              (HW init, counter read/write)
    ©¸©¤©¤ parse_algo            (Frame parsing algorithm)
```

## 13. Porting Guide

### 13.1 Required Interfaces

1. **OS Interface**:
   - Thread create/delete
   - Queue create/put/get
   - Critical section enter/exit

2. **UART Interface**:
   - Hardware initialization
   - DMA counter read/write

3. **Parsing Algorithm**:
   - Function code parse function
   - Or transparent callback function

### 13.2 Porting Steps

1. Implement `uart_rx_os_interface_t` interface
2. Implement `uart_ops_t` interface
3. Write frame parsing algorithm
4. Call `notify_isr_cb()` in interrupt service routine
5. Initialize protocol layer instance

---

**Version**: 1.02  
**Author**: Donzel  
**Date**: 2025-12-30
