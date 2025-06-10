# File Version Control System (FVCS)

![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![C++: 17](https://img.shields.io/badge/C++-17-blue.svg)
![Python: 3.8+](https://img.shields.io/badge/Python-3.8+-green.svg)
![Flask: 2.0](https://img.shields.io/badge/Flask-2.0-red.svg)

A lightweight, Git-inspired version control system for tracking file changes with local/remote collaboration capabilities.

---

## Features

### Core Version Control
- Commit history with diffs
- Branching and merging
- Custom LCS-based diff algorithm

### Remote Sync
- `clone`/`push`/`pull` operations
- Flask server for repository hosting
- Atomic operations for consistency

### Efficient Storage
- Incremental diffs (no redundant copies)
- Compressed metadata storage

---

## Installation

### Prerequisites
- C++17 compiler (GCC/Clang)
- Python 3.8+ (for server)
- OpenSSL (for SHA hashing)

### Start server (requires Flask)
python3 server.py

### Initialize repository
./vcs init

### Stage files
./vcs add <filename>

### Commit changes
./vcs commit "message"

### Branching
./vcs createbranch feature-x

./vcs switch feature-x

### Remote operations (requires server)
./vcs clone <remote_url> <dir>

./vcs push

./vcs pull

Project Structure
FVCS/

├── vcs.cpp # Core version control logic

├── server.py # Remote sync server (Flask)

├── Makefile # Build configuration

├── .vcs/ # Local repository data

│ ├── commits/ # Commit history

│ └── staging/ # Staged files

└── tests/ # Unit tests
