# SRT Connection Examples

This repository contains example implementations of different SRT (Secure Reliable Transport) connection modes and a VISCA over SRT proxy implementation.

## VISCA over SRT Proxy

The VISCA over SRT proxy implementation allows VISCA IP camera control commands to be tunneled through SRT connections. This is particularly useful in high-latency or unreliable network conditions where TCP-based VISCA IP might be problematic.

### Features
- NDI tally integration
  - Map NDI sources to VISCA cameras
  - Configurable tally commands
  - Program and Preview tally support
  - Automatic tally state tracking

### NDI Tally Integration

#### Protocol Structure
The VISCA-SRT proxy uses a message protocol that supports both VISCA commands and NDI tally:

1. Message Format:
   ```
   [Protocol Type (1 byte)] [Message-Specific Data]
   ```

2. Protocol Types:
   - `0x01`: VISCA message
   - `0x02`: NDI tally message

3. VISCA Message Structure:
   ```
   [Protocol Type] [VISCA Type] [Camera ID] [Sequence] [Length] [VISCA Data]
        1 byte      1 byte       1 byte     2 bytes    2 bytes    N bytes
   ```

4. NDI Tally Message Structure:
   ```
   [Protocol Type] [Tally State] [Name Length] [Timestamp] [Source Name]
        1 byte       1 byte        1 byte       4 bytes     N bytes
   ```

#### Tally State Protocol
NDI tally states are mapped to VISCA commands as follows:

1. Program (On-Air):
   - State: `0x01`
   - Default Command: Red tally lamp
   - Can be customized per camera

2. Preview:
   - State: `0x02`
   - Default Command: Green tally lamp
   - Can be customized per camera

3. Program + Preview:
   - State: `0x03`
   - Uses Program (Red) by default
   - Configurable priority

4. Off:
   - State: `0x00`
   - Turns off tally lamp
   - Can use custom command


The VISCA-SRT proxy supports mapping NDI sources to VISCA cameras for tally state control. When an NDI source's tally state changes (Program/Preview), the corresponding camera receives configured VISCA commands.

#### NDI Tally Configuration
```json
{
    "cameras": [
        {
            "id": 1,
            "name": "Main Camera",
            "ndi_mapping": {
                "source_name": "MainCam",
                "tally_enabled": true,
                "commands": {
                    "program": [129, 1, 126, 1, 10, 0, 2, 255],
                    "preview": [129, 1, 126, 1, 10, 0, 1, 255],
                    "off": [129, 1, 126, 1, 10, 0, 3, 255]
                }
            }
        }
    ],
    "ndi_settings": {
        "tally_update_interval": 100,
        "source_discovery_interval": 1000,
        "program_tally_priority": true
    }
}
```

#### Tally Commands
The example above uses standard VISCA commands for tally control:
- Program: Tally lamp red (0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x02, 0xFF)
- Preview: Tally lamp green (0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x01, 0xFF)
- Off: Tally lamp off (0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x03, 0xFF)

Different cameras may require different commands - consult your camera's VISCA documentation.

- Proxies VISCA commands over SRT connections
- Supports multiple cameras/endpoints
- Configurable connection parameters
- Automatic reconnection handling
- Comprehensive error handling and logging
- JSON-based configuration

### Architecture
```
         VISCA IP              SRT              VISCA IP
[Controller] <----> [Client] <=====> [Server] <----> [Camera]
```

### Components
1. **VISCA-SRT Server**
   - Accepts SRT connections from clients
   - Maintains connections to VISCA IP cameras
   - Forwards commands and responses
   - Handles multiple cameras simultaneously

2. **VISCA-SRT Client**
   - Connects to VISCA-SRT server via SRT
   - Accepts VISCA IP connections from controllers
   - Forwards commands and responses
   - Supports multiple endpoints


### Usage

#### Running the Server
1. Create a server configuration file:
```bash
cp /etc/visca_srt/server_config.json myserver.json
# Edit myserver.json to configure your cameras
```

2. Start the server:
```bash
visca_srt_server -c myserver.json
```

The server will:
- Listen for SRT connections on the configured port
- Establish connections to configured VISCA cameras
- Forward commands from clients to cameras
- Return camera responses to clients

#### Running the Client
1. Create a client configuration file:
```bash
cp /etc/visca_srt/client_config.json myclient.json
# Edit myclient.json to configure your endpoints
```

2. Start the client:
```bash
visca_srt_client -c myclient.json
```

The client will:
- Connect to the VISCA-SRT server
- Listen for VISCA controllers on configured endpoints
- Forward controller commands through SRT
- Return camera responses to controllers

#### Command Line Options
Both server and client support:
- `-c, --config <path>`: Specify configuration file path
- `-h, --help`: Show help message

#### Monitoring and Control

#### NDI Tally Examples

1. Testing tally with a simple command:
```bash
# Send program tally state to camera 1
echo -e "\x02\x01\x07MainCam\x00\x00\x00\x01" | nc -u localhost 9000
```

