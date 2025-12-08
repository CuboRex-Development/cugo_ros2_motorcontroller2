#ifndef MOTOR_LOGIC_H
#define MOTOR_LOGIC_H

#include <cmath>
#include <algorithm>

// モータの速度上限値
#define CUGOV4_MAX_MOTOR_RPM  130
#define CUGOV3i_MAX_MOTOR_RPM 180

// モーターのRPM値を格納するための構造体
struct MotorRPM {
  float left;
  float right;
};

// RPMクランプ関数
MotorRPM clamp_rpm_simple(MotorRPM target_rpm, float max_rpm);
MotorRPM clamp_rpm_rotation_priority(MotorRPM target_rpm, float max_rpm);

// 製品IDに基づく最大RPM取得
float check_max_rpm(int product_id);

#endif // MOTOR_LOGIC_H
