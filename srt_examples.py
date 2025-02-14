import srt
import sys
import time
import socket
import threading

def create_srt_socket():
    """Create and configure a basic SRT socket with recommended settings."""
    sock = srt.socket()
    # Set send and receive buffer sizes
    sock.setsockopt(srt.SRTO_RCVBUF, 1024*1024)
    sock.setsockopt(srt.SRTO_SNDBUF, 1024*1024)
    # Set connection timeout (ms)
    sock.setsockopt(srt.SRTO_CONNTIMEO, 3000)
    # Enable message API
    sock.setsockopt(srt.SRTO_MESSAGEAPI, True)
    # Set latency (ms)
    sock.setsockopt(srt.SRTO_LATENCY, 200)
    return sock

def srt_caller(host='127.0.0.1', port=9000, timeout_ms=3000):
    """Example of SRT caller mode."""
    sock = None
    
    try:
        sock = create_srt_socket()
        # Set specific timeout for this connection
        sock.setsockopt(srt.SRTO_CONNTIMEO, timeout_ms)
        sock.setsockopt(srt.SRTO_SNDTIMEO, timeout_ms)
        
        print(f"[Caller] Connecting to {host}:{port}")
        sock.connect((host, port))
        print("[Caller] Connected successfully")
        
        # Send some test data
        for i in range(5):
            try:
                message = f"Caller message {i}".encode()
                sock.send(message)
                print(f"[Caller] Sent: {message.decode()}")
                time.sleep(1)
            except srt.SRTError as e:
                if "Connection timed out" in str(e):
                    print("[Caller] Send timeout")
                    break
                raise
            
    except srt.SRTError as e:
        print(f"[Caller] SRT Error: {e}")
    except Exception as e:
        print(f"[Caller] General Error: {e}")
    finally:
        if sock:
            try:
                sock.close()
                print("[Caller] Connection closed")
            except Exception as e:
                print(f"[Caller] Error closing socket: {e}")

def srt_listener(host='127.0.0.1', port=9000, timeout_ms=3000):
    """Example of SRT listener mode."""
    sock = None
    client_sock = None
    
    try:
        sock = create_srt_socket()
        # Set specific timeout for this connection
        sock.setsockopt(srt.SRTO_CONNTIMEO, timeout_ms)
        
        sock.bind((host, port))
        sock.listen(1)
        print(f"[Listener] Listening on {host}:{port}")
        
        client_sock, addr = sock.accept()
        print(f"[Listener] Accepted connection from {addr}")
        
        # Set receive timeout for client socket
        client_sock.setsockopt(srt.SRTO_RCVTIMEO, timeout_ms)
        
        while True:
            try:
                data = client_sock.recv(1024)
                if not data:
                    print("[Listener] Connection closed by peer")
                    break
                print(f"[Listener] Received: {data.decode()}")
            except srt.SRTError as e:
                if "Connection timed out" in str(e):
                    print("[Listener] Receive timeout")
                    break
                raise
            
    except Exception as e:
        print(f"[Listener] Error: {e}")
    finally:
        if client_sock:
            try:
                client_sock.close()
                print("[Listener] Client connection closed")
            except Exception as e:
                print(f"[Listener] Error closing client socket: {e}")
        
        if sock:
            try:
                sock.close()
                print("[Listener] Server closed")
            except Exception as e:
                print(f"[Listener] Error closing server socket: {e}")

def srt_rendezvous(host='127.0.0.1', port=9000, peer_host='127.0.0.1', peer_port=9001, timeout_ms=3000):
    """Example of SRT rendezvous mode."""
    sock = None
    
    try:
        sock = create_srt_socket()
        # Set specific timeouts for this connection
        sock.setsockopt(srt.SRTO_CONNTIMEO, timeout_ms)
        sock.setsockopt(srt.SRTO_SNDTIMEO, timeout_ms)
        sock.setsockopt(srt.SRTO_RCVTIMEO, timeout_ms)
        
        # Enable rendezvous mode
        sock.setsockopt(srt.SRTO_RENDEZVOUS, 1)
        
        # Bind to local address
        sock.bind((host, port))
        print(f"[Rendezvous] Binding to {host}:{port}")
        print(f"[Rendezvous] Connecting to peer at {peer_host}:{peer_port}")
        
        # Attempt rendezvous connection
        try:
            sock.connect((peer_host, peer_port))
            print("[Rendezvous] Connected in rendezvous mode")
        except srt.SRTError as e:
            if "Connection timed out" in str(e):
                print("[Rendezvous] Connection attempt timed out")
                return
            raise
        
        # Send some test data
        for i in range(5):
            try:
                message = f"Rendezvous message {i}".encode()
                sock.send(message)
                print(f"[Rendezvous] Sent: {message.decode()}")
                
                # Try to receive response
                try:
                    data = sock.recv(1024)
                    if data:
                        print(f"[Rendezvous] Received: {data.decode()}")
                except srt.SRTError as e:
                    if "Connection timed out" not in str(e):
                        raise
                
                time.sleep(1)
            except srt.SRTError as e:
                if "Connection timed out" in str(e):
                    print("[Rendezvous] Send timeout")
                    break
                raise
            
    except srt.SRTError as e:
        print(f"[Rendezvous] SRT Error: {e}")
    except Exception as e:
        print(f"[Rendezvous] General Error: {e}")
    finally:
        if sock:
            try:
                sock.close()
                print("[Rendezvous] Connection closed")
            except Exception as e:
                print(f"[Rendezvous] Error closing socket: {e}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python srt_examples.py <mode>")
        print("Modes: caller, listener, rendezvous")
        return

    mode = sys.argv[1].lower()
    
    if mode == "caller":
        srt_caller()
    elif mode == "listener":
        srt_listener()
    elif mode == "rendezvous":
        # For rendezvous, we need two instances running with swapped ports
        if len(sys.argv) > 2 and sys.argv[2] == "peer2":
            srt_rendezvous(port=9001, peer_port=9000)
        else:
            srt_rendezvous(port=9000, peer_port=9001)
    else:
        print(f"Unknown mode: {mode}")

if __name__ == "__main__":
    main()
