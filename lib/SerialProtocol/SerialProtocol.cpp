#include "SerialProtocol.h"
#include <cstring>

uint16_t calculate_checksum(const void* data, size_t size, size_t start) {
  uint32_t sum = 0;
  const uint8_t* bytes = static_cast<const uint8_t*>(data);

  for (size_t i = start; i < size; i += 2) {
    // リトルエンディアンのバイト列から16bit整数を復元
    uint16_t word = (static_cast<uint16_t>(bytes[i + 1]) << 8) | static_cast<uint16_t>(bytes[i]);
    sum += word;
  }

  // 桁あふれ処理 (キャリーを回収)
  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  // 1の補数 (ビット反転)
  return static_cast<uint16_t>(~sum);
}

void write_float_to_buf(uint8_t* buf, int target, float val) {
  std::memcpy(buf + target, &val, sizeof(float));
}

void write_int_to_buf(uint8_t* buf, int target, int val) {
  std::memcpy(buf + target, &val, sizeof(int));
}

void write_bool_to_buf(uint8_t* buf, int target, bool val) {
  std::memcpy(buf + target, &val, sizeof(bool));
}

float read_float_from_buf(const uint8_t* buf, int header_size, int target) {
  float val;
  std::memcpy(&val, buf + header_size + target, sizeof(float));
  return val;
}

int read_int_from_buf(const uint8_t* buf, int header_size, int target) {
  int val;
  std::memcpy(&val, buf + header_size + target, sizeof(int));
  return val;
}

bool read_bool_from_buf(const uint8_t* buf, int header_size, int target) {
  bool val;
  std::memcpy(&val, buf + header_size + target, sizeof(bool));
  return val;
}

uint8_t read_uint8_t_from_buf(const uint8_t* buf, int header_size, int target) {
  return buf[header_size + target];
}

uint16_t read_uint16_t_from_header(const uint8_t* buf, int header_size, int target) {
  if (target >= header_size - 1) return 0;
  uint16_t val;
  std::memcpy(&val, buf + target, sizeof(uint16_t));
  return val;
}

void create_serial_packet(uint8_t* packet, uint16_t* header, uint8_t* body) {
  std::memcpy(packet, header, SERIAL_HEADER_SIZE);
  std::memcpy(packet + SERIAL_HEADER_SIZE, body, SERIAL_BIN_BUFF_SIZE);
}
