#include "visca_srt_common.hpp"
#include "ndi_tally_common.hpp"
#include <gtest/gtest.h>
#include <chrono>

using namespace visca_srt;

TEST(NdiTallyTest, MessageSerialization) {
    NdiTallyMessage msg;
    msg.ndi_source_name = "TestCam";
    msg.state = TallyState::PROGRAM;
    msg.timestamp = 1234567890;

    // Serialize message
    auto serialized = msg.serialize();

    // Verify protocol type
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(MessageType::NDI_TALLY));
    
    // Verify tally state
    EXPECT_EQ(serialized[1], static_cast<uint8_t>(TallyState::PROGRAM));
    
    // Verify name length
    EXPECT_EQ(serialized[2], 7); // Length of "TestCam"
    
    // Verify timestamp (big-endian)
    uint32_t stored_timestamp = (static_cast<uint32_t>(serialized[3]) << 24) |
                               (static_cast<uint32_t>(serialized[4]) << 16) |
                               (static_cast<uint32_t>(serialized[5]) << 8) |
                               static_cast<uint32_t>(serialized[6]);
    EXPECT_EQ(stored_timestamp, 1234567890);
    
    // Verify source name
    std::string stored_name(serialized.begin() + 7, serialized.end());
    EXPECT_EQ(stored_name, "TestCam");
}

TEST(NdiTallyTest, MessageDeserialization) {
    // Create test message buffer
    std::vector<uint8_t> buffer = {
        0x02,  // Protocol type (NDI_TALLY)
        0x01,  // State (PROGRAM)
        0x07,  // Name length
        0x49, 0x96, 0x02, 0xD2,  // Timestamp (1234567890 in big-endian)
        'T', 'e', 's', 't', 'C', 'a', 'm'  // Source name
    };

    auto msg = NdiTallyMessage::deserialize(buffer);
    
    EXPECT_EQ(msg.protocol_type, MessageType::NDI_TALLY);
    EXPECT_EQ(msg.state, TallyState::PROGRAM);
    EXPECT_EQ(msg.timestamp, 1234567890);
    EXPECT_EQ(msg.ndi_source_name, "TestCam");
}

TEST(NdiTallyTest, InvalidMessageDeserialization) {
    // Test with buffer too small for header
    std::vector<uint8_t> small_buffer = {0x02, 0x01, 0x07};
    EXPECT_THROW(NdiTallyMessage::deserialize(small_buffer), ViscaSrtException);

    // Test with invalid name length
    std::vector<uint8_t> invalid_length = {
        0x02,  // Protocol type
        0x01,  // State
        0xFF,  // Invalid length
        0x00, 0x00, 0x00, 0x00  // Timestamp
    };
    EXPECT_THROW(NdiTallyMessage::deserialize(invalid_length), ViscaSrtException);
}

TEST(NdiTallyTest, TallyStateValues) {
    EXPECT_EQ(static_cast<uint8_t>(TallyState::OFF), 0x00);
    EXPECT_EQ(static_cast<uint8_t>(TallyState::PROGRAM), 0x01);
    EXPECT_EQ(static_cast<uint8_t>(TallyState::PREVIEW), 0x02);
    EXPECT_EQ(static_cast<uint8_t>(TallyState::PROGRAM_PREVIEW), 0x03);
}

TEST(NdiTallyTest, ViscaCommandMapping) {
    NdiCameraMapping mapping;
    mapping.program_tally_command = {0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x02, 0xFF};
    mapping.preview_tally_command = {0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x01, 0xFF};
    mapping.tally_off_command = {0x81, 0x01, 0x7E, 0x01, 0x0A, 0x00, 0x03, 0xFF};

    // Verify command lengths
    EXPECT_EQ(mapping.program_tally_command.size(), 8);
    EXPECT_EQ(mapping.preview_tally_command.size(), 8);
    EXPECT_EQ(mapping.tally_off_command.size(), 8);

    // Verify command format (VISCA command structure)
    EXPECT_EQ(mapping.program_tally_command[0], 0x81);  // VISCA command start
    EXPECT_EQ(mapping.program_tally_command[7], 0xFF);  // VISCA command terminator
}
