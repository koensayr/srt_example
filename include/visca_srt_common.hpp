#pragma once

#include <srt.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <json.hpp> // Using nlohmann/json
#include <memory>
#include <chrono>

// Use nlohmann::json for configuration parsing
using json = nlohmann::json;

namespace visca_srt {

// Message types for protocol
enum class MessageType : uint8_t {
    VISCA = 0x01,
    NDI_TALLY = 0x02
};

// Common error types
class ViscaSrtException : public std::runtime_error {
public:
    explicit ViscaSrtException(const std::string& msg) : std::runtime_error(msg) {}
};

// VISCA message types
enum class ViscaMessageType : uint8_t {
    COMMAND = 0x01,
    RESPONSE = 0x02,
    INQUIRY = 0x03,
    ERROR = 0xFF
};

// Base class for all messages
struct BaseMessage {
    MessageType protocol_type;
    
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual ~BaseMessage() = default;
};

// Structure for SRT-encapsulated VISCA message
struct ViscaMessage : BaseMessage {
    ViscaMessageType type;
    uint8_t camera_id;
    uint16_t sequence;
    uint16_t length;
    std::vector<uint8_t> data;

    ViscaMessage() {
        protocol_type = MessageType::VISCA;
    }

    // Serialize message to binary format
    std::vector<uint8_t> serialize() const override {
        std::vector<uint8_t> buffer(data.size() + 7);  // +1 for protocol type
        buffer[0] = static_cast<uint8_t>(protocol_type);
        buffer[1] = static_cast<uint8_t>(type);
        buffer[2] = camera_id;
        *(uint16_t*)(buffer.data() + 3) = htons(sequence);
        *(uint16_t*)(buffer.data() + 5) = htons(length);
        std::copy(data.begin(), data.end(), buffer.begin() + 7);
        return buffer;
    }

    // Deserialize from binary format
    static ViscaMessage deserialize(const std::vector<uint8_t>& buffer) {
        if (buffer.size() < 7) {
            throw ViscaSrtException("Message too short for header");
        }

        ViscaMessage msg;
        msg.protocol_type = MessageType::VISCA;
        msg.type = static_cast<ViscaMessageType>(buffer[1]);
        msg.camera_id = buffer[2];
        msg.sequence = ntohs(*(uint16_t*)(buffer.data() + 3));
        msg.length = ntohs(*(uint16_t*)(buffer.data() + 5));

        if (buffer.size() < msg.length + 7) {
            throw ViscaSrtException("Message data incomplete");
        }

        msg.data.assign(buffer.begin() + 7, buffer.begin() + 7 + msg.length);
        return msg;
    }
};

// SRT Socket wrapper with RAII
class SrtSocket {
private:
    SRTSOCKET socket_;
    bool connected_;

public:
    SrtSocket() : socket_(SRT_INVALID_SOCK), connected_(false) {
        socket_ = srt_create_socket();
        if (socket_ == SRT_INVALID_SOCK) {
            throw ViscaSrtException("Failed to create SRT socket");
        }
    }

    ~SrtSocket() {
        if (socket_ != SRT_INVALID_SOCK) {
            srt_close(socket_);
        }
    }

    // Delete copy constructor and assignment
    SrtSocket(const SrtSocket&) = delete;
    SrtSocket& operator=(const SrtSocket&) = delete;

    SRTSOCKET get() const { return socket_; }
    bool is_connected() const { return connected_; }

    void set_options(const json& config) {
        int yes = 1;
        srt_setsockopt(socket_, 0, SRTO_RCVSYN, &yes, sizeof(yes));
        srt_setsockopt(socket_, 0, SRTO_SNDSYN, &yes, sizeof(yes));

        if (config.contains("latency")) {
            int latency = config["latency"].get<int>();
            srt_setsockopt(socket_, 0, SRTO_LATENCY, &latency, sizeof(latency));
        }

        if (config.contains("max_bw")) {
            int64_t max_bw = config["max_bw"].get<int64_t>();
            srt_setsockopt(socket_, 0, SRTO_MAXBW, &max_bw, sizeof(max_bw));
        }
    }

    void connect(const std::string& host, uint16_t port) {
        sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &sa.sin_addr);

        if (srt_connect(socket_, (sockaddr*)&sa, sizeof(sa)) == SRT_ERROR) {
            throw ViscaSrtException("Failed to connect SRT socket");
        }
        connected_ = true;
    }

    void bind(const std::string& address, uint16_t port) {
        sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &sa.sin_addr);

        if (srt_bind(socket_, (sockaddr*)&sa, sizeof(sa)) == SRT_ERROR) {
            throw ViscaSrtException("Failed to bind SRT socket");
        }
    }

    void listen(int backlog = 5) {
        if (srt_listen(socket_, backlog) == SRT_ERROR) {
            throw ViscaSrtException("Failed to listen on SRT socket");
        }
        connected_ = true;
    }

    std::unique_ptr<SrtSocket> accept() {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SRTSOCKET client_sock = srt_accept(socket_, (sockaddr*)&client_addr, &addr_len);
        
        if (client_sock == SRT_INVALID_SOCK) {
            return nullptr;
        }

        auto client_socket = std::make_unique<SrtSocket>();
        client_socket->socket_ = client_sock;
        client_socket->connected_ = true;
        return client_socket;
    }
};

// VISCA command utilities
namespace visca_util {
    // Common VISCA command patterns
    const uint8_t COMMAND_PREFIX = 0x81;
    const uint8_t INQUIRY_PREFIX = 0x82;
    const uint8_t RESPONSE_PREFIX = 0x90;
    const uint8_t TERMINATOR = 0xFF;

    // Helper to validate VISCA message
    bool validate_message(const std::vector<uint8_t>& data) {
        if (data.empty()) return false;
        if (data[0] != COMMAND_PREFIX && 
            data[0] != INQUIRY_PREFIX && 
            data[0] != RESPONSE_PREFIX) return false;
        if (data.back() != TERMINATOR) return false;
        return true;
    }
}

} // namespace visca_srt
