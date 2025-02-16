# SRT Connection Examples

This repository contains example implementations of different SRT (Secure Reliable Transport) connection modes: caller, listener, and rendezvous. The examples demonstrate how to establish connections using each mode and perform basic data transfer.


## Installation

### Using Homebrew
The easiest way to install on macOS is using Homebrew:

```bash
# Add the tap repository
brew tap koensayr/srt-examples

# Install the package
brew install srt-examples
```

This will install both the C++ and Python examples, along with all dependencies.

#### Upgrading
To upgrade to the latest version:
```bash
brew update
brew upgrade srt-examples
```

#### Version Information
- Current stable version: 1.0.0
- Required dependencies:
  - libsrt (automatically installed)
  - cmake (for building)
  - python@3.9 or later (for Python example)

#### Installation Locations
After installation:
- C++ executable: `$(brew --prefix)/bin/srt_example`
- Python script: `$(brew --prefix)/bin/srt_examples.py`

### Manual Installation

#### Prerequisites

1. CMake (3.10 or higher)
2. C++11 compatible compiler
3. libsrt development package

On Ubuntu/Debian:
```bash
sudo apt-get install libsrt-dev cmake build-essential
```

On macOS with Homebrew:
```bash
brew install srt cmake
```

### For Python Implementation
```bash
pip install srt
```

## Building the C++ Example

```bash
mkdir build
cd build
cmake ..
make
```

## Connection Modes

### 1. Caller Mode
The caller mode is a client that initiates a connection to a listening server.

C++ Implementation:
```bash
./srt_example caller
```

Python Implementation:
```bash
python srt_examples.py caller
```

### 2. Listener Mode
The listener mode acts as a server that waits for incoming connections.

C++ Implementation:
```bash
./srt_example listener
```

Python Implementation:
```bash
python srt_examples.py listener
```

### 3. Rendezvous Mode
Rendezvous mode allows peer-to-peer connection where both parties can initiate the connection simultaneously. This requires running two instances with different port configurations.

C++ Implementation:
```bash
# Terminal 1
./srt_example rendezvous

# Terminal 2
./srt_example rendezvous peer2
```

Python Implementation:
```bash
# Terminal 1
python srt_examples.py rendezvous

# Terminal 2
python srt_examples.py rendezvous peer2
```

## Example Usage Scenarios

### C++ Implementation

1. Basic client-server setup:
   - First, start the listener (server):
     ```bash
     ./srt_example listener
     ```
   - Then, in another terminal, start the caller (client):
     ```bash
     ./srt_example caller
     ```

2. Peer-to-peer setup using rendezvous mode:
   - Start the first peer:
     ```bash
     ./srt_example rendezvous
     ```
   - Start the second peer:
     ```bash
     ./srt_example rendezvous peer2
     ```

### Python Implementation

1. Basic client-server setup:
   - First, start the listener (server):
     ```bash
     python srt_examples.py listener
     ```
   - Then, in another terminal, start the caller (client):
     ```bash
     python srt_examples.py caller
     ```

2. Peer-to-peer setup using rendezvous mode:
   - Start the first peer:
     ```bash
     python srt_examples.py rendezvous
     ```
   - Start the second peer:
     ```bash
     python srt_examples.py rendezvous peer2
     ```

## Code Structure

Both implementations demonstrate:
- Basic SRT socket creation and configuration
- Connection establishment in different modes
- Simple data transmission
- Error handling and proper socket cleanup
- Timeout handling
- Thread-safe operations

### C++ Implementation (srt_example.cpp)
- Uses libSRT directly for low-level control
- Includes comprehensive error handling with `check_srt_error()`
- Implements proper resource cleanup with RAII principles
- Uses modern C++11 features for better code organization

Functions:
- `create_srt_socket()`: Sets up a configured SRT socket
- `srt_caller()`: Implements the caller (client) mode
- `srt_listener()`: Implements the listener (server) mode
- `srt_rendezvous()`: Implements the rendezvous (peer-to-peer) mode

### Python Implementation (srt_examples.py)
- Uses the python-srt binding for higher-level access
- Provides simpler, more Pythonic interface
- Includes context-aware error handling
- Automatic resource management through Python's garbage collection

Functions:
- `create_srt_socket()`: Creates and configures an SRT socket
- `srt_caller()`: Implements the caller (client) mode
- `srt_listener()`: Implements the listener (server) mode
- `srt_rendezvous()`: Implements the rendezvous (peer-to-peer) mode

## Implementation Notes

Common Features:
- Both implementations use localhost (127.0.0.1) by default
- Default ports are 9000 for caller/listener and 9000/9001 for rendezvous mode
- Connection timeout set to 3 seconds
- Message size limit of 1500 bytes

C++ Implementation Specifics:
- Direct use of libSRT API for fine-grained control
- Explicit memory management and resource cleanup
- Comprehensive error reporting through `srt_getlasterror()`
- Support for both IPv4 and IPv6 addressing

Python Implementation Specifics:
- High-level abstraction through python-srt
- Automatic resource management
- Simplified error handling with Python exceptions
- Focus on code readability and maintainability

Error Handling:
- Both implementations handle:
  - Socket creation failures
  - Connection timeouts
  - Connection failures
  - Send/receive errors
  - Resource cleanup
  - Invalid addresses
  - Network errors
