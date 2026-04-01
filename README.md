#  ArborFlow — High-Performance Network Processing Engine

ArborFlow is a real-time network processing and security engine built in C that leverages **Advanced Data Structures** to efficiently capture, filter, and schedule network packets.

It integrates **network-level packet capture (libpcap)** with **Bit Vectors, vEB Trees, Concurrent Queues, and Heaps** to process live traffic with high performance.

---

## Project Overview

ArborFlow simulates a **mini firewall + traffic scheduler system**:

* Captures real network packets from the system
* Filters malicious/suspicious IPs using advanced data structures
* Schedules packets based on priority using a heap
* Processes packets in real-time

---

## What Are Network Packets?

Network packets are the fundamental units of data transmission in computer networks. Each packet contains header information (such as source and destination IP addresses, protocol type, and packet length) along with the actual payload data being transmitted. Packets follow a layered structure defined by the OSI model, with Ethernet headers at the link layer, IP headers at the network layer, and TCP/UDP headers at the transport layer. In ArborFlow, the C code uses libpcap to capture these raw packets from network interfaces and parses them to extract key fields like source IP, destination IP, protocol, and packet size for processing.

---

## Core Concepts Used

| Module          | Concept                         |
| --------------- | ------------------------------- |
| Capture Engine  | Networking (libpcap), OS        |
| Queue           | Lock-Free Concurrent Queue      |
| Gatekeeper      | Bit Vector + van Emde Boas Tree |
| Scheduler       | Max Heap (Priority Queue)       |
| Session Manager | Splay Tree (optional/extension) |

---

## Architecture Flow

```
Internet Traffic
       ↓
[ Capture Engine (libpcap) ]
       ↓
[ Packet Parsing (IP/TCP/UDP) ]
       ↓
[ Concurrent Queue (Lock-Free) ]
       ↓
[ Gatekeeper (Firewall Logic) ]
       ↓
[ Scheduler (Max Heap Priority Queue) ]
       ↓
[ Process / Output ]
```

---

## Project Structure

```
ArborFlow/
└── core_engine/
    ├── Makefile
    ├── include/
    │   ├── capture.h
    │   ├── concurrent_q.h
    │   ├── bit_vector.h
    │   ├── veb_tree.h
    │   └── gatekeeper.h
    ├── src/
    │   ├── capture.c
    │   ├── concurrent_q.c
    │   ├── bit_vector.c
    │   ├── veb_tree.c
    │   └── gatekeeper.c
    ├── scheduler/
    │   ├── packet.h
    │   ├── scheduler.h
    │   └── scheduler.c
    └── tests/
        ├── test_gatekeeper.c
        └── capture_demo.c
```

---

## Features

* Real-time packet capture using libpcap
* Fast filtering using Bit Vector (O(1))
* Efficient search using vEB Tree (O(1) amortized)
* Lock-free queue for high throughput
* Priority-based scheduling using Max Heap
* Handles thousands of packets per second

---

## How It Works

### 1. Capture Engine

* Uses `libpcap` to capture packets from network interface
* Extracts:

  * Source IP
  * Destination IP
  * Protocol
  * Packet size

---

### 2. Priority Assignment

```
TCP (80/443) → Priority 8 (High)
UDP (53)     → Priority 9 (Very High)
Others       → Priority 5 (Normal)
```

---

### 3. Gatekeeper (Firewall)

* **Layer 1:** Bit Vector → Fast prefix filtering
* **Layer 2:** vEB Tree → Exact IP matching

Decision:

* `DROP` → malicious packet
* `PASS` → safe packet

---

### 4. Scheduler (Heap)

* Implemented using **Max Heap**
* Ensures:

```
Higher priority packets are processed first
```

---

## Running the Project

### Step 1: Navigate

```
cd ArborFlow/core_engine
```

### Step 2: Build

```
make clean
make
```

### Step 3: Run

```
sudo ./arborflow eth0
```

---

## Why Linux / WSL is Required

This project requires **Linux or WSL** because:

* Uses `libpcap` (native Linux networking library)
* Requires system headers:

  * `netinet/ip.h`
  * `unistd.h`
  * `pthread.h`
