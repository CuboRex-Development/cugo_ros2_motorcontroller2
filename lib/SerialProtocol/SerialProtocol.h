#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// バッファサイズ
#define SERIAL_BIN_BUFF_SIZE 64
#define SERIAL_HEADER_SIZE 8

// チェックサム計算
uint16_t calculate_checksum(const void* data, size_t size, size_t start = 0);

// バッファへの書き込み
void write_float_to_buf(uint8_t* buf, int target, float val);
void write_int_to_buf(uint8_t* buf, int target, int val);
void write_bool_to_buf(uint8_t* buf, int target, bool val);

// バッファからの読み込み
float read_float_from_buf(const uint8_t* buf, int header_size, int target);
int read_int_from_buf(const uint8_t* buf, int header_size, int target);
bool read_bool_from_buf(const uint8_t* buf, int header_size, int target);
uint8_t read_uint8_t_from_buf(const uint8_t* buf, int header_size, int target);
uint16_t read_uint16_t_from_header(const uint8_t* buf, int header_size, int target);

// パケット作成
void create_serial_packet(uint8_t* packet, uint16_t* header, uint8_t* body);

#endif // SERIAL_PROTOCOL_H
