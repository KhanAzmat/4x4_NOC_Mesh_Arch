# Mesh NoC Platform (4×4)

Quick‑start (Linux / macOS):

```bash
git clone <repo> mesh_noc_platform   # or copy this folder
cd mesh_noc_platform
make run                 # build & run full HAL test‑suite
make clean               # remove objects
```

Environment options:

* `TEST=<basic|performance|stress>` – run subset of tests  
* `TRACE=1` – verbose NoC packet trace  
* `HAL=<shared.so>` – load external HAL implementation  

This is a **pure C11** software model (no RTL) that simulates eight RISC‑V tiles
and eight 512‑bit DMEM modules arranged in a 4 × 4 mesh.  
It is intended as a reference platform for validating AI‑generated HAL/driver
code that moves data via on‑chip DMA engines and the mesh NoC.


Industry Standard Architecture : 

┌─────────────────────────────────────────────────────────┐
│                 Application Layer                       │
│  • Tests, User Applications, Business Logic             │
└─────────────────────────────────────────────────────────┘
                              │ Function Calls
┌─────────────────────────────────────────────────────────┐
│            Hardware Abstraction Layer (HAL)             │
│  • Platform-independent API                             │
│  • Address validation & translation                     │  
│  • Error handling & standardization                     │
│  • Thread safety & resource management                  │
└─────────────────────────────────────────────────────────┘
                              │ Driver Calls
┌─────────────────────────────────────────────────────────┐
│                  Driver Layer                           │
│  • Hardware-specific implementation                     │
│  • Register manipulation                                │
│  • Hardware protocol handling                           │
│  • Base hardware interface                              │
└─────────────────────────────────────────────────────────┘
                              │ Hardware Interface
┌─────────────────────────────────────────────────────────┐
│                Physical Hardware                        │
│  • Registers, Peripherals, Memory                       │
└─────────────────────────────────────────────────────────┘



┌────────────────────────────────────────────────────────────────────────────────────┐
│                           4x4 Mesh NoC Platform Architecture                       │
├────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                    │
│  ┌───────────────┐    ┌─────────────────────────────────────────────────────────┐  │
│  │   C0 Master   │    │                 8-Tile Mesh                             │  │
│  │  (Main Thread)│    │                                                         │  │
│  │               │    │  [T1]──[T2]----[T3]──[T4]---[T5]──[T6]----[T7]          │  │
│  │ ┌───────────┐ │    │   │     │       │     │       │     │       │           │  │
│  │ │Interrupt  │ │    │   │     │       │     │       │     │       │           │  │
│  │ │Controller │ │    │   │     │       │     │       │     │       │           │  │
│  │ │  (64 IRQ) │ │    │   │     │       │     │       │     │       │           │  │
│  │ └───────────┘ │    │   │     │       │     │       │     │       │           │  │
│  │               │    │   │     │       │     │       │     │       │           │  │
│  │ ┌───────────┐ │    │  [D0]──[D1]----[D2]──[D3]----[D4]──[D5]----[D6]──[D7]   │  │
│  │ │Task Queue │ │    │                                                         │  │
│  │ │ Manager   │ │    │  T = Tile (Processor Thread)    D = DMEM Module         │  │
│  │ │(64 tasks) │ │    │                                                         │  │
│  │ └───────────┘ │    └─────────────────────────────────────────────────────────┘  │
│  └───────────────┘                                                                 │
└────────────────────────────────────────────────────────────────────────────────────┘




