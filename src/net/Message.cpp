#include "net/Message.hpp"

#include <stdexcept>

#include "utils/byte_tools.hpp"

Message Message::Parse(const std::string& message_string) {
    if (message_string.size() < 4) {
        throw std::runtime_error("Message too short to parse");
    }
    
    size_t length = utils::BytesToInt32(message_string.substr(0, 4));
    if (length == 0) {
        return { MessageId::kKeepAlive, 0, "" };
    }

    if (message_string.size() < 5) {
        throw std::runtime_error("Message too short for ID");
    }
    
    uint8_t id = uint8_t(static_cast<unsigned char>(message_string[4]));
    std::string payload;
    if (id > 3) {
        if (message_string.size() > 5) {
            payload = message_string.substr(5);
        }
    }

    return { static_cast<MessageId>(id), length, payload };
}

Message Message::Init(MessageId id, const std::string& payload) {
    return { id, payload.size() + 1, payload };
}

std::string Message::ToString() const {
    std::string message_id;
    unsigned char ch = static_cast<uint8_t>(id) & 0xFF;
    message_id += ch;
    return utils::Int32ToBytes(static_cast<int>(message_length)) + message_id + payload;
}
