#include <srt.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>

// Error handling helper
void check_srt_error(const std::string& context) {
    int error = srt_getlasterror(nullptr);
    if (error != SRT_SUCCESS) {
        std::cerr << context << " failed: " << srt_getlasterror_str() << std::endl;
        throw std::runtime_error(context + " failed");
    }
}

// Create and configure SRT socket with common settings
SRTSOCKET create_srt_socket() {
    SRTSOCKET sock = srt_create_socket();
    if (sock == SRT_INVALID_SOCK) {
        check_srt_error("Socket creation");
    }

    // Set common options
    int yes = 1;
    int timeout = 3000; // 3 seconds

    srt_setsockopt(sock, 0, SRTO_RCVSYN, &yes, sizeof(yes));
    srt_setsockopt(sock, 0, SRTO_SNDSYN, &yes, sizeof(yes));
    srt_setsockopt(sock, 0, SRTO_CONNTIMEO, &timeout, sizeof(timeout));
    
    return sock;
}

// Caller mode example
void srt_caller(const std::string& host, int port) {
    std::cout << "[Caller] Starting..." << std::endl;
    
    SRTSOCKET sock = create_srt_socket();
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        srt_close(sock);
        throw std::runtime_error("Invalid address");
    }

    std::cout << "[Caller] Connecting to " << host << ":" << port << std::endl;
    
    int result = srt_connect(sock, (sockaddr*)&sa, sizeof(sa));
    if (result == SRT_ERROR) {
        check_srt_error("Connect");
    }

    std::cout << "[Caller] Connected successfully" << std::endl;

    try {
        // Send test data
        for (int i = 0; i < 5; i++) {
            std::string message = "Caller message " + std::to_string(i);
            if (srt_send(sock, message.c_str(), message.length()) == SRT_ERROR) {
                check_srt_error("Send");
            }
            std::cout << "[Caller] Sent: " << message << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Caller] Error: " << e.what() << std::endl;
    }

    srt_close(sock);
    std::cout << "[Caller] Connection closed" << std::endl;
}

// Listener mode example
void srt_listener(const std::string& host, int port) {
    std::cout << "[Listener] Starting..." << std::endl;
    
    SRTSOCKET sock = create_srt_socket();
    sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sa.sin_addr) != 1) {
        srt_close(sock);
        throw std::runtime_error("Invalid address");
    }

    if (srt_bind(sock, (sockaddr*)&sa, sizeof(sa)) == SRT_ERROR) {
        check_srt_error("Bind");
    }

    if (srt_listen(sock, 1) == SRT_ERROR) {
        check_srt_error("Listen");
    }

    std::cout << "[Listener] Listening on " << host << ":" << port << std::endl;

    try {
        sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        SRTSOCKET client_sock = srt_accept(sock, (sockaddr*)&client_addr, &addrlen);
        
        if (client_sock == SRT_INVALID_SOCK) {
            check_srt_error("Accept");
        }

        char clienthost[NI_MAXHOST];
        char clientservice[NI_MAXSERV];
        getnameinfo((sockaddr*)&client_addr, addrlen,
                    clienthost, sizeof(clienthost),
                    clientservice, sizeof(clientservice),
                    NI_NUMERICHOST|NI_NUMERICSERV);
        
        std::cout << "[Listener] Accepted connection from " 
                  << clienthost << ":" << clientservice << std::endl;

        char buffer[1500];
        while (true) {
            int received = srt_recv(client_sock, buffer, sizeof(buffer));
            if (received == SRT_ERROR) {
                check_srt_error("Receive");
                break;
            }
            if (received == 0) {
                std::cout << "[Listener] Connection closed by peer" << std::endl;
                break;
            }
            std::string message(buffer, received);
            std::cout << "[Listener] Received: " << message << std::endl;
        }

        srt_close(client_sock);
    }
    catch (const std::exception& e) {
        std::cerr << "[Listener] Error: " << e.what() << std::endl;
    }

    srt_close(sock);
    std::cout << "[Listener] Server closed" << std::endl;
}

