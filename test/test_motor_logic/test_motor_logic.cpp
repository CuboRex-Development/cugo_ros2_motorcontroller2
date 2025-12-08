#include <unity.h>
#include "MotorLogic.h"

void setUp(void) {}
void tearDown(void) {}

// ========================================
// check_max_rpm のテスト
// ========================================

void test_check_max_rpm_product_id_0_returns_v4_rpm(void) {
    float result = check_max_rpm(0);
    TEST_ASSERT_EQUAL_FLOAT(CUGOV4_MAX_MOTOR_RPM, result);
}

void test_check_max_rpm_product_id_1_returns_v3i_rpm(void) {
    float result = check_max_rpm(1);
    TEST_ASSERT_EQUAL_FLOAT(CUGOV3i_MAX_MOTOR_RPM, result);
}

void test_check_max_rpm_unknown_product_id_returns_v4_rpm(void) {
    float result = check_max_rpm(99);
    TEST_ASSERT_EQUAL_FLOAT(CUGOV4_MAX_MOTOR_RPM, result);
}

// ========================================
// clamp_rpm_simple のテスト
// ========================================

void test_clamp_rpm_simple_within_limit_unchanged(void) {
    MotorRPM input = {50.0f, 50.0f};
    MotorRPM result = clamp_rpm_simple(input, 130.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result.right);
}

void test_clamp_rpm_simple_over_limit_clamped(void) {
    MotorRPM input = {150.0f, 150.0f};
    MotorRPM result = clamp_rpm_simple(input, 130.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.right);
}

void test_clamp_rpm_simple_negative_over_limit_clamped(void) {
    MotorRPM input = {-150.0f, -150.0f};
    MotorRPM result = clamp_rpm_simple(input, 130.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -130.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -130.0f, result.right);
}

void test_clamp_rpm_simple_mixed_values(void) {
    MotorRPM input = {50.0f, 150.0f};
    MotorRPM result = clamp_rpm_simple(input, 130.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result.left);   // 変更なし
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.right); // クランプ
}

// ========================================
// clamp_rpm_rotation_priority のテスト
// ========================================

void test_clamp_rpm_rotation_priority_within_limit_unchanged(void) {
    MotorRPM input = {50.0f, 50.0f};
    MotorRPM result = clamp_rpm_rotation_priority(input, 130.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result.right);
}

void test_clamp_rpm_rotation_priority_straight_over_limit(void) {
    // 直進のみ（回転成分なし）で上限超過
    MotorRPM input = {150.0f, 150.0f};
    MotorRPM result = clamp_rpm_rotation_priority(input, 130.0f);

    // 回転成分0なので、両輪とも130にクランプ
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.right);
}

void test_clamp_rpm_rotation_priority_preserves_rotation(void) {
    // 右旋回しながら前進（左100, 右160）
    // v_trans = (160 + 100) / 2 = 130
    // v_rot = (160 - 100) / 2 = 30
    // 回転を維持して直進を減速
    MotorRPM input = {100.0f, 160.0f};
    float max_rpm = 130.0f;
    MotorRPM result = clamp_rpm_rotation_priority(input, max_rpm);

    // 回転成分30を維持、直進は100にクランプ
    // left = 100 - 30 = 70, right = 100 + 30 = 130
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 70.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 130.0f, result.right);
}

void test_clamp_rpm_rotation_priority_both_exceed_limit(void) {
    // 両輪とも上限超過の場合
    MotorRPM input = {200.0f, 300.0f};
    float max_rpm = 130.0f;
    MotorRPM result = clamp_rpm_rotation_priority(input, max_rpm);

    // 結果が最大RPMを超えないことを確認
    TEST_ASSERT_TRUE(std::abs(result.left) <= max_rpm + 0.01f);
    TEST_ASSERT_TRUE(std::abs(result.right) <= max_rpm + 0.01f);
}

void test_clamp_rpm_rotation_priority_pure_rotation(void) {
    // その場旋回（直進成分なし）
    MotorRPM input = {-100.0f, 100.0f};
    float max_rpm = 130.0f;
    MotorRPM result = clamp_rpm_rotation_priority(input, max_rpm);

    // 上限内なので変更なし
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, result.right);
}

void test_clamp_rpm_rotation_priority_reverse(void) {
    // 後退
    MotorRPM input = {-150.0f, -150.0f};
    float max_rpm = 130.0f;
    MotorRPM result = clamp_rpm_rotation_priority(input, max_rpm);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, -130.0f, result.left);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -130.0f, result.right);
}

// ========================================
// メイン
// ========================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // check_max_rpm tests
    RUN_TEST(test_check_max_rpm_product_id_0_returns_v4_rpm);
    RUN_TEST(test_check_max_rpm_product_id_1_returns_v3i_rpm);
    RUN_TEST(test_check_max_rpm_unknown_product_id_returns_v4_rpm);

    // clamp_rpm_simple tests
    RUN_TEST(test_clamp_rpm_simple_within_limit_unchanged);
    RUN_TEST(test_clamp_rpm_simple_over_limit_clamped);
    RUN_TEST(test_clamp_rpm_simple_negative_over_limit_clamped);
    RUN_TEST(test_clamp_rpm_simple_mixed_values);

    // clamp_rpm_rotation_priority tests
    RUN_TEST(test_clamp_rpm_rotation_priority_within_limit_unchanged);
    RUN_TEST(test_clamp_rpm_rotation_priority_straight_over_limit);
    RUN_TEST(test_clamp_rpm_rotation_priority_preserves_rotation);
    RUN_TEST(test_clamp_rpm_rotation_priority_both_exceed_limit);
    RUN_TEST(test_clamp_rpm_rotation_priority_pure_rotation);
    RUN_TEST(test_clamp_rpm_rotation_priority_reverse);

    return UNITY_END();
}
