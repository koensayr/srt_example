#include "visca_srt_common.hpp"
#include <iostream>
#include <fstream>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <filesystem>
#include <signal.h>

using namespace visca_srt;

// Structure to hold VISCA endpoint configuration
class ViscaEndpoint {
public:
    std::string name;
    std::string ip_address;
    uint16_t port;
    uint8_t camera_id;
    std::atomic<bool> connected{false};
    int socket_fd{-1};
    std::chrono::milliseconds reconnect_interval;
    std::chrono::milliseconds command_timeout;
};

class ViscaSrtClient {
private:
    json config_;
    std::unique_ptr<SrtSocket> srt_socket_;
    std::map<uint8_t, ViscaEndpoint> endpoints;
    std::atomic<bool> running{true};
    std::mutex endpoints_mutex;
    uint16_t sequence_counter{0};

    // Helper function to create and configure SRT socket
    SRTSOCKET create_srt_socket() {
        SRTSOCKET sock = srt_create_socket();
        if (sock == SRT_INVALID_SOCK) {
            throw std::runtime_error("Failed to create SRT socket");
        }

        // Set SRT options
        int yes = 1;
        int latency = 20; // ms
        srt_setsockopt(sock, 0, SRTO_SNDSYN, &yes, sizeof(yes));
        srt_setsockopt(sock, 0, SRTO_LATENCY, &latency, sizeof(latency));
        
        return sock;
    }

