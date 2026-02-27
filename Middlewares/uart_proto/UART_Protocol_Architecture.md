# UART Protocol Layer Architecture

## 1. Overview

The UART protocol layer is a configurable asynchronous serial protocol processing framework that supports three operating modes:
- **Function Code Mode**: Frame parsing and callback dispatch based on function codes
- **Transparent Mode**: Raw data transparent transmission
- **Dual Strategy Mode**: Runtime dynamic switching between the above two modes

## 2. Overall Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    UART Protocol Layer Instance                  │
│                      (uart_proto_t)                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────┐    │
│  │ Input Args   │  │ Private Data │  │  Public API       │    │
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
        │OS Interface│  │UART Ops  │   │Parse Algo│
        └──────────┘   └──────────┘   └──────────┘
```

## 3. Core Components

### 3.1 Input Arguments (uart_proto_input_arg_t)

```
uart_proto_input_arg_t
├── frame_parse_att         // Frame parsing attributes
│   ├── recv_buf_att        // Receive buffer configuration
│   │   ├── recv_buf        // Buffer pointer
│   │   └── buffer_size     // Buffer size
│   └── parse_algo          // Parsing algorithm
│       ├── algo_type       // Algorithm type (dual mode)
│       └── u               // Algorithm union
│           ├── funcoude_algo     // Function code algorithm
│           └── transparent_algo  // Transparent algorithm
├── uart_ops                // Hardware operation interface
│   ├── pf_uart_init()
│   ├── pf_get_counter()
│   └── pf_set_counter()
├── os_interface            // Operating system interface
│   ├── pf_os_thread_create()
│   ├── pf_os_queue_create()
│   ├── pf_os_enter_critical()
│   └── pf_os_exit_critical()
├── thread_att              // Thread attributes (optional)
└── uart_proto_config       // Protocol configuration (optional)
```

### 3.2 Private Data (uart_proto_priv_data_t)

```
uart_proto_priv_data_t
├── is_inited                    // Initialization flag
├── num_notify_isr_cb_call       // ISR notification count
├── parse_fail_cnt               // Parse failure count
├── queue_handle                 // Queue handle
├── data_counter                 // Data counter
├── header                       // Header index
├── tail                         // Tail index
├── parse_buf                    // Parse buffer
└── funcode_sentinel             // Function code linked list sentinel (function code mode)
```

## 4. Operating Modes

### 4.1 Function Code Mode (UART_PROTO_MODE_FUNCTION_CODE)

```
Receive Data → Frame Parsing → Extract Function Code → Find Subscribers → Callback Notification
```

**Features**:
- Supports multiple subscribers for different function codes
- Uses ordered linked list to manage subscription relationships
- Automatic multi-frame parsing until buffer is exhausted

**Subscription Mechanism**:
```
Subscriber A (Function Code 0x01)  ───┐
Subscriber B (Function Code 0x03)  ───┼──→ Ordered Linked List
Subscriber C (Function Code 0x05)  ───┘
```

### 4.2 Transparent Mode (UART_PROTO_MODE_TRANSPARENT)

```
Receive Data → Direct Enqueue → Transparent Callback
```

**Features**:
- No frame structure parsing
- Raw data passed directly
- Suitable for custom protocols or streaming data

### 4.3 Dual Strategy Mode (UART_PROTO_MODE_DUAL_STRATEGY)

```
Receive Data → Check Current Algorithm Type → Function Code Parsing / Transparent Processing
                            ↑
                   Runtime Switchable
```

**Features**:
- Runtime dynamic algorithm switching
- Switch via `pf_strategy_algo()` interface
- Contains both function code subscription mechanism and transparent interface

## 5. Data Flow Diagram

### 5.1 Receive Flow

```
              ╔═══════════════╗
              ║ UART DMA IRQ  ║
              ╚═══════════════╝
                      │
                      ▼
         ┌────────────────────────┐
         │   notify_isr_cb()      │ ←─── Called from ISR context
         │  1. Calculate ring buffer range   │
         │  2. Handle wraparound situations  │
         │  3. Call parsing function         │
         └────────────────────────┘
                      │
         ┌────────────┴────────────┐
         │                         │
         ▼                         ▼
  ┏━━━━━━━━━━━━┓          ┏━━━━━━━━━━━━┓
  ┃Function Code┃          ┃ Transparent┃
  ┃   Parsing   ┃          ┃  Parsing   ┃
  ┗━━━━━━━━━━━━┛          ┗━━━━━━━━━━━━┛
         │                         │
         └────────────┬────────────┘
                      ▼
              ╔═══════════════╗
              ║ Message Queue ║
              ╚═══════════════╝
                      │
                      ▼
         ┌────────────────────────┐
         │   parse_thread()       │ ←─── Background thread
         │  1. Retrieve data from queue  │
         │  2. Call subscriber callbacks │
         └────────────────────────┘
                      │
                      ▼
              ┌──────────────┐
              │ User Callback│
              └──────────────┘
