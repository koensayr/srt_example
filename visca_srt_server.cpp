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

// Structure to hold VISCA camera configuration
#include "ndi_tally_common.hpp"
#include <chrono>

class ViscaCamera {
public:
    std::string name;
    std::string ip_address;
    uint16_t port;
    std::atomic<bool> connected{false};
    int socket_fd{-1};

    // NDI Tally related members
    NdiCameraMapping ndi_mapping;
    std::atomic<TallyState> current_tally_state{TallyState::OFF};
    std::chrono::steady_clock::time_point last_tally_update;

    // Send tally state command to camera
    bool send_tally_command(TallyState state) {
        if (!connected) return false;

        std::vector<uint8_t> command;
        switch (state) {
            case TallyState::PROGRAM:
                command = ndi_mapping.program_tally_command;
                break;
            case TallyState::PREVIEW:
                command = ndi_mapping.preview_tally_command;
                break;
            case TallyState::OFF:
            default:
                command = ndi_mapping.tally_off_command;
                break;
        }

        if (command.empty()) return false;

        if (send(socket_fd, command.data(), command.size(), 0) < 0) {
            connected = false;
            return false;
        }

        current_tally_state = state;
        last_tally_update = std::chrono::steady_clock::now();
        return true;
    }

    // Update tally state if changed
    bool update_tally_state(TallyState new_state) {
        if (new_state != current_tally_state) {
            return send_tally_command(new_state);
        }
        return true;
    }
};

// Structure for SRT-encapsulated VISCA message
struct ViscaMessage {
    uint8_t camera_id;
    uint16_t sequence;
    uint16_t length;
    std::vector<uint8_t> data;
};

class ViscaSrtServer {
private:
    json config_;
    std::unique_ptr<SrtSocket> server_socket_;
    std::map<uint8_t, ViscaCamera> cameras;
    std::atomic<bool> running{true};
    std::mutex cameras_mutex;
    
    // NDI tally handling
    std::thread ndi_tally_thread_;
    std::map<std::string, TallyState> ndi_tally_states_;
    std::mutex ndi_tally_mutex_;
    
    void handle_ndi_tally() {
        auto update_interval = std::chrono::milliseconds(
            config_["ndi_settings"]["tally_update_interval"].get<int>());
            
        while (running) {
            std::this_thread::sleep_for(update_interval);
            
            std::lock_guard<std::mutex> tally_lock(ndi_tally_mutex_);
            std::lock_guard<std::mutex> camera_lock(cameras_mutex);
            
            // Update each camera's tally state based on NDI source state
            for (auto& [id, camera] : cameras) {
                if (!camera.ndi_mapping.ndi_source_name.empty()) {
                    auto it = ndi_tally_states_.find(camera.ndi_mapping.ndi_source_name);
                    if (it != ndi_tally_states_.end()) {
                        camera.update_tally_state(it->second);
                    }
                }
            }
        }
    }
    
    // Handle incoming NDI tally message
    void process_ndi_tally_message(const NdiTallyMessage& msg) {
        std::lock_guard<std::mutex> lock(ndi_tally_mutex_);
        ndi_tally_states_[msg.ndi_source_name] = msg.state;
    }
    
    // Message queues for each camera
    std::map<uint8_t, std::queue<ViscaMessage>> message_queues;
    std::mutex queues_mutex;

    // Helper function to create and configure SRT socket
    SRTSOCKET create_srt_socket() {
        SRTSOCKET sock = srt_create_socket();
        if (sock == SRT_INVALID_SOCK) {
            throw std::runtime_error("Failed to create SRT socket");
        }

        // Set SRT options
        int yes = 1;
        int latency = 20; // ms
        srt_setsockopt(sock, 0, SRTO_RCVSYN, &yes, sizeof(yes));
        srt_setsockopt(sock, 0, SRTO_LATENCY, &latency, sizeof(latency));
        
        return sock;
    }

