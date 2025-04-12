#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace visca_srt {

// NDI Tally states
enum class TallyState : uint8_t {
    OFF = 0x00,
    PROGRAM = 0x01,
    PREVIEW = 0x02,
    PROGRAM_PREVIEW = 0x03
};

// Structure for NDI Tally message
struct NdiTallyMessage : BaseMessage {
    std::string ndi_source_name;
    TallyState state;
    uint32_t timestamp;

    NdiTallyMessage() {
        protocol_type = MessageType::NDI_TALLY;
    }

    // Serialize to binary format
    std::vector<uint8_t> serialize() const override {
        // Calculate required size
        size_t name_length = ndi_source_name.length();
        std::vector<uint8_t> buffer(name_length + 7);  // +1 for protocol type

        // Write protocol type and data
        buffer[0] = static_cast<uint8_t>(protocol_type);
        buffer[1] = static_cast<uint8_t>(state);
        buffer[2] = static_cast<uint8_t>(name_length);
        
        // Write timestamp (big-endian)
        buffer[3] = (timestamp >> 24) & 0xFF;
        buffer[4] = (timestamp >> 16) & 0xFF;
        buffer[5] = (timestamp >> 8) & 0xFF;
        buffer[6] = timestamp & 0xFF;

        // Copy source name
        std::copy(ndi_source_name.begin(), ndi_source_name.end(), buffer.begin() + 7);

        return buffer;
    }

    // Deserialize from binary format
    static NdiTallyMessage deserialize(const std::vector<uint8_t>& buffer) {
        if (buffer.size() < 7) {
            throw ViscaSrtException("Buffer too small for NDI tally message");
        }

        NdiTallyMessage msg;
        msg.protocol_type = MessageType::NDI_TALLY;
        msg.state = static_cast<TallyState>(buffer[1]);
        uint8_t name_length = buffer[2];

        // Read timestamp (big-endian)
        msg.timestamp = (static_cast<uint32_t>(buffer[3]) << 24) |
                       (static_cast<uint32_t>(buffer[4]) << 16) |
                       (static_cast<uint32_t>(buffer[5]) << 8) |
                       static_cast<uint32_t>(buffer[6]);

        // Read source name
        if (buffer.size() < 7 + name_length) {
            throw ViscaSrtException("Buffer too small for NDI source name");
        }
        msg.ndi_source_name.assign(buffer.begin() + 7, buffer.begin() + 7 + name_length);

        return msg;
    }
};

// Structure for mapping NDI sources to cameras
struct NdiCameraMapping {
    std::string ndi_source_name;
    uint8_t camera_id;
    bool tally_program_enabled{true};
    bool tally_preview_enabled{true};
    std::vector<uint8_t> program_tally_command;
    std::vector<uint8_t> preview_tally_command;
    std::vector<uint8_t> tally_off_command;
};

} // namespace visca_srt
