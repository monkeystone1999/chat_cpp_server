#pragma once
#include <memory>
#include <string.h>
#include <string>
#include <vector>

enum class PacketType : uint16_t { Hello = 1, Message, Login, Exit, Enter };

#pragma pack(push, 1)
struct PacketHeader {
  PacketType type;
  uint16_t length;
};
struct MessageBody {
  char who[30];
  char Message[1024];
};

#pragma pack(pop)

PacketType getPacketType(std::string &data) {
  PacketHeader *h = reinterpret_cast<PacketHeader *>(&data);
  return h->type;
};

std::vector<uint8_t> SendPacket(PacketType type, const char *message,
                                const char *who) {
  std::vector<uint8_t> packet;
  PacketHeader *Header = reinterpret_cast<PacketHeader *>(packet.data());
  Header->type = type;
  Header->length = sizeof(uint16_t) * 2;
  MessageBody *Body =
      reinterpret_cast<MessageBody *>(packet.data() + sizeof(PacketHeader));
  int message_size = strlen(message), who_size = strlen(who);
  strncpy(Body->who, who, who_size);
  strncpy(Body->Message, message, message_size);
  return packet;
};

void RecvPacket(char *buffer) {
  PacketHeader *Header = reinterpret_cast<PacketHeader *>(buffer);
  switch (Header->type) {
  case PacketType::Hello:
    break;
  case PacketType::Message:
    break;
  case PacketType::Login:
    break;
  case PacketType::Exit:
    break;
  case PacketType::Enter:
    break;
  }
};