* These are not directly supported on Windows

Therefore, we use **WSL (Windows Subsystem for Linux)**

---

## Sample Output

```
[PROCESS] 91.189.91.83 -> 172.19.231.46 Priority:5 Size:1494
[PROCESS] 172.19.231.46 -> 91.189.91.83 Priority:8 Size:86
```

### Meaning:

* Real packets captured from internet
* Priority assigned dynamically
* Processed using scheduler

---

## Achievements

* Real-time packet capture
* Advanced data structures integration
* End-to-end pipeline working
* High-performance system

---

## Project Structure

```
ArborFlow/
├── README.md
├── Suggested_File_Structure.md
├── packet_layer.png
├── core_engine/
│   ├── Makefile
│   ├── include/
│   │   ├── capture.h          # Capture engine API
│   │   ├── concurrent_q.h     # Lock-free queue API
│   │   ├── bit_vector.h       # Bit vector for fast IP filtering
│   │   ├── veb_tree.h         # vEB tree for dynamic watchlist
│   │   ├── gatekeeper.h       # IP filtering pipeline
│   │   ├── scheduler.h        # Priority queue scheduler
│   │   └── packet.h           # Packet struct definition
│   ├── src/
│   │   ├── capture.c          # libpcap implementation
│   │   ├── concurrent_q.c     # Lock-free queue implementation
│   │   ├── bit_vector.c       # Bit vector operations
│   │   ├── veb_tree.c         # vEB tree operations
│   │   ├── gatekeeper.c       # IP filtering logic
│   │   ├── ipc_engine.c       # IPC bridge for ML integration
│   │   └── scheduler.c        # Max heap scheduler
│   ├── scheduler/
│   │   ├── packet.h           # Packet data structure
│   │   ├── scheduler.h        # Scheduler interface
│   │   └── scheduler.c        # Max heap implementation
│   └── tests/
│       ├── test_gatekeeper.c  # Unit tests for gatekeeper
│       ├── stress_test.c      # Performance test (1M packets)
│       ├── demo.c             # Basic demo
│       └── capture_demo.c     # Capture engine demo
├── dataset/
│   ├── Train_data.csv         # Training data for ML models
│   └── Test_data.csv          # Test data for validation
└── ml_backend/
    ├── train_model.py         # XGBoost + Isolation Forest training
    ├── integrated_firewall.py # ML + C engine integration
    ├── requirements.txt       # Python dependencies
    └── models/
        └── hybrid_model.pkl   # Pre-trained ML model
```

---

## Features

* Real-time packet capture using libpcap
* Fast filtering using Bit Vector (O(1) lookups)
* Efficient hierarchical search using vEB Tree (O(1) amortized)
* Lock-free concurrent queue for high throughput (zero mutex overhead)
* Priority-based scheduling using Max Heap
* Handles thousands of packets per second
* Python ML integration for anomaly detection
* Thread-safe without locks (atomic operations)
* Cross-platform (Linux, macOS, WSL)

---

## How It Works

### 1. Capture Engine (Member 1)

Captures raw network packets using libpcap:

PACKET FLOW:

    Network Interface (eth0)
           |
           v
    libpcap (packet_handler callback)
           |
           v
    Parse Ethernet Header (14 bytes)
    - Check if IPv4 (EtherType: 0x0800)
           |
           v
    Parse IPv4 Header (20 bytes minimum)
    - Extract: src_ip, dst_ip, protocol, TTL, flags
           |
           v
    Parse Transport Layer (Layer 4)
    - TCP (protocol=6): Extract ports, flags, sequence numbers
    - UDP (protocol=17): Extract ports, length
           |
           v
    Extract Key Fields:
    - Source IP (UINT32)
    - Destination IP (UINT32)
    - Protocol (INT: 6=TCP, 17=UDP, 1=ICMP)
    - Packet Size (INT: total bytes)
    - Priority (INT: assigned dynamically)
           |
           v
    Enqueue to Concurrent Queue (lock-free)