┌───────────────────────────────────────────────────────────────────────────────────┐
│                             Software Stack                                        │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────────────┐  │
│  │                            Test Layer                                       │  │
│  │  • CPU Local Move    • DMA Local Transfer    • DMA Remote Transfer          │  │
│  │  • NoC Bandwidth     • NoC Latency           • Random DMA Remote            │  │
│  │  • C0 Gather         • C0 Distribute         • Parallel C0 Access           │  │
│  └─────────────────────────────────────────────────────────────────────────────┘  │
│                                      │ g_hal.*                                    │
│  ┌─────────────────────────────────────────────────────────────────────────────┐  │
│  │                            HAL Layer                                        │  │
│  │  • hal_cpu_local_move()      • hal_dma_local_transfer()                     │  │
│  │  • hal_dma_remote_transfer() • hal_memory_read/write/fill/set()             │  │
│  │  • hal_mesh_route_optimal()  • hal_get_dmem_status()                        │  │
│  └─────────────────────────────────────────────────────────────────────────────┘  │
│                                      │ Driver Calls                               │
│  ┌─────────────────────────────────────────────────────────────────────────────┐  │
│  │                           Driver Layer                                      │  │
│  │  • Memory Driver (memmove)   • DMA Driver (dma_local_transfer)              │  │
│  │  • NoC Driver (noc_send_packet) • DMEM Driver (dmem_copy)                   │  │
│  │  • Address Manager (addr_to_ptr)                                            │  │
│  └─────────────────────────────────────────────────────────────────────────────┘  │
│                                      │ Hardware Interface                         │
│  ┌─────────────────────────────────────────────────────────────────────────────┐  │
│  │                        Hardware Simulation                                  │  │
│  │  • NoC Arbitration & Timing  • Memory Simulation  • Thread Coordination     │  │
│  │  • Interrupt Routing         • Task Scheduling    • Platform Control        │  │
│  └─────────────────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────────────────────┘



┌────────────────────────────────────────────────────────────────────────────────────┐
│                              Memory Address Map                                    │
├────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                    │
│  0x80000000  ┌─────────────────┐                                                   │
│              │  C0 Master      │  64 KiB                                           │
│              │  Control Region │                                                   │
│  0x80010000  └─────────────────┘                                                   │
│                                                                                    │
│  0x20C00000  ┌─────────────────┐                                                   │
│              │     DMEM7       │  256 KiB                                          │
│  0x20800000  ├─────────────────┤                                                   │
│              │     DMEM6       │  256 KiB                                          │
│  0x20400000  ├─────────────────┤                                                   │
│              │     DMEM5       │  256 KiB                                          │
│  0x20000000  ├─────────────────┤                                                   │
│              │     DMEM4       │  256 KiB                                          │
│  0x10C00000  ├─────────────────┤                                                   │
│              │     DMEM3       │  256 KiB                                          │
│  0x10800000  ├─────────────────┤                                                   │
│              │     DMEM2       │  256 KiB                                          │
│  0x10400000  ├─────────────────┤                                                   │
│              │     DMEM1       │  256 KiB                                          │
│  0x10000000  ├─────────────────┤                                                   │
│              │     DMEM0       │  256 KiB                                          │
│              └─────────────────┘                                                   │
│                                                                                    │
│  0x00700000  ┌─────────────────┐  Each Tile: 1MB stride                            │
│              │     TILE7       │  ┌─────────────┐                                  │
│  0x00600000  ├─────────────────┤  │ 0xF000: DMA │                                  │
│              │     TILE6       │  │ 0x8000: DLM1│ 128 KiB                          │
│  0x00500000  ├─────────────────┤  │ 0x0000: DLM │ 32 KiB                           │
│              │     TILE5       │  └─────────────┘                                  │
│  0x00400000  ├─────────────────┤                                                   │
│              │     TILE4       │                                                   │
│  0x00300000  ├─────────────────┤                                                   │
│              │     TILE3       │                                                   │
│  0x00200000  ├─────────────────┤                                                   │
│              │     TILE2       │                                                   │
│  0x00100000  ├─────────────────┤                                                   │
│              │     TILE1       │                                                   │
│  0x00000000  └─────────────────┘                                                   │
│              │     TILE0       │ (C0 Master - No Processor Thread)                 │
│              └─────────────────┘                                                   │
└────────────────────────────────────────────────────────────────────────────────────┘




┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                INTERRUPT SUBSYSTEM ARCHITECTURE                     │
│                              4x4 Mesh NoC Platform                                  │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                    TILE LAYER                                       │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────────┬───────────────┤
│   TILE 1    │   TILE 2    │   TILE 3    │   TILE 4    │   TILE 5    │   TILE 6-7    │
│ (Thread)    │ (Thread)    │ (Thread)    │ (Thread)    │ (Thread)    │   (Threads)   │
│             │             │             │             │             │               │
│ Interrupt   │ Interrupt   │ Interrupt   │ Interrupt   │ Interrupt   │ Interrupt     │
│ Triggers:   │ Triggers:   │ Triggers:   │ Triggers:   │ Triggers:   │ Triggers:     │
│ • Task Done │ • Task Done │ • Task Done │ • Task Done │ • Task Done │ • Task Done   │
│ • Error     │ • Error     │ • Error     │ • Error     │ • Error     │ • Error       │
│ • DMA Done  │ • DMA Done  │ • DMA Done  │ • DMA Done  │ • DMA Done  │ • DMA Done    │
│ • Shutdown  │ • Shutdown  │ • Shutdown  │ • Shutdown  │ • Shutdown  │ • Shutdown    │
│             │             │             │             │             │               │
│      │      │      │      │      │      │      │      │      │      │       │       │
└──────┼──────┴──────┼──────┴──────┼──────┴──────┼──────┴──────┼──────┴───────┼───────┘
       │             │             │             │             │              │
       │ IRQ Packets │ IRQ Packets │ IRQ Packets │ IRQ Packets │ IRQ Packets  │ IRQ Packets
       ▼             ▼             ▼             ▼             ▼              ▼
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                                  NoC MESH LAYER                                     │
│                                                                                     │
│  ┌───────────────────────────────────────────────────────────────────────────────┐  │
│  │                            MESH ROUTER                                        │  │
│  │                                                                               │  │
│  │  Packet Type Detection:                                                       │  │
│  │  ┌─────────────────┬──────────────────────────────────────────────────────┐   │  │
│  │  │ PKT_INTERRUPT_REQ│ → Highest Priority → Immediate Routing             │   │  │
│  │  │ PKT_INTERRUPT_ACK│ → High Priority → Immediate Routing                │   │  │
│  │  │ PKT_DMA_TRANSFER │ → Lower Priority → Arbitration + Queueing          │   │  │
│  │  └─────────────────┴──────────────────────────────────────────────────────┘   │  │
│  │                                                                               │  │
│  │  Functions:                                                                   │  │
│  │  • noc_send_packet()         ← Entry point for all packets                   │  │
│  │  • noc_handle_interrupt_packet() ← Dedicated interrupt routing               │  │
│  │  • No arbitration delays for interrupts (1μs routing only)                    │  │
│  │                                                                               │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                     │
│                                       │                                             │
│                               IRQ Packets to C0                                     │
│                                       ▼                                            │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────┐
│                            C0 MASTER INTERRUPT SUBSYSTEM                            │
│                                  (Main Thread)                                      │
│                                                                                     │
│ ┌─────────────────────────────────────────────────────────────────────────────────┐ │
│ │                         NoC INTERRUPT RECEIVER                                  │ │
│ │                                                                                 │ │
│ │  noc_handle_received_interrupt()                                                │ │
│ │  ├─── Validate interrupt packet                                                 │ │
│ │  ├─── Check queue capacity                                                      │ │
│ │  ├─── Enqueue to circular buffer                                                │ │
│ │  └─── Signal interrupt available                                                │ │
│ │                                                                                 │ │
│ └─────────────────────────────────────────────────────────────────────────────────┘ │
│                                       │                                             │
│                                       ▼                                             │
│ ┌─────────────────────────────────────────────────────────────────────────────────┐ │
│ │                     INTERRUPT CONTROLLER                                        │ │
│ │                                                                                 │ │
│ │  ┌───────────────────────────────────────────────────────────────────────────┐  │ │
│ │  │                    CIRCULAR BUFFER QUEUE                                  │  │ │
│ │  │                   (64 pending interrupts max)                             │  │ │
│ │  │                                                                           │  │ │
│ │  │ [IRQ1][IRQ2][IRQ3][    ][    ][    ]...[    ][    ]                       │  │ │
│ │  │   ▲                                                   ▲                                   │  │ │
│ │  │  Head                                               Tail                  │  │ │
│ │  │                                                                           │  │ │
│ │  │ Thread-Safe Operations:                                                   │  │ │
│ │  │ • pthread_mutex_t irq_lock                                                │  │ │
│ │  │ • pthread_cond_t irq_available                                            │  │ │
│ │  │ • Drop policy when full                                                   │  │ │
│ │  └───────────────────────────────────────────────────────────────────────────┘  │ │
│ │                                                                                 │ │
│ │  Statistics:                                                                    │ │
│ │  • interrupts_received     • interrupts_processed     • interrupts_dropped      │ │
│ │                                                                                 │ │
│ └─────────────────────────────────────────────────────────────────────────────────┘ │
│                                       │                                             │
│                                       ▼                                             │
│ ┌─────────────────────────────────────────────────────────────────────────────────┐ │
│ │                    INTERRUPT PROCESSING ENGINE                                  │ │
│ │                                                                                 │ │
│ │  c0_process_pending_interrupts()                                                │ │
│ │  ├─── Dequeue interrupt from buffer                                             │ │
│ │  ├─── Lookup handler by interrupt type                                          │ │
│ │  ├─── Call appropriate ISR                                                      │ │
│ │  └─── Update statistics                                                         │ │
│ │                                                                                 │ │
│ └─────────────────────────────────────────────────────────────────────────────────┘ │
│                                       │                                             │
│                                       ▼                                             │
│ ┌─────────────────────────────────────────────────────────────────────────────────┐ │
│ │                     ISR DISPATCH TABLE                                          │ │
│ │                                                                                 │ │
│ │  isr_handlers[IRQ_TYPE_MAX+1] = {                                               │ │
│ │    [IRQ_TYPE_TASK_COMPLETE]    → default_task_complete_handler()               │ │
│ │    [IRQ_TYPE_ERROR]            → default_error_handler()                       │ │
│ │    [IRQ_TYPE_DMA_COMPLETE]     → default_dma_complete_handler()                │ │
│ │    [IRQ_TYPE_RESOURCE_REQUEST] → default_resource_request_handler()            │ │
│ │    [IRQ_TYPE_SHUTDOWN]         → default_shutdown_handler()                    │ │
│ │  }                                                                              │ │
│ │                                                                                 │ │
│ └─────────────────────────────────────────────────────────────────────────────────┘ │
│                                       │                                             │
│                                       ▼                                             │
│ ┌─────────────────────────────────────────────────────────────────────────────────┐ │
│ │                        ISR EXECUTION                                            │ │
│ │                                                                                 │ │
│ │  Each ISR receives:                                                             │ │
│ │  • interrupt_request_t* irq (source tile, type, data, message)                  │ │
│ │  • void* platform_context (access to platform state)                            │ │
│ │                                                                                 │ │
│ │  ISR Actions:                                                                   │ │
│ │  • Update platform counters (active_tasks--, completed_tasks++)                 │ │
│ │  • Log event details                                                            │ │
│ │  • Trigger system responses (error recovery, resource allocation)               │ │
│ │  • Return success/failure status                                                │ │
│ │                                                                                 │ │
│ └─────────────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                              INTERRUPT FLOW SUMMARY                                     │
│                                                                                         │
│  1. TRIGGER:     Tile completes task → tile_signal_task_complete()                      │ 
│  2. PACKAGING:   Create interrupt_request_t → noc_send_interrupt_packet()               │
│  3. TRANSPORT:   NoC packet with PKT_INTERRUPT_REQ → mesh_router                        │
│  4. ROUTING:     Highest priority, immediate delivery → noc_handle_interrupt_packet()   │
│  5. RECEPTION:   Convert packet → interrupt_request_t → noc_handle_received_interrupt()│
│  6. QUEUEING:    Add to circular buffer → signal availability                           │
│  7. PROCESSING:  Dequeue → lookup handler → c0_process_pending_interrupts()            │
│  8. EXECUTION:   Call ISR → update platform state → log results                        │
│                                                                                         │
│                        CURRENT ACTIVE INTERRUPT SOURCES:                                │
│                        • Tiles 1-7: Task completion, Errors, Shutdown                   │
│                        • Future: DMA completion, Resource requests                      │
│                                                                                         │
│                        INTERRUPT PRIORITIES (High to Low):                              │
│                        1. Interrupt packets (no arbitration delay)                      │
│                        2. Data packets (arbitration + timing delays)                    │
│                                                                                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘


#Test Case Generation Flow (for DMEM) :

┌──────────────────────────────┐
│  HAL / Driver / memory_map   │
└───────────────┬──────────────┘
                │
                ▼
┌───────────────────────────────────────────────────────────┐
│  Cursor + Sonnet 4:                                       │
│   generates prompt for DMEM test-cases                    │
|          (generating_test_cases_DMEM.txt)                 │ 
└───────────────┬───────────────────────────────────────────┘
                │
                ▼
┌───────────────────────────────────────────────────────────┐
│  GPT o3 + prompt from step 2                              │
│  generates DMEM test-cases                                │
└───────────────────────────────────────────────────────────┘




