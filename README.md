#  ArborFlow — High-Performance Network Processing Engine

ArborFlow is a real-time network processing and security engine built in C that leverages **Advanced Data Structures** to efficiently capture, filter, and schedule network packets.

It integrates **network-level packet capture (libpcap)** with **Bit Vectors, vEB Trees, Concurrent Queues, and Heaps** to process live traffic with high performance.

---

## 🚀 Project Overview

ArborFlow simulates a **mini firewall + traffic scheduler system**:

* Captures real network packets from the system
* Filters malicious/suspicious IPs using advanced data structures
* Schedules packets based on priority using a heap
* Processes packets in real-time

---

## 🧠 Core Concepts Used

| Module          | Concept                         |
| --------------- | ------------------------------- |
| Capture Engine  | Networking (libpcap), OS        |
| Queue           | Lock-Free Concurrent Queue      |
| Gatekeeper      | Bit Vector + van Emde Boas Tree |
| Scheduler       | Max Heap (Priority Queue)       |
| Session Manager | Splay Tree (optional/extension) |

---

## ⚙️ Architecture Flow

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

## 📂 Project Structure

```
ArborFlow/
└── core_engine/
    ├── Makefile
    ├── main.c
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

## 🔥 Features

* ✅ Real-time packet capture using **libpcap**
* ✅ Fast filtering using **Bit Vector (O(1))**
* ✅ Efficient search using **vEB Tree (O(log log U))**
* ✅ Lock-free queue for high throughput
* ✅ Priority-based scheduling using **Max Heap**
* ✅ Handles thousands of packets per second

---

## 📦 How It Works

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

## ▶️ Running the Project

### 📌 Step 1: Navigate

```
cd ArborFlow/core_engine
```

### 📌 Step 2: Build

```
make clean
make
```

### 📌 Step 3: Run

```
sudo ./arborflow eth0
```

---

## ⚠️ Why Linux / WSL is Required

This project requires **Linux or WSL** because:

* Uses `libpcap` (native Linux networking library)
* Requires system headers:

  * `netinet/ip.h`
  * `unistd.h`
  * `pthread.h`
* These are not directly supported on Windows

👉 Therefore, we use **WSL (Windows Subsystem for Linux)**

---

## 📊 Sample Output

```
[PROCESS] 91.189.91.83 -> 172.19.231.46 Priority:5 Size:1494
[PROCESS] 172.19.231.46 -> 91.189.91.83 Priority:8 Size:86
```

### Meaning:

* Real packets captured from internet
* Priority assigned dynamically
* Processed using scheduler

---

## 🎯 Achievements

* Real-time packet capture ✔️
* Advanced data structures integration ✔️
* End-to-end pipeline working ✔️
* High-performance system ✔️

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