    // Connect to VISCA endpoint
    bool connect_to_endpoint(ViscaEndpoint& endpoint) {
        endpoint.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (endpoint.socket_fd < 0) {
            std::cerr << "Failed to create socket for endpoint " << endpoint.name << std::endl;
            return false;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(endpoint.port);
        inet_pton(AF_INET, endpoint.ip_address.c_str(), &addr.sin_addr);

        if (connect(endpoint.socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to connect to endpoint " << endpoint.name << std::endl;
            close(endpoint.socket_fd);
            endpoint.socket_fd = -1;
            return false;
        }

        endpoint.connected = true;
        return true;
    }

    // Send VISCA command over SRT
    bool send_visca_command(const ViscaMessage& msg) {
        std::vector<uint8_t> buffer(msg.data.size() + 5);
        buffer[0] = msg.camera_id;
        *(uint16_t*)(buffer.data() + 1) = htons(msg.sequence);
        *(uint16_t*)(buffer.data() + 3) = htons(msg.length);
        std::copy(msg.data.begin(), msg.data.end(), buffer.begin() + 5);

        return srt_send(srt_socket, (char*)buffer.data(), buffer.size()) > 0;
    }

    // Monitor VISCA endpoints for commands
    void monitor_endpoints() {
        std::vector<uint8_t> buffer(1500);
        
        while (running) {
            std::lock_guard<std::mutex> lock(endpoints_mutex);
            for (auto& endpoint_pair : endpoints) {
                if (!endpoint_pair.second.connected) {
                    if (!connect_to_endpoint(endpoint_pair.second)) {
                        continue;
                    }
                }

                // Check for VISCA commands
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(endpoint_pair.second.socket_fd, &readfds);

                struct timeval tv = {0, 10000}; // 10ms timeout
                if (select(endpoint_pair.second.socket_fd + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                    int received = recv(endpoint_pair.second.socket_fd, buffer.data(), buffer.size(), 0);
                    if (received > 0) {
                        // Forward command over SRT
                        ViscaMessage msg;
                        msg.camera_id = endpoint_pair.second.camera_id;
                        msg.sequence = ++sequence_counter;
                        msg.length = received;
                        msg.data.assign(buffer.begin(), buffer.begin() + received);

                        if (!send_visca_command(msg)) {
                            std::cerr << "Failed to send VISCA command over SRT" << std::endl;
                        }
                    }
                    else if (received <= 0) {
                        // Connection lost
                        close(endpoint_pair.second.socket_fd);
                        endpoint_pair.second.connected = false;
                    }
                }
            }
        }
    }

    // Handle responses from SRT server
    void handle_srt_responses() {
        std::vector<uint8_t> buffer(1500);
        
        while (running) {
            int received = srt_recv(srt_socket, (char*)buffer.data(), buffer.size());
            if (received <= 0) {
                if (running) {
                    std::cerr << "SRT connection lost" << std::endl;
                }
                break;
            }

            if (received < 5) continue;

            // Parse VISCA message
            ViscaMessage msg;
            msg.camera_id = buffer[0];
            msg.sequence = ntohs(*(uint16_t*)(buffer.data() + 1));
            msg.length = ntohs(*(uint16_t*)(buffer.data() + 3));
            msg.data.assign(buffer.begin() + 5, buffer.begin() + 5 + msg.length);

            // Forward to appropriate endpoint
            std::lock_guard<std::mutex> lock(endpoints_mutex);
            auto it = endpoints.find(msg.camera_id);
            if (it != endpoints.end() && it->second.connected) {
                send(it->second.socket_fd, msg.data.data(), msg.data.size(), 0);
            }
        }
    }

public:
    explicit ViscaSrtClient(const std::string& config_path) {
        // Load configuration
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            throw ViscaSrtException("Failed to open config file: " + config_path);
        }
        config_ = json::parse(config_file);

        // Initialize SRT library
        if (srt_startup() == -1) {
            throw ViscaSrtException("Failed to initialize SRT");
        }

        // Load endpoint configurations
        if (config_.contains("endpoints")) {
            for (const auto& ep_config : config_["endpoints"]) {
                ViscaEndpoint endpoint;
                endpoint.name = ep_config["name"].get<std::string>();
                endpoint.ip_address = ep_config["ip_address"].get<std::string>();
                endpoint.port = ep_config["port"].get<uint16_t>();
                endpoint.camera_id = ep_config["camera_id"].get<uint8_t>();
                endpoint.reconnect_interval = std::chrono::milliseconds(
                    ep_config["reconnect_interval"].get<int>());
                endpoint.command_timeout = std::chrono::milliseconds(
                    ep_config["command_timeout"].get<int>());
                endpoints[endpoint.camera_id] = endpoint;
            }
        }
    }

    ~ViscaSrtClient() {
        stop();
        srt_cleanup();
    }

    // Print client configuration
    void print_config() const {
        std::cout << "VISCA-SRT Client Configuration:" << std::endl;
        std::cout << "SRT Server: " << config_["srt_server"]["host"].get<std::string>()
                  << ":" << config_["srt_server"]["port"].get<uint16_t>() << std::endl;
        std::cout << "\nConfigured Endpoints:" << std::endl;
        
        for (const auto& [id, endpoint] : endpoints) {
            std::cout << "Endpoint " << static_cast<int>(id) << ": " 
                      << endpoint.name << " (" 
                      << endpoint.ip_address << ":" 
                      << endpoint.port << ")" << std::endl;
        }
    }

    // Start the client
    void start() {
        // Create and configure SRT socket
        srt_socket_ = std::make_unique<SrtSocket>();
        
        // Apply SRT settings from config
        if (config_.contains("srt_settings")) {
            srt_socket_->set_options(config_["srt_settings"]);
        }

        // Connect to SRT server
        const auto& server_config = config_["srt_server"];
        const std::string& host = server_config["host"].get<std::string>();
        uint16_t port = server_config["port"].get<uint16_t>();

        try {
            srt_socket_->connect(host, port);
            std::cout << "Connected to SRT server at " << host << ":" << port << std::endl;

            // Start endpoint monitoring thread
            std::thread monitor_thread(&ViscaSrtClient::monitor_endpoints, this);
            monitor_thread.detach();

            // Start SRT response handling thread
            std::thread response_thread(&ViscaSrtClient::handle_srt_responses, this);
            response_thread.detach();

            print_config();
        }
        catch (const ViscaSrtException& e) {
            throw ViscaSrtException("Failed to connect to SRT server: " + std::string(e.what()));
        }
    }

    // Stop the client
    void stop() {
        running = false;
        
        if (srt_socket != SRT_INVALID_SOCK) {
            srt_close(srt_socket);
        }

        // Close all endpoint connections
        std::lock_guard<std::mutex> lock(endpoints_mutex);
        for (auto& endpoint : endpoints) {
            if (endpoint.second.connected) {
                close(endpoint.second.socket_fd);
                endpoint.second.connected = false;
            }
        }
    }
};

// Signal handler for graceful shutdown
std::atomic<bool> g_running{true};
void signal_handler(int signal) {
    g_running = false;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  -c, --config <path>    Path to configuration file (default: /etc/visca_srt/client_config.json)" << std::endl;
    std::cout << "  -h, --help             Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string config_path = "/etc/visca_srt/client_config.json";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_path = argv[++i];
            } else {
                std::cerr << "Error: Configuration path required after " << arg << std::endl;
                return 1;
            }
        }
    }

    try {
        ViscaSrtClient client(config_path);
        client.start();

        // Wait for shutdown signal
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "\nShutting down client..." << std::endl;
    }
    catch (const ViscaSrtException& e) {
        std::cerr << "VISCA-SRT Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
