#ifndef CUGOCMDMODE_H
#define CUGOCMDMODE_H

#include "RPi_Pico_TimerInterrupt.h"
#include "hardware/watchdog.h"

#define CUGOV4_MAX_MOTOR_RPM  130    //V4モータの速度上限値
#define CUGOV3i_MAX_MOTOR_RPM 180    //V3iモータの速度上限値

// 各種動作モード定義
#define CUGO_RC_MODE 0
#define CUGO_CMD_MODE 1

//LD2関連
#define CUGO_LD2_COUNT_MAX 65536
#define NVIC_SYSRESETREQ 2

//カウント関連
extern long int cugo_current_count_L;
extern long int cugo_current_count_R;

extern bool cugo_switching_reset;

// 全体制御
void cugo_init();
void cugo_reset();
bool cugo_timer_handler0(struct repeating_timer* t);

//モータ直接制御関連
void cugo_rpm_direct_instructions(float left, float right);

//LD2関連
//便利関数
void ld2_float_to_frame(float data, long int start, unsigned char* index);   //配列indexの4番目からfloat dataを書き込む場合-> FloatTolong int(data, 4, index);
void ld2_frame_to_float(unsigned char* index, long int start, float* data);  //配列indexの3番目からfloat dataに書き込む場合-> ld2_frame_to_float(index, 3, data);
void ld2_frame_to_short(unsigned char* index, long int start, short* data);  //配列indexの3番目からulong int16_t dataに書き込む場合-> ld2_frame_to_float(index, 3, data);
//通信関係
void ld2_write_cmd(unsigned char cmd[10]);
void ld2_get_cmd();
//設定
void ld2_set_feedback(unsigned char freq_index, unsigned char kindof_data);  //freq{0:10[hz] 1:50[hz] 2:100[hz]} kindof_data{0b1:Mode 0b10:CMD_RPM 0b100:CurrentRPM 0b1000:AveCurrentRPM 0b10000000:EncorderData}
void ld2_set_control_mode(unsigned char mode);                               //mode{0:RC_mode 1:CMD_Mode}
void ld2_set_encorder(unsigned char frame[12]);
void ld2_encoder_reset();
#endif