#include <unity.h>
#include "SerialProtocol.h"
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

// ========================================
// write/read float テスト
// ========================================

void test_write_and_read_float(void) {
    uint8_t buf[16] = {0};
    float expected = 123.456f;

    write_float_to_buf(buf, 4, expected);
    float result = read_float_from_buf(buf, 0, 4);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, result);
}

void test_write_and_read_float_negative(void) {
    uint8_t buf[16] = {0};
    float expected = -98.765f;

    write_float_to_buf(buf, 0, expected);
    float result = read_float_from_buf(buf, 0, 0);

    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, result);
}

// ========================================
// write/read int テスト
// ========================================

void test_write_and_read_int(void) {
    uint8_t buf[16] = {0};
    int expected = 12345;

    write_int_to_buf(buf, 4, expected);
    int result = read_int_from_buf(buf, 0, 4);

    TEST_ASSERT_EQUAL_INT(expected, result);
}

void test_write_and_read_int_negative(void) {
    uint8_t buf[16] = {0};
    int expected = -54321;

    write_int_to_buf(buf, 0, expected);
    int result = read_int_from_buf(buf, 0, 0);

    TEST_ASSERT_EQUAL_INT(expected, result);
}

// ========================================
// read_uint16_t_from_header テスト
// ========================================

void test_read_uint16_t_from_header(void) {
    uint8_t buf[8] = {0x34, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint16_t result = read_uint16_t_from_header(buf, 8, 0);

    TEST_ASSERT_EQUAL_UINT16(0x1234, result);
}

void test_read_uint16_t_from_header_out_of_bounds(void) {
    uint8_t buf[8] = {0};
    // target >= header_size - 1 の場合は0を返す
    uint16_t result = read_uint16_t_from_header(buf, 8, 7);

    TEST_ASSERT_EQUAL_UINT16(0, result);
}

// ========================================
// calculate_checksum テスト
// ========================================

void test_calculate_checksum_basic(void) {
    // 単純なテストケース
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint16_t result = calculate_checksum(data, sizeof(data));

    // 全部0なので、~0 = 0xFFFF
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, result);
}

void test_calculate_checksum_nonzero(void) {
    // 0x0102 + 0x0304 = 0x0406
    // ~0x0406 = 0xFBF9
    uint8_t data[] = {0x02, 0x01, 0x04, 0x03};
    uint16_t result = calculate_checksum(data, sizeof(data));

    TEST_ASSERT_EQUAL_UINT16(0xFBF9, result);
}

void test_calculate_checksum_with_start_offset(void) {
    uint8_t data[] = {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
    // start=2 から開始なので、0xFFFFは無視される
    uint16_t result = calculate_checksum(data, sizeof(data), 2);

    TEST_ASSERT_EQUAL_UINT16(0xFFFF, result);
}

// ========================================
// create_serial_packet テスト
// ========================================

void test_create_serial_packet(void) {
    uint16_t header[4] = {0x1111, 0x2222, 0x3333, 0x4444};
    uint8_t body[SERIAL_BIN_BUFF_SIZE];
    memset(body, 0xAA, SERIAL_BIN_BUFF_SIZE);

    uint8_t packet[SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE];
    create_serial_packet(packet, header, body);

    // ヘッダ部分を確認
    TEST_ASSERT_EQUAL_UINT8(0x11, packet[0]);
    TEST_ASSERT_EQUAL_UINT8(0x11, packet[1]);

    // ボディ部分を確認
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet[SERIAL_HEADER_SIZE]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, packet[SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE - 1]);
}

// ========================================
// メイン
// ========================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // write/read float tests
    RUN_TEST(test_write_and_read_float);
    RUN_TEST(test_write_and_read_float_negative);

    // write/read int tests
    RUN_TEST(test_write_and_read_int);
    RUN_TEST(test_write_and_read_int_negative);

    // read_uint16_t_from_header tests
    RUN_TEST(test_read_uint16_t_from_header);
    RUN_TEST(test_read_uint16_t_from_header_out_of_bounds);

    // calculate_checksum tests
    RUN_TEST(test_calculate_checksum_basic);
    RUN_TEST(test_calculate_checksum_nonzero);
    RUN_TEST(test_calculate_checksum_with_start_offset);

    // create_serial_packet tests
    RUN_TEST(test_create_serial_packet);

    return UNITY_END();
}