2. Python script for NDI tally simulation:
```python
import socket
import struct
import time

def send_tally_state(sock, source_name, state):
    # Create message: protocol(1) + state(1) + name_len(1) + timestamp(4) + name(N)
    timestamp = int(time.time())
    msg = struct.pack('!BBBl', 0x02, state, len(source_name), timestamp)
    msg += source_name.encode()
    sock.send(msg)

# Connect to VISCA-SRT server
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.connect(('localhost', 9000))

# Simulate tally changes
try:
    # Set to Program (red tally)
    send_tally_state(sock, 'MainCam', 0x01)
    time.sleep(2)
    
    # Set to Preview (green tally)
    send_tally_state(sock, 'MainCam', 0x02)
    time.sleep(2)
    
    # Turn off tally
    send_tally_state(sock, 'MainCam', 0x00)
finally:
    sock.close()
```

3. Integration with NDI Tools:
```bash
# Use ndi-tools to monitor real NDI sources and send tally states
ndi-monitor --source "MainCam" --tally-script ./send_tally.py
```

The server will handle these tally messages and translate them into the appropriate VISCA commands for each mapped camera.

- Both applications respond to SIGINT (Ctrl+C) and SIGTERM for graceful shutdown
- Status updates and errors are logged to stdout/stderr
- Connection status for cameras/endpoints is displayed on startup
- Detailed error messages help diagnose connection issues

## SRT Connection Modes
## Configuration

The VISCA-SRT proxy uses JSON configuration files for both server and client components.

### Server Configuration
```json
{
    "bind_address": "0.0.0.0",
    "srt_port": 9000,
    "cameras": [
        {
            "id": 1,
            "name": "Main Stage Camera",
            "ip_address": "192.168.1.100",
            "port": 52381,
            "reconnect_interval": 5000
        }
    ],
    "srt_settings": {
        "latency": 20,
        "max_bw": 1500000,
        "input_buffer": 1024000,
        "output_buffer": 1024000,
        "max_clients": 5
    }
}
```

### Client Configuration
```json
{
    "srt_server": {
        "host": "127.0.0.1",
        "port": 9000
    },
    "endpoints": [
        {
            "camera_id": 1,
            "name": "Main Control PTZ",
            "ip_address": "192.168.1.200",
            "port": 52381,
            "reconnect_interval": 5000,
            "command_timeout": 2000
        }
    ],
    "srt_settings": {
        "latency": 20,
        "max_bw": 1500000,
        "connection_timeout": 3000
    }
}
```

### Configuration Options

#### Server Options
- `bind_address`: IP address to bind the server (default: "0.0.0.0")
- `srt_port`: Port for SRT connections
- `cameras`: Array of VISCA camera configurations
  - `id`: Unique camera identifier
  - `name`: Display name for the camera
  - `ip_address`: Camera's VISCA IP address
  - `port`: Camera's VISCA port (typically 52381)
  - `reconnect_interval`: Milliseconds between reconnection attempts

#### Client Options
- `srt_server`: Connection details for the VISCA-SRT server
- `endpoints`: Array of VISCA endpoint configurations
  - `camera_id`: Must match server's camera ID
  - `name`: Display name for the endpoint
  - `ip_address`: Local IP to listen for VISCA controllers
  - `port`: Local port for VISCA controllers
  - `command_timeout`: Milliseconds to wait for command response

#### Common SRT Settings
- `latency`: SRT latency in milliseconds
- `max_bw`: Maximum bandwidth in bits per second
- `input_buffer`/`output_buffer`: Buffer sizes in bytes
=======
This repository contains example implementations of different SRT (Secure Reliable Transport) connection modes: caller, listener, and rendezvous. The examples demonstrate how to establish connections using each mode and perform basic data transfer.

## Installation

### Using Docker
You can build and run the project using Docker:

```bash
# Build the Docker image
docker build -t visca-srt .

# Run the server (using host network for SRT connectivity)
docker run --network host visca-srt

# Run the client (override default command)
docker run --network host visca-srt visca_srt_client

# Run with custom configuration
docker run -v /path/to/config:/etc/visca_srt visca-srt
```

The Docker image uses a multi-stage build to minimize size and includes all necessary dependencies. The server configuration can be customized by mounting a local config directory to `/etc/visca_srt` in the container.

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

#### Building the VISCA-SRT Proxy

1. Dependencies:
```bash
# Ubuntu/Debian
sudo apt-get install libsrt-dev nlohmann-json3-dev cmake build-essential

# macOS
brew install srt nlohmann-json cmake
```

2. Build from source:
```bash
mkdir build && cd build
cmake ..
make

# Optional: Install system-wide
sudo make install
```

3. Verify installation:
```bash
# Check server
visca_srt_server --help

# Check client
visca_srt_client --help
```

The installation will place:
- Executables in `/usr/local/bin/`
- Default configs in `/etc/visca_srt/`
- Documentation in `/usr/local/share/doc/visca_srt/`


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