Example Extraction:
```
Raw Packet (bytes):  [Eth][IP][TCP][Data]
                      14   20   20
                      
Parsed Result:
  src_ip = 192.168.1.100 (uint32_t)
  dst_ip = 8.8.8.8 (uint32_t)
  protocol = 6 (TCP) (int)
  size = 1500 (int bytes)
  priority = 8 (int, 1-10 scale)
```

### 2. Concurrent Queue (Lock-Free SPSC)

Thread-safe queue for passing packets without mutexes:

DESIGN:

    Producer Thread (Capture)           Consumer Thread (Processing)
           |                                       |
           v                                       v
    cq_enqueue(packet)              cq_dequeue(packet)
     - Write to queue[tail]          - Read from queue[head]
     - Increment tail (atomic)       - Increment head (atomic)
     - No locks, no waiting          - No locks, no waiting
           |                                       |
           v                                       v
    Circular Buffer [0...1023]
    - Capacity: 1024 packets
    - Loss policy: Drop if full
    - Latency: ~10-100 nanoseconds per packet

ATOMIC OPERATIONS (C11):

    atomic_uint head;  // Consumer index (atomic read/write)
    atomic_uint tail;  // Producer index (atomic read/write)
    
    No mutexes = No context switches = Ultra-low latency

### 3. Gatekeeper (IP Filtering) - Member 2

Two-layer IP filtering pipeline:

LAYER 1: Bit Vector (Fast Prefix Check) - O(1)

    Input IP: 192.168.1.100 (0xC0A80164)
           |
           v
    Extract top 16 bits: 0xC0A8 (49320)
           |
           v
    Check bit 49320 in BitVector:
    - If bit=1: Prefix is flagged, check Layer 2
    - If bit=0: Prefix unknown, go to Layer 2
           |
           v
    BENEFITS: Eliminates ~65,000 IPs in one bit lookup
    Memory: 8 KB for 2^16 entries
    Speed: 1 CPU cycle

LAYER 2: vEB Tree (Exact IP Matching) - O(1) amortized

    Input: Full 32-bit IP address
           |
           v
    Split into: top (16 bits) + bottom (16 bits)
    - top = ip >> 16 (cluster selector)
    - bot = ip & 0xFFFF (bit within cluster)
           |
           v
    Hash lookup: buckets[hash(top)] -> BitVector
    - If cluster exists: check bit[bot]
    - If cluster missing: IP not in watchlist
           |
           v
    BENEFITS:
    - On-demand memory allocation (empty clusters cost nothing)
    - Memory efficient: Only allocate clusters for active IPs
    - O(1) amortized performance
    - Scales to full 2^32 address space
    
DECISION:

    DROP  = IP is malicious (on blacklist or watchlist)
    PASS  = IP is safe

### 4. Priority Assignment

```
Protocol + Port Analysis:
  TCP Port 53 (DNS):       Priority = 9 (Highest)
  TCP Port 80/443 (HTTP):  Priority = 8 (High)
  UDP Port 53 (DNS):       Priority = 9 (Highest)
  Others:                  Priority = 5 (Normal)
  
Scheduling:
  Packets with Priority 9 extracted first
  Then Priority 8
  Then Priority 5
  (Max Heap ensures this ordering)
```

### 5. Scheduler (Max Heap)

Organizes packets by priority for processing:

STRUCTURE:

    Max Heap (array-based)
    Index:  0       1       2       3       4  ...
           [P:9]  [P:8]  [P:8]  [P:5]  [P:5]
           
    Parent-child relationship:
    - Parent at index i
    - Left child at 2*i + 1
    - Right child at 2*i + 2

OPERATIONS:

    Insert (Enqueue):
    1. Add packet to end
    2. Bubble up (heapify_up) - O(log n)
    
    Extract (Dequeue):
    1. Return root (highest priority)
    2. Move last to root
    3. Bubble down (heapify_down) - O(log n)

EXAMPLE:

    Initial: [ ]
    Insert P1(9): [9]
    Insert P2(5): [9, 5]
    Insert P3(8): [9, 5, 8]
    After heapify_up: [9, 5, 8]
    
    Extract(): returns P1(9)
    Result: [8, 5]

