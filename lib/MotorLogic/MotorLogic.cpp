#include "MotorLogic.h"

MotorRPM clamp_rpm_simple(MotorRPM target_rpm, float max_rpm) {
  MotorRPM new_rpm = target_rpm;
  if (std::abs(target_rpm.left) >= max_rpm) {
    new_rpm.left = target_rpm.left / std::abs(target_rpm.left) * max_rpm;
  }
  if (std::abs(target_rpm.right) >= max_rpm) {
    new_rpm.right = target_rpm.right / std::abs(target_rpm.right) * max_rpm;
  }
  return new_rpm;
}

MotorRPM clamp_rpm_rotation_priority(MotorRPM target_rpm, float max_rpm) {
  // --- ステップ1: 目標RPMを並進速度(v_trans)と角速度(v_rot)に分解 ---
  float v_trans = (target_rpm.right + target_rpm.left) / 2.0f;
  float v_rot = (target_rpm.right - target_rpm.left) / 2.0f;

  // --- ステップ2: 角速度(v_rot)自体の上限処理 ---
  float clamped_v_rot = std::max(-max_rpm, std::min(max_rpm, v_rot));

  // --- ステップ3: 回転を維持するために、並進の上限を計算 ---
  float v_trans_limit = max_rpm - std::abs(clamped_v_rot);

  // --- ステップ4: 並進速度を上限値に丸める ---
  float clamped_v_trans = std::max(-v_trans_limit, std::min(v_trans_limit, v_trans));

  // --- ステップ5: 最終的なRPMを再計算 ---
  MotorRPM new_rpm;
  new_rpm.left = clamped_v_trans - clamped_v_rot;
  new_rpm.right = clamped_v_trans + clamped_v_rot;

  return new_rpm;
}

float check_max_rpm(int product_id) {
  if (product_id == 0) {
    return CUGOV4_MAX_MOTOR_RPM;
  } else if (product_id == 1) {
    return CUGOV3i_MAX_MOTOR_RPM;
  } else {
    return CUGOV4_MAX_MOTOR_RPM;
  }
}
