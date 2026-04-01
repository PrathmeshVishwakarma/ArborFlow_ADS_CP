#  ArborFlow — High-Performance Network Processing Engine

ArborFlow is a real-time network processing and security engine built in C that leverages **Advanced Data Structures** to efficiently capture, filter, and schedule network packets.

It integrates **network-level packet capture (libpcap)** with **Bit Vectors, vEB Trees, Concurrent Queues, and Heaps** to process live traffic with high performance.

---

## Project Overview

ArborFlow implements a **high-performance firewall and traffic scheduler system**:

* Captures real network packets from the system interface
* Filters malicious/suspicious IPs using advanced data structures
* Schedules packets based on priority using a heap
* Processes packets in real-time with minimal latency

**Key Performance Goals:**
- Process millions of packets per second
- O(1) IP filtering lookups
- Lock-free queue for zero contention
- Sub-microsecond per-packet latency

---

## What Are Network Packets?

### Definition

A **network packet** is a unit of data transmitted across a network. It contains:
- **Header Information** (metadata about the packet)
- **Payload** (the actual data being transmitted)

Packets follow a **layered structure** known as the **OSI Model**:

```
┌─────────────────────────────────────────────────┐
│  Application Layer (Layer 7)                    │
│  (HTTP, HTTPS, DNS, etc.)                       │
├─────────────────────────────────────────────────┤
│  Transport Layer (Layer 4)                      │
│  TCP / UDP Headers                              │
├─────────────────────────────────────────────────┤
│  Internet Layer (Layer 3)                       │
│  IP Headers (Source IP, Destination IP)         │
├─────────────────────────────────────────────────┤
│  Link Layer (Layer 2)                           │
│  Ethernet Header (Source MAC, Destination MAC)  │
├─────────────────────────────────────────────────┤
│  Physical Layer (Layer 1)                       │
│  Raw bits transmitted over wire                 │
└─────────────────────────────────────────────────┘
```

**Visual Packet Structure:**
```
Ethernet Frame: [Src MAC | Dst MAC | EtherType] 14 bytes
    |
    └─> IPv4 Header: [Src IP | Dst IP | Protocol | TTL | ...] 20 bytes (minimum)
            |
            └─> TCP/UDP Header: [Src Port | Dst Port | Seq | Ack | ...] 20/8 bytes
                    |
                    └─> Payload: [HTTP Data, DNS Query, etc.]
```

See **packet_layer.png** for visual representation of packet layers.

---

## How C Reads Network Packets

### 1. Capture with libpcap

```c
#include <pcap.h>

// Open network device for packet capture
pcap_t *handle = pcap_open_live("eth0", 65535, 1, 1000, errbuf);

// Set a filter to capture only IP packets
struct bpf_program fp;
pcap_compile(handle, &fp, "ip", 0, PCAP_NETMASK_UNKNOWN);
pcap_setfilter(handle, &fp);

// Start capturing packets (calls callback for each packet)
pcap_loop(handle, -1, packet_handler, (u_char *)user_data);
```

### 2. Packet Handler Callback

When `libpcap` captures a packet, it calls your handler function:

```c
void packet_handler(u_char *user, const struct pcap_pkthdr *header, 
                    const u_char *packet) {
    // header->len = total packet length in bytes
    // packet = raw bytes of the packet (including all headers)
}
```

### 3. C Data Types Used for Packet Reading

ArborFlow uses the following C data types and structures:

**Ethernet Layer (Layer 2):**
```c
struct ether_header {
    u_char  ether_dhost[ETHER_ADDR_LEN];  // Destination MAC (6 bytes)
    u_char  ether_shost[ETHER_ADDR_LEN];  // Source MAC (6 bytes)
    u_short ether_type;                    // Protocol: IPv4 (0x0800), etc. (2 bytes)
} __attribute__((packed));
// Total: 14 bytes
```

**IPv4 Header (Layer 3):**
```c
struct ip {
    u_char  ip_hl:4,           // Header length (4 bits)
            ip_v:4;             // Version (4 bits)
    u_char  ip_tos;             // Type of Service (1 byte)
    u_short ip_len;             // Total packet length (2 bytes)
    u_short ip_id;              // Identification (2 bytes)
    u_short ip_off;             // Fragment offset (2 bytes)
    u_char  ip_ttl;             // Time to Live (1 byte)
    u_char  ip_p;               // Protocol: TCP (6), UDP (17), ICMP (1) (1 byte)
    u_short ip_sum;             // Checksum (2 bytes)
    struct  in_addr ip_src;     // Source IP (4 bytes)
    struct  in_addr ip_dst;     // Destination IP (4 bytes)
} __attribute__((packed));
// Total: 20 bytes (minimum, can be larger with options)
```