```

### 5.2 Ring Buffer Management

```
DMA Receive Buffer (Ring):
┌───────────────────────────────────────┐
│  #########-------########             │
│  ↑        ↑       ↑                   │
│  Used     tail    DMA write position  │
└───────────────────────────────────────┘
  
  header: Total received bytes (cumulative)
  tail:   Processed bytes (cumulative)
  data_counter: DMA last stop position
```

**Wraparound Handling**:
- If data crosses buffer boundary, copy to `parse_buf`
- If data is in contiguous region, process directly (`NON_COPY_WHEN_NON_WRAP`)

## 6. Thread Model

```
┌─────────────────┐
│  Parse Thread   │  Priority: PARSE_THREAD_PRIORITY (16)
│ parse_thread()  │  Stack Depth: PARSE_THREAD_STACK_DEPTH (0x200)
└─────────────────┘
        │
        │ Blocked on message queue
        ▼
    ┌────────┐
    │Queue Wait│
    └────────┘
        │
        │ Receive parse info
        ▼
    ┌────────┐
    │Dispatch│
    │Callback│
    └────────┘
```

**Thread Safety**:
- ISR and thread decoupled via message queue
- Critical section protection for shared variables (header, tail, counter)
- Subscription linked list operations use critical sections

## 7. API Interface

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
6. Initialize subscription linked list (function code mode)

### 7.2 ISR Callback Interface

```c
void notify_isr_cb(uart_proto_t *const self);
```

**Function**: DMA receive complete interrupt callback
**Invocation Timing**:
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

### 7.4 Algorithm Switching Interface (Dual Strategy Mode)

```c
uart_proto_status_t pf_strategy_algo(
    uart_proto_t *const self,
    parse_algo_t *const algo
);
```

## 8. Error Handling

### 8.1 Parse Error Types

| Error Type | Handling Strategy |
|-----------|-------------------|
| `ALGO_ING` | Parsing incomplete, wait for more data |
| `ALGO_ERR_LENGTH_INVALID` | Invalid length, skip current data |
| `ALGO_ERR_CRC` | CRC check failed, skip current frame |
| `ALGO_ERR_NOICE` | Noise data, continue parsing |
| `ALGO_ERR_OTHERS` | Severe error, discard all data |

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
// Operating mode selection
#define UART_PROTO_MODE_DEFAULT  UART_PROTO_MODE_FUNCTION_CODE

// Performance parameters
#define NUM_NOTIFY_ISR_CB_CALL          3   // ISR callback retry count
#define MAX_PARSE_NUM_ONCE_TRIGGER      10  // Max frames parsed per trigger

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
// Subscribe to read holding registers (0x03)
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

### 10.3 Multi-Protocol Compatibility

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
- Reduces memory copy overhead

### 11.2 Batch Parsing

- Single interrupt can parse multiple complete frames
- Limited by `MAX_PARSE_NUM_ONCE_TRIGGER` to prevent excessive processing
- Prevents interrupt processing time from being too long

### 11.3 Ordered Linked List

- Subscription list sorted by function code
- Speeds up lookup
- Supports binary search optimization (extensible)

## 12. Dependencies

```
uart_proto.c/h
    ├── t_list.h              (Function code mode)
    ├── os_interface          (Thread, queue, critical section)
    ├── uart_ops              (Hardware init, counter read/write)
    └── parse_algo            (Frame parsing algorithm)
```

## 13. Porting Guide

### 13.1 Required Interfaces

1. **OS Interface**:
   - Thread create/delete
   - Queue create/send/receive
   - Critical section enter/exit

2. **UART Interface**:
   - Hardware initialization
   - DMA counter read/write

3. **Parsing Algorithm**:
   - Function code parsing function
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
