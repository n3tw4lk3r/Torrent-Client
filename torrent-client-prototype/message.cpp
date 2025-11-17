#include "message.h"
#include "byte_tools.h"

Message Message::Parse(const std::string& messageString) {
    size_t length = BytesToInt(messageString.substr(0, 4));
    if (length == 0) {
        return { MessageId(10), 0, "" };
    }

    uint8_t id = uint8_t((unsigned char) messageString[4]);
    std::string payload;
    if (id > 3) {
        payload = messageString.substr(5);
    }

    return { MessageId(id), length, payload };
}

Message Message::Init(MessageId id, const std::string& payload) {
    return { id, payload.size() + 1, payload };
}

std::string Message::ToString() const {
    std::string msgId;
    unsigned char ch = static_cast<uint8_t>(id) & 0xFF;
    msgId += ch;
    return IntToBytes(messageLength) + msgId + payload;
}