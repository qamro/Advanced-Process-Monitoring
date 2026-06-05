<div align="center">

```
 ██████╗ ██╗  ██╗ ██████╗ ███████╗████████╗
██╔════╝ ██║  ██║██╔═══██╗██╔════╝╚══██╔══╝
██║  ███╗███████║██║   ██║███████╗   ██║   
██║   ██║██╔══██║██║   ██║╚════██║   ██║   
╚██████╔╝██║  ██║╚██████╔╝███████║   ██║   
 ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝   ╚═╝  
          M O N I T O R  v1.0
```

**A real-time system monitor for Linux, built from scratch in C.**

[![Language](https://img.shields.io/badge/Language-C-00ff88?style=flat-square&labelColor=0a0a0a)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/Platform-Linux-00aaff?style=flat-square&labelColor=0a0a0a)](https://www.kernel.org/)
[![Interface](https://img.shields.io/badge/Interface-ncurses-ff6b35?style=flat-square&labelColor=0a0a0a)](https://invisible-island.net/ncurses/)
[![License](https://img.shields.io/badge/License-MIT-cc44ff?style=flat-square&labelColor=0a0a0a)](LICENSE)

</div>

---

## 🖥️ Preview

<div align="center">

<img width="1920" height="1041" alt="ghost" src="https://github.com/user-attachments/assets/9f04283f-9453-431b-987f-b61fa80b705e" />

</div>

---

## What is Ghost Monitor?

Ghost Monitor is a **lightweight, live system monitoring tool** that runs entirely inside your terminal. No GUI. No dependencies beyond ncurses. It reads directly from the Linux kernel's `/proc` filesystem — the same source of truth used by tools like `htop`, `top`, and `vmstat` — and renders a clean, color-coded dashboard that updates every second.

Built as a deep dive into **systems programming**, it touches the Linux kernel interface, process scheduling data, memory management internals, network I/O accounting, and terminal rendering — all in plain C.

---

## Features

| Feature | Details |
|---|---|
| **CPU Monitoring ⚙️** | Per-core usage with real delta-based calculation |
| **Memory 🧠** | Used / cached / total with live percentage bars |
| **Network I/O 🌐** | RX and TX throughput in B/s · KB/s · MB/s |
| **Disk I/O 💽** | Read and write throughput, auto-detects SATA / NVMe / VirtIO |
| **Sparklines 📈** | 40-sample ASCII history graph for CPU, memory, and network |
| **Load Average ⚡** | 1m / 5m / 15m system load from `/proc/loadavg` |
| **Uptime Counter ⏱️** | Live session uptime in HH:MM:SS |
| **Color Thresholds 🌡️** | Green → Amber → Red as load increases |
| **Critical Alerts 🚨** | Blinking warning when any CPU core exceeds 80% |
| **Safe Rendering 🔒** | Pure ASCII + ncurses ACS — works on any terminal |

---

## Project Structure

```
ghost_monitor/
├── main.c        ← Entry point — main loop, timing, input handling
├── cpu.c/h       ← Per-core CPU usage via /proc/stat (delta method)
├── mem.c/h       ← RAM usage via /proc/meminfo
├── net.c/h       ← Network throughput via /proc/net/dev
├── disk.c/h      ← Disk I/O via /proc/diskstats
├── system.c/h    ← Helper: total CPU time reader
├── ui.c/h        ← Full ncurses UI — panels, bars, sparklines, colors
└── Makefile      ← Build configuration
```

---

## How It Works

Ghost Monitor never uses external monitoring APIs. Everything comes from reading text files in `/proc` — a virtual filesystem the Linux kernel exposes in RAM. Here is what each `/proc` file provides:

```
/proc/stat          →  CPU time counters per core (user, system, idle...)
/proc/meminfo       →  Memory totals, available, cached, buffers
/proc/net/dev       →  Cumulative bytes sent/received per interface
/proc/diskstats     →  Cumulative sectors read/written per disk
/proc/loadavg       →  System load averages (1m, 5m, 15m)
```

Since these are **cumulative counters**, the program reads them twice — one second apart — and calculates the **delta** to find the actual current rate. This is the same technique used by `htop` and the Linux kernel's own accounting tools.

```
CPU Usage = (work_now - work_before) / (total_now - total_before) × 100
Net Speed = (bytes_now - bytes_before) / elapsed_seconds
```

---

## Requirements

- Linux (any distribution)
- GCC
- ncurses development library

```bash
# Debian / Ubuntu
sudo apt install gcc libncurses5-dev

# Arch Linux
sudo pacman -S gcc ncurses

# Fedora / RHEL
sudo dnf install gcc ncurses-devel
```

---

## Build & Run

```bash
# Clone or extract the project
cd ghost_monitor

# Build
make

# Run
./ghost

# Quit
press Q
```

---

## Controls

| Key | Action |
|-----|--------|
| `Q` or `q` | Quit the monitor |

Mouse movement is fully ignored — it will not cause any flicker or redraw.

---

## Architecture — The Main Loop

```
start
  │
  ├─ init ncurses, disable mouse, set colors
  │
  ├─ warm-up read (first snapshot of all counters)
  │
  ├─ sleep 1 second
  │
  └─ loop forever:
        │
        ├── read /proc/stat        → CPU delta
        ├── read /proc/meminfo     → RAM %
        ├── read /proc/net/dev     → net delta
        ├── read /proc/diskstats   → disk delta
        │
        ├── draw full UI (ncurses clear + refresh)
        │
        ├── drain input queue → exit on Q
        │
        └── sleep 1 second → repeat
```

The warm-up read before the loop ensures the very first frame shows real data instead of a zero spike.

---

## 🧠 Technologies Used

C Language (Systems Programming)
ncurses library
Linux /proc filesystem

---

## 🚀 Project Goal

This project was built to deeply understand:

Linux system internals
Process and memory management
Real-time terminal UI development
Low-level performance monitoring tools

---

## 👨‍💻 Author

<div align="center">

### Bakhouche Mohamed Qamar Eddine

*Computer Science student exploring*


*Systems Programming 🧠 · Operating Systems 🐧 · Low-level Software Design ⚙️*

---

*"The best way to understand an operating system is to talk to it directly."*

</div>

---

## ⭐ License

This project is open-source for educational purposes.

---

<div align="center">
<sub>Built with C · powered by the Linux kernel · rendered by ncurses</sub>
</div>
