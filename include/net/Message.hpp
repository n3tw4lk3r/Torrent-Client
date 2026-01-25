#pragma once

#include <cstdint>
#include <string>

enum class MessageId : uint8_t {
    kChoke = 0,
    kUnchoke,
    kInterested,
    kNotInterested,
    kHave,
    kBitField,
    kRequest,
    kPiece,
    kCancel,
    kPort,
    kKeepAlive,
};

struct Message {
    MessageId id;
    size_t message_length;
    std::string payload;

    static Message Parse(const std::string& message_string);
    static Message Init(MessageId id, const std::string& payload);
    std::string ToString() const;
};