**TCP Header (Layer 4):**
```c
struct tcphdr {
    u_short th_sport;           // Source port (2 bytes)
    u_short th_dport;           // Destination port (2 bytes)
    tcp_seq th_seq;             // Sequence number (4 bytes)
    tcp_seq th_ack;             // Acknowledgement number (4 bytes)
    u_char  th_offx2;           // Data offset and reserved (1 byte)
    u_char  th_flags;           // Flags: SYN, ACK, FIN, RST, etc. (1 byte)
    u_short th_win;             // Window size (2 bytes)
    u_short th_sum;             // Checksum (2 bytes)
    u_short th_urp;             // Urgent pointer (2 bytes)
} __attribute__((packed));
// Total: 20 bytes (minimum)
```

**UDP Header (Layer 4):**
```c
struct udphdr {
    u_short uh_sport;           // Source port (2 bytes)
    u_short uh_dport;           // Destination port (2 bytes)
    u_short uh_ulen;            // Length of UDP header + data (2 bytes)
    u_short uh_sum;             // Checksum (2 bytes)
} __attribute__((packed));
// Total: 8 bytes
```

### 4. Parsing Packets in C

```c
// Step 1: Parse Ethernet header (first 14 bytes)
struct ether_header *eth = (struct ether_header *)packet;

// Check if it's IPv4
if (ntohs(eth->ether_type) != ETHERTYPE_IP) {
    return;  // Skip non-IP packets
}

// Step 2: Parse IP header (comes after Ethernet header)
struct ip *ip_hdr = (struct ip *)(packet + sizeof(struct ether_header));

uint32_t src_ip = ntohl(ip_hdr->ip_src.s_addr);  // Source IP (network to host order)
uint32_t dst_ip = ntohl(ip_hdr->ip_dst.s_addr);  // Destination IP
int protocol = ip_hdr->ip_p;                      // TCP=6, UDP=17
int packet_size = header->len;                    // Total packet length

// Step 3: Parse TCP/UDP header based on protocol
if (protocol == IPPROTO_TCP) {
    // Pointer arithmetic: skip Ethernet (14) + IP header (ip_hl*4)
    struct tcphdr *tcp = (struct tcphdr *)((u_char *)ip_hdr + (ip_hdr->ip_hl * 4));
    
    int src_port = ntohs(tcp->th_sport);
    int dst_port = ntohs(tcp->th_dport);
    
} else if (protocol == IPPROTO_UDP) {
    struct udphdr *udp = (struct udphdr *)((u_char *)ip_hdr + (ip_hdr->ip_hl * 4));
    
    int src_port = ntohs(udp->uh_sport);
    int dst_port = ntohs(udp->uh_dport);
}
```

### 5. Key Important Notes

- **Network Byte Order (Big-Endian):** Network protocols use big-endian byte order. Use `ntohl()` to convert to host order and `htons()` for the reverse.
- **Structure Packing:** `__attribute__((packed))` ensures no padding in structs, preserving exact byte layout.
- **Header Lengths:** IP header has variable length. Use `ip_hl * 4` to get actual header size in bytes.
- **Pointer Arithmetic:** Cast to `u_char*` (or `uint8_t*`) for byte-level pointer arithmetic.

---

## Core Concepts and Data Structures

| Module          | Data Structure              | Complexity | Purpose                               |
| --------------- | --------------------------- | ---------- | ------------------------------------- |
| Capture Engine  | libpcap + pthreads          | O(1)       | Real-time packet ingestion            |
| Queue           | Lock-Free SPSC Queue        | O(1)       | Thread-safe packet passing            |
| Gatekeeper      | Bit Vector + vEB Tree       | O(1)       | Fast IP filtering and blacklisting    |
| Scheduler       | Max Heap                    | O(log n)   | Priority-based packet scheduling      |
| Session Manager | Splay Tree (optional)       | O(log n)   | Active connection tracking            |

---

## Architecture Flow

```
Internet Traffic (raw packets from network interface)
       |
       v
[ Capture Engine (libpcap) ]
  - Intercepts packets from network interface
  - Parses Ethernet/IP/TCP/UDP headers
  - Extracts: src_ip, dst_ip, protocol, size, ports
       |
       v
[ Concurrent Queue (Lock-Free SPSC) ]
  - Thread-safe FIFO queue (1024 packets max)
  - Producer: Capture thread
  - Consumer: Processing threads
       |
       v
[ Gatekeeper (Firewall Logic) ]
  - Layer 1: BitVector prefix check (O(1))
  - Layer 2: vEB Tree exact IP match (O(1) amortized)
  - Decision: DROP or PASS
       |
       v
[ Scheduler (Max Heap Priority Queue) ]
  - Sorts packets by priority
  - High priority: DNS (9), HTTP/HTTPS (8), Others (5)
  - O(log n) insert/extract
       |
       v
[ Process / Output / Forward ]
```

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