    // Connect to VISCA camera
    bool connect_to_camera(ViscaCamera& camera) {
        camera.socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (camera.socket_fd < 0) {
            std::cerr << "Failed to create socket for camera " << camera.name << std::endl;
            return false;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(camera.port);
        inet_pton(AF_INET, camera.ip_address.c_str(), &addr.sin_addr);

        if (connect(camera.socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to connect to camera " << camera.name << std::endl;
            close(camera.socket_fd);
            camera.socket_fd = -1;
            return false;
        }

        camera.connected = true;
        return true;
    }

    // Handle VISCA commands from SRT clients
    void handle_srt_client(std::unique_ptr<SrtSocket> client_socket) {
        std::vector<uint8_t> buffer(1500);
        
        while (running) {
            try {
                // Receive data from SRT client
                int received = srt_recv(client_socket->get(), (char*)buffer.data(), buffer.size());
                if (received <= 0) {
                    break;
                }

                // Get protocol type from first byte
                buffer.resize(received);
                MessageType protocol_type = static_cast<MessageType>(buffer[0]);

                try {
                    switch (protocol_type) {
                        case MessageType::VISCA: {
                            buffer.erase(buffer.begin()); // Remove protocol type byte
                            auto msg = ViscaMessage::deserialize(buffer);

                            // Validate message
                            if (!visca_util::validate_message(msg.data)) {
                                std::cerr << "Invalid VISCA message received" << std::endl;
                                continue;
                            }

                            process_visca_message(msg, client_socket.get());
                            break;
                        }
                        case MessageType::NDI_TALLY: {
                            buffer.erase(buffer.begin()); // Remove protocol type byte
                            auto tally_msg = NdiTallyMessage::deserialize(buffer);
                            process_ndi_tally_message(tally_msg);
                            
                            std::cout << "NDI Tally update - Source: '" << tally_msg.ndi_source_name 
                                     << "', State: " << static_cast<int>(tally_msg.state)
                                     << ", Time: " << tally_msg.timestamp << std::endl;
                            break;
                        }
                        default:
                            std::cerr << "Unknown protocol type: " << static_cast<int>(protocol_type) << std::endl;
                            continue;
                    }
                }
                catch (const ViscaSrtException& e) {
                    std::cerr << "Protocol error: " << e.what() << std::endl;
                }
                catch (const std::exception& e) {
                    std::cerr << "Error processing message: " << e.what() << std::endl;
                }

                // Forward to appropriate camera
                std::lock_guard<std::mutex> lock(cameras_mutex);
                auto it = cameras.find(msg.camera_id);
                if (it != cameras.end() && it->second.connected) {
                    // Send command to camera
                    if (send(it->second.socket_fd, msg.data.data(), msg.data.size(), 0) < 0) {
                        std::cerr << "Failed to send command to camera " << it->second.name << std::endl;
                        continue;
                    }

                    // Wait for and forward response if it's a command or inquiry
                    if (msg.type == MessageType::COMMAND || msg.type == MessageType::INQUIRY) {
                        std::vector<uint8_t> response(1500);
                        int resp_size = recv(it->second.socket_fd, response.data(), response.size(), 0);
                        
                        if (resp_size > 0) {
                            ViscaMessage resp_msg;
                            resp_msg.type = MessageType::RESPONSE;
                            resp_msg.camera_id = msg.camera_id;
                            resp_msg.sequence = msg.sequence;
                            resp_msg.length = resp_size;
                            resp_msg.data.assign(response.begin(), response.begin() + resp_size);

                            auto serialized = resp_msg.serialize();
                            srt_send(client_socket->get(), (char*)serialized.data(), serialized.size());
                        }
                    }
                } else {
                    std::cerr << "Camera " << static_cast<int>(msg.camera_id) << " not found or not connected" << std::endl;
                }
            }
            catch (const ViscaSrtException& e) {
                std::cerr << "Error handling client message: " << e.what() << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Unexpected error: " << e.what() << std::endl;
                break;
            }
        }
    }

    // Monitor VISCA cameras for responses
    void monitor_cameras() {
        std::vector<uint8_t> buffer(1500);
        
        while (running) {
            std::lock_guard<std::mutex> lock(cameras_mutex);
            for (auto& camera_pair : cameras) {
                if (!camera_pair.second.connected) {
                    if (!connect_to_camera(camera_pair.second)) {
                        continue;
                    }
                }

                // Check for VISCA responses
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(camera_pair.second.socket_fd, &readfds);

                struct timeval tv = {0, 10000}; // 10ms timeout
                if (select(camera_pair.second.socket_fd + 1, &readfds, nullptr, nullptr, &tv) > 0) {
                    int received = recv(camera_pair.second.socket_fd, buffer.data(), buffer.size(), 0);
                    if (received > 0) {
                        // Queue response for sending over SRT
                        ViscaMessage msg;
                        msg.camera_id = camera_pair.first;
                        msg.sequence = 0; // Response message
                        msg.length = received;
                        msg.data.assign(buffer.begin(), buffer.begin() + received);

                        std::lock_guard<std::mutex> q_lock(queues_mutex);
                        message_queues[camera_pair.first].push(msg);
                    }
                    else if (received <= 0) {
                        // Connection lost
                        close(camera_pair.second.socket_fd);
                        camera_pair.second.connected = false;
                    }
                }
            }
        }
    }

public:
    explicit ViscaSrtServer(const std::string& config_path) {
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

        // Load camera configurations
        if (config_.contains("cameras")) {
            for (const auto& cam_config : config_["cameras"]) {
                ViscaCamera camera;
                camera.name = cam_config["name"].get<std::string>();
                camera.ip_address = cam_config["ip_address"].get<std::string>();
                camera.port = cam_config["port"].get<uint16_t>();
                uint8_t id = cam_config["id"].get<uint8_t>();
                cameras[id] = camera;
            }
        }
    }

    ~ViscaSrtServer() {
        stop();
        srt_cleanup();
    }

    // Print server configuration
    void print_config() const {
        std::cout << "VISCA-SRT Server Configuration:" << std::endl;
        std::cout << "Bind Address: " << config_["bind_address"].get<std::string>() << std::endl;
        std::cout << "SRT Port: " << config_["srt_port"].get<uint16_t>() << std::endl;
        std::cout << "\nConfigured Cameras:" << std::endl;
        
        for (const auto& [id, camera] : cameras) {
            std::cout << "Camera " << static_cast<int>(id) << ": " 
                      << camera.name << " (" 
                      << camera.ip_address << ":" 
                      << camera.port << ")" << std::endl;
        }
    }

    // Initialize NDI tally configuration from JSON
    void init_ndi_tally_config() {
        for (auto& [id, camera] : cameras) {
            if (camera.ndi_mapping.ndi_source_name.empty() && 
                config_["cameras"][id-1].contains("ndi_mapping")) {
                const auto& mapping = config_["cameras"][id-1]["ndi_mapping"];
                camera.ndi_mapping.ndi_source_name = mapping["source_name"].get<std::string>();
                camera.ndi_mapping.camera_id = id;
                
                // Load tally commands
                const auto& cmds = mapping["commands"];
                camera.ndi_mapping.program_tally_command = cmds["program"].get<std::vector<uint8_t>>();
                camera.ndi_mapping.preview_tally_command = cmds["preview"].get<std::vector<uint8_t>>();
                camera.ndi_mapping.tally_off_command = cmds["off"].get<std::vector<uint8_t>>();
            }
        }
    }

    // Start the server
    void start() {
        // Create and configure server socket
        server_socket_ = std::make_unique<SrtSocket>();
        
        // Apply SRT settings from config
        if (config_.contains("srt_settings")) {
            server_socket_->set_options(config_["srt_settings"]);
        }

        // Initialize NDI tally configuration
        init_ndi_tally_config();

        // Start NDI tally handling thread if enabled
        if (config_.contains("ndi_settings")) {
            ndi_tally_thread_ = std::thread(&ViscaSrtServer::handle_ndi_tally, this);
        }

        // Bind and listen
        const std::string& bind_addr = config_["bind_address"].get<std::string>();
        uint16_t port = config_["srt_port"].get<uint16_t>();
        
        server_socket_->bind(bind_addr, port);
        server_socket_->listen(config_["srt_settings"]["max_clients"].get<int>());

        // Start camera monitoring thread
        std::thread monitor_thread(&ViscaSrtServer::monitor_cameras, this);
        monitor_thread.detach();

        print_config();
        std::cout << "\nVISCA-SRT server is running..." << std::endl;

        // Accept and handle clients
        while (running) {
            try {
                auto client_socket = server_socket_->accept();
                if (!client_socket || !running) {
                    continue;
                }

                // Configure client socket with server settings
                if (config_.contains("srt_settings")) {
                    client_socket->set_options(config_["srt_settings"]);
                }

                std::thread client_thread([this, socket = std::move(client_socket)]() {
                    handle_srt_client(std::move(socket));
                });
                client_thread.detach();
            }
            catch (const ViscaSrtException& e) {
                std::cerr << "Client connection error: " << e.what() << std::endl;
            }
        }
    }

    // Process VISCA message
    void process_visca_message(const ViscaMessage& msg, SRTSOCKET client_socket) {
        std::lock_guard<std::mutex> lock(cameras_mutex);
        auto it = cameras.find(msg.camera_id);
        if (it != cameras.end() && it->second.connected) {
            // Send command to camera
            if (send(it->second.socket_fd, msg.data.data(), msg.data.size(), 0) < 0) {
                std::cerr << "Failed to send command to camera " << it->second.name << std::endl;
                return;
            }

            // Wait for and forward response if it's a command or inquiry
            if (msg.type == MessageType::COMMAND || msg.type == MessageType::INQUIRY) {
                std::vector<uint8_t> response(1500);
                int resp_size = recv(it->second.socket_fd, response.data(), response.size(), 0);
                
                if (resp_size > 0) {
                    ViscaMessage resp_msg;
                    resp_msg.type = MessageType::RESPONSE;
                    resp_msg.camera_id = msg.camera_id;
                    resp_msg.sequence = msg.sequence;
                    resp_msg.length = resp_size;
                    resp_msg.data.assign(response.begin(), response.begin() + resp_size);

                    auto serialized = resp_msg.serialize();
                    srt_send(client_socket, (char*)serialized.data(), serialized.size());
                }
            }
        } else {
            std::cerr << "Camera " << static_cast<int>(msg.camera_id) << " not found or not connected" << std::endl;
        }
    }

    // Stop the server
    void stop() {
        if (!running) return;
        
        running = false;
        std::cout << "Stopping VISCA-SRT server..." << std::endl;

        // Wait for NDI tally thread to finish
        if (ndi_tally_thread_.joinable()) {
            ndi_tally_thread_.join();
        }

        // Close SRT server socket
        server_socket_.reset();

        // Close all camera connections
        {
            std::lock_guard<std::mutex> lock(cameras_mutex);
            for (auto& camera_pair : cameras) {
                if (camera_pair.second.connected) {
                    std::cout << "Disconnecting camera: " << camera_pair.second.name << std::endl;
                    close(camera_pair.second.socket_fd);
                    camera_pair.second.connected = false;
                }
            }
            cameras.clear();
        }

        // Clear NDI tally states
        {
            std::lock_guard<std::mutex> lock(ndi_tally_mutex_);
            ndi_tally_states_.clear();
        }

        std::cout << "Server stopped" << std::endl;
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
    std::cout << "  -c, --config <path>    Path to configuration file (default: /etc/visca_srt/server_config.json)" << std::endl;
    std::cout << "  -h, --help             Show this help message" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string config_path = "/etc/visca_srt/server_config.json";

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
        ViscaSrtServer server(config_path);
        server.start();

        // Wait for shutdown signal
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "\nShutting down..." << std::endl;
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