// Rendezvous mode example
void srt_rendezvous(const std::string& local_host, int local_port,
                    const std::string& peer_host, int peer_port) {
    std::cout << "[Rendezvous] Starting..." << std::endl;
    
    SRTSOCKET sock = create_srt_socket();
    
    // Enable rendezvous mode
    int yes = 1;
    srt_setsockopt(sock, 0, SRTO_RENDEZVOUS, &yes, sizeof(yes));

    // Setup local address
    sockaddr_in local_sa;
    memset(&local_sa, 0, sizeof(local_sa));
    local_sa.sin_family = AF_INET;
    local_sa.sin_port = htons(local_port);
    
    if (inet_pton(AF_INET, local_host.c_str(), &local_sa.sin_addr) != 1) {
        srt_close(sock);
        throw std::runtime_error("Invalid local address");
    }

    // Bind to local address
    if (srt_bind(sock, (sockaddr*)&local_sa, sizeof(local_sa)) == SRT_ERROR) {
        check_srt_error("Bind");
    }

    std::cout << "[Rendezvous] Bound to " << local_host << ":" << local_port << std::endl;

    // Setup peer address
    sockaddr_in peer_sa;
    memset(&peer_sa, 0, sizeof(peer_sa));
    peer_sa.sin_family = AF_INET;
    peer_sa.sin_port = htons(peer_port);
    
    if (inet_pton(AF_INET, peer_host.c_str(), &peer_sa.sin_addr) != 1) {
        srt_close(sock);
        throw std::runtime_error("Invalid peer address");
    }

    std::cout << "[Rendezvous] Connecting to peer at " 
              << peer_host << ":" << peer_port << std::endl;

    try {
        if (srt_connect(sock, (sockaddr*)&peer_sa, sizeof(peer_sa)) == SRT_ERROR) {
            check_srt_error("Connect");
        }

        std::cout << "[Rendezvous] Connected in rendezvous mode" << std::endl;

        // Send and receive data
        for (int i = 0; i < 5; i++) {
            // Send data
            std::string message = "Rendezvous message " + std::to_string(i);
            if (srt_send(sock, message.c_str(), message.length()) == SRT_ERROR) {
                check_srt_error("Send");
            }
            std::cout << "[Rendezvous] Sent: " << message << std::endl;

            // Try to receive response
            char buffer[1500];
            int received = srt_recv(sock, buffer, sizeof(buffer));
            if (received > 0) {
                std::string received_message(buffer, received);
                std::cout << "[Rendezvous] Received: " << received_message << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Rendezvous] Error: " << e.what() << std::endl;
    }

    srt_close(sock);
    std::cout << "[Rendezvous] Connection closed" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check for help flag
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        std::cout << "Usage: " << argv[0] << " <mode> [options]" << std::endl;
        std::cout << "Modes:" << std::endl;
        std::cout << "  caller     - Start in caller (client) mode" << std::endl;
        std::cout << "  listener   - Start in listener (server) mode" << std::endl;
        std::cout << "  rendezvous - Start in rendezvous (peer-to-peer) mode" << std::endl;
        std::cout << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  peer2      - For rendezvous mode, use alternate port configuration" << std::endl;
        return (argc < 2) ? 1 : 0;  // Return 0 if --help was specified
    }

    // Initialize SRT library
    if (srt_startup() == -1) {
        std::cerr << "SRT startup failed" << std::endl;
        return 1;
    }

    try {
        std::string mode = argv[1];
        if (mode == "caller") {
            srt_caller("127.0.0.1", 9000);
        }
        else if (mode == "listener") {
            srt_listener("127.0.0.1", 9000);
        }
        else if (mode == "rendezvous") {
            if (argc > 2 && std::string(argv[2]) == "peer2") {
                srt_rendezvous("127.0.0.1", 9001, "127.0.0.1", 9000);
            } else {
                srt_rendezvous("127.0.0.1", 9000, "127.0.0.1", 9001);
            }
        }
        else {
            std::cerr << "Unknown mode: " << mode << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    // Clean up SRT library
    srt_cleanup();
    return 0;
}