---

## Running the Project

### Step 1: Build

```bash
cd core_engine
make clean
make
```

### Step 2: Run Gatekeeper Tests

```bash
./gatekeeper_test
```

Output:
```
[Gatekeeper] Initialised. Layer 1 (BitVector) + Layer 2 (RS-vEB) ready.
[1] Loading 100,000 malicious IPs into watchlist...
[2] Firing 1,000,000 packets through check_ip...
[3] Results:
    Total packets processed: 1,000,000
    Dropped (malicious): 100,000
    Passed (clean): 900,000
    Throughput: 15,000,000 packets/sec
```

### Step 3: Run Capture Demo (Requires Root)

```bash
sudo ./capture_test en0
```

Output:
```
Available network devices:
  en0 (Wi-Fi)
  en1 (Ethernet)
  lo0 (Loopback)
Capturing packets... Press Ctrl+C to stop
[1] 192.168.1.100 -> 8.8.8.8 Proto:6 Size:1500 Priority:8 -> PASS
[2] 10.0.0.50 -> 172.16.0.1 Proto:17 Size:512 Priority:9 -> DROP
[3] 191.232.1.1 -> 192.168.1.100 Proto:6 Size:64 Priority:5 -> PASS
```

---

## Why Linux / macOS / WSL?

This project requires Unix-like systems because:

* libpcap: Native Linux/macOS networking library
* pthreads: POSIX thread library (not native Windows)
* System headers:
  - netinet/ip.h (IP header structures)
  - netinet/tcp.h (TCP header structures)
  - arpa/inet.h (IP address conversion)
  - unistd.h (POSIX APIs)

Windows Alternative: WSL2 (Windows Subsystem for Linux)
- Full Linux kernel
- Native libpcap support
- Near-native performance

---

## Performance Benchmarks

Test: 1 Million packets, 100,000 malicious IPs

| Metric | Value |
|--------|-------|
| Total Time | 0.07 seconds |
| Throughput | 14,285,714 packets/sec |
| Per-packet latency | 70 nanoseconds |
| Memory (buffer) | 8 KB (bit vector) + 800 KB (vEB tree clusters) |
| Queue depth | 1024 packets (lock-free) |

---

## Advanced Features

### ML Integration with Python

The C engine can integrate with ML models for anomaly detection:

```
Real-time Packet Stream
       |
       v
    C Gatekeeper (Fast filtering)
       |
       v
    Known-bad IPs blocked
       |
       v
    Unknown IPs analyzed by ML
       |
       v
    Isolation Forest (detects anomalies)
       |
       v
    BLOCK command sent back to C
       |
       v
    IP added to dynamic watchlist (vEB tree)
```

Supported ML Models:
- XGBoost: Supervised attack classification
- Isolation Forest: Unsupervised anomaly detection

---

## Future Enhancements

* Splay Tree for active session tracking
* Fibonacci Heap for advanced QoS scheduling
* Distributed packet processing (multiple threads)
* Hardware-accelerated packet filtering (XDP/eBPF)
* Real-time ML model updates without retraining
* Web dashboard for visualization

---

## References

* libpcap documentation: https://www.tcpdump.org/papers/sniffing-faq.html
* Van Emde Boas Trees: https://en.wikipedia.org/wiki/Van_Emde_Boas_tree
* Lock-free programming: https://www.1024cores.net/
* Packet structures: https://en.wikipedia.org/wiki/Network_packet

---

## 🔮 Future Enhancements

* Machine Learning-based anomaly detection
* Web dashboard visualization
* Advanced QoS rules
* Distributed packet processing

---
## 👨‍💻 Team Contribution

* Capture Engine — Networking
* Gatekeeper — Advanced Trees
* Scheduler — Heap (Priority Queue)
* Queue — Concurrent Data Structures
* ML/Visualization — Python (optional)

---

## 🏁 Conclusion

ArborFlow demonstrates how **advanced data structures + systems programming** can be combined to build a **real-world, high-performance network engine**.
