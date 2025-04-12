#!/usr/bin/env python3

import socket
import struct
import time
import argparse
import sys

def create_tally_message(source_name, state):
    """Create an NDI tally message in the VISCA-SRT protocol format."""
    timestamp = int(time.time())
    # Protocol type (0x02 for NDI tally)
    msg = bytearray([0x02])
    # Tally state
    msg.extend([state])
    # Source name length
    msg.extend([len(source_name)])
    # Timestamp (4 bytes, big-endian)
    msg.extend(struct.pack('!l', timestamp))
    # Source name
    msg.extend(source_name.encode())
    return msg

def send_tally_state(sock, source_name, state, verbose=False):
    """Send a tally state message to the VISCA-SRT server."""
    msg = create_tally_message(source_name, state)
    sock.send(msg)
    if verbose:
        state_names = {
            0x00: "OFF",
            0x01: "PROGRAM",
            0x02: "PREVIEW",
            0x03: "PROGRAM+PREVIEW"
        }
        print(f"Sent tally state {state_names.get(state, 'UNKNOWN')} to {source_name}")

def main():
    parser = argparse.ArgumentParser(description="Test NDI tally functionality in VISCA-SRT proxy")
    parser.add_argument("--host", default="localhost", help="VISCA-SRT server host")
    parser.add_argument("--port", type=int, default=9000, help="VISCA-SRT server port")
    parser.add_argument("--source", default="MainCam", help="NDI source name")
    parser.add_argument("--cycle", action="store_true", help="Cycle through all tally states")
    parser.add_argument("--state", type=int, choices=[0, 1, 2, 3], 
                        help="Tally state (0=Off, 1=Program, 2=Preview, 3=Both)")
    parser.add_argument("--interval", type=float, default=2.0, 
                        help="Interval between state changes in cycle mode")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")
    
    args = parser.parse_args()

    try:
        # Create UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect((args.host, args.port))

        if args.cycle:
            print(f"Cycling tally states for source {args.source}")
            try:
                while True:
                    for state in [0, 1, 2, 3]:
                        send_tally_state(sock, args.source, state, args.verbose)
                        time.sleep(args.interval)
            except KeyboardInterrupt:
                print("\nStopping tally cycle")
        else:
            if args.state is None:
                parser.error("--state is required when not using --cycle")
            send_tally_state(sock, args.source, args.state, args.verbose)

    except ConnectionRefusedError:
        print(f"Error: Could not connect to {args.host}:{args.port}")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        sock.close()

    return 0

if __name__ == "__main__":
    sys.exit(main())
