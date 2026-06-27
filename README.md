# megastorage 🚀

A custom console-based archiver written in C, implementing the classic **LZSS (Lempel-Ziv-Storer-Szymanski)** compression algorithm featuring an efficient hash table for match chain lookups.

## ✨ Features
* **LZSS Algorithm:** Fast decompression speeds and stable compression rates for repeating data structures.
* **Hash Table Lookup:** High-speed match hunting in a sliding window with Chain Depth Limitation to maintain an optimal balance between speed and compression ratio.
* **Smart Mode Fallback:** If a data block cannot be efficiently compressed (i.e., the compressed size exceeds the original), the archiver automatically saves it raw (STORE mode) to avoid file bloating.
* **Interactive TUI:** A clean, retro-styled blue console user interface built specifically for the Windows command prompt.
* **Multi-file Containers (.mgsa):** Ability to pack multiple files into a single archive container and explore its contents seamlessly using the built-in Container Explorer.

## 🛠 Technical Specifications
* **Sliding Window Size:** 4096 bytes (12-bit offset)
* **Minimum Match Length (Min Match):** 3 bytes
* **Maximum Match Length (Max Match):** 18 bytes (4-bit length)
* **Match Marker Size:** 16 bits (2 bytes) of packed data (Offset + Length)

## 📦 Building the Project
The project is tailored for the **GCC (MinGW)** compiler.

### Build via Command Line:
```bash
gcc -c compressor.c -o compressor.o
gcc -c interface.c -o interface.o
gcc -c main.c -o main.o
gcc -o megastorage.exe compressor.o interface.o main.o
