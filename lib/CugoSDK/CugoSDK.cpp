#include "CugoSDK.h"
#include "Arduino.h"
#include <math.h>

RPI_PICO_Timer ITimer0(0);

// モード指定
int cugo_old_runmode = CUGO_CMD_MODE;
int cugo_runmode = CUGO_CMD_MODE;
bool cugo_switching_reset = true;

//カウント関連
long int cugo_current_count_L = 0;
long int cugo_current_count_R = 0;
long int cugo_prev_encoder_L = 0;
long int cugo_prev_encoder_R = 0;

//ld2関連
volatile long cugo_ld2_id = 0;
volatile long cugo_ld2_feedback_hz = 0;
volatile long cugo_ld2_feedback_dutation = 0;
const long int cugo_ld2_index_tofreq[3] = { 10, 50, 100 };

//初期化関数
void cugo_init() {
  Serial.begin(115200, SERIAL_8N1);   //PCとの通信
  delay(1000);                        //LD-2起動待機
  Serial1.begin(115200, SERIAL_8N1);  //BLDCとの通信

  ld2_set_feedback(2, 0b10000001);  //freq{0:10[hz] 1:50[hz] 2:100[hz]} kindof_data{0b1:Mode 0b10:CMD_RPM 0b100:CurrentRPM 0b1000:AveCurrentRPM 0b10000000:EncorderData}
  delay(1000);
  if (!(ITimer0.attachInterruptInterval(cugo_ld2_feedback_dutation * 1000, cugo_timer_handler0))) {
    //Serial.println(F("Can't set ITimer0. Select another freq. or timer"));
  }
  delay(1000);
  ld2_set_control_mode(CUGO_CMD_MODE);
  delay(1000);
}

bool cugo_timer_handler0(struct repeating_timer* t) {
  ld2_get_cmd();
  return true;
}

void cugo_rpm_direct_instructions(float left, float right) {
  unsigned char frame[10] = { 0xFF, 0x02, 0, 0, 0, 0, 0, 0, 0, 0 };
  ld2_float_to_frame(left, 2, frame);
  ld2_float_to_frame(right, 6, frame);
  ld2_write_cmd(frame);
}

//------------------------------------便利関数
void ld2_float_to_frame(float data, long int start, unsigned char* index) {  //配列indexの4番目からfloat dataを書き込む場合-> FloatToInt(data, 4, index);
  memcpy(&index[start], &data, 4);
}
void ld2_frame_to_float(unsigned char* index, long int start, float* data) {  //配列indexの3番目からfloat dataに書き込む場合-> ld2_frame_to_float(index, 3, data);
  memcpy(data, &index[start], 4);
}
void ld2_frame_to_short(unsigned char* index, long int start, short* data) {  //配列indexの3番目からuint16_t dataに書き込む場合-> ld2_frame_to_float(index, 3, data);
  memcpy(data, &index[start], 2);
}

//------------------------------------通信関係
void ld2_write_cmd(unsigned char cmd[10]) {  //引数はidとチェックサム以外の配列
  long int i;
  unsigned char checksum = cugo_ld2_id;
  for (i = 0; i < 10; i++) {
    Serial1.write(cmd[i]);
    checksum += (unsigned char)(cmd[i]);
  }
  Serial1.write(cugo_ld2_id);
  Serial1.write(checksum);
  cugo_ld2_id++;
  if (cugo_ld2_id > 0xFF) cugo_ld2_id = 0;
}

void ld2_get_cmd() {  //引数はidとチェックサム以外の配列
  unsigned char frame[12];
  while (Serial1.available() >= 12) {
    while (Serial1.read() != 0xFF) {
    }
    frame[0] = 0xFF;
    for (long int i = 1; i < 12; i++) {
      frame[i] = Serial1.read();
    }
    /*
    if(frame[1] == 0x8E){  //5.5.6 Encoder Feedback
      ld2_set_encorder(frame);
    }*/

    if (frame[1] == 0x80) {  //5.5.1 Control Mode Feedback
      if (frame[2] == 0x00) {
        if (cugo_switching_reset) {
          if (cugo_old_runmode == CUGO_CMD_MODE) {
            //Serial.println(F("###   RESETTING........     ###"));
            cugo_reset();
          } else if (cugo_old_runmode == CUGO_RC_MODE) {
            cugo_runmode = CUGO_RC_MODE;
          } else {
          }
        } else {
          if (cugo_old_runmode == CUGO_CMD_MODE) {
            cugo_runmode = CUGO_RC_MODE;
            cugo_old_runmode = CUGO_RC_MODE;
            //Serial.println(F("###   MODE:CUGO_RC_MODE     ###"));

          } else if (cugo_old_runmode == CUGO_RC_MODE) {
            cugo_runmode = CUGO_RC_MODE;
          } else {
          }
        }
      } else if (frame[2] == 0x01) {
        if (cugo_old_runmode == CUGO_RC_MODE) {
          cugo_runmode = CUGO_CMD_MODE;
          cugo_old_runmode = CUGO_CMD_MODE;
          //Serial.println(F("###   MODE:CUGO_CMD_MODE    ###"));
        } else if (cugo_old_runmode == CUGO_CMD_MODE) {
          cugo_runmode = CUGO_CMD_MODE;
        } else {
        }
      }
    } else if (frame[1] == 0x82) {  //5.5.2 CMD RPM Feedback

    } else if (frame[1] == 0x84) {  //5.5.3 Current RPM Feedback
      //ld2_frame_to_float(frame,2,&rpm_current_L);
      //ld2_frame_to_float(frame,4,&rpm_current_R);
    } else if (frame[1] == 0x86) {  //5.5.4 Average RPM Feedback

    } else if (frame[1] == 0x8D) {  //5.5.5 SBUS Signal Feedback

    } else if (frame[1] == 0x8E) {  //5.5.6 Encoder Feedback
      ld2_set_encorder(frame);

    } else if (frame[1] == 0x8F) {  //Data Feedback Config

    } else {
    }
  }
}

void ld2_set_encorder(unsigned char frame[12]) {
  short encoderR = 0, encoderL = 0;
  ld2_frame_to_short(frame, 2, &encoderL);
  ld2_frame_to_short(frame, 4, &encoderR);

  int diff_L = encoderL - cugo_prev_encoder_L;
  int diff_R = encoderR - cugo_prev_encoder_R;
  // オーバーフロー処理
  if (diff_L < -CUGO_LD2_COUNT_MAX / 2) {
    diff_L += CUGO_LD2_COUNT_MAX;
  } else if (diff_L > CUGO_LD2_COUNT_MAX / 2) {
    diff_L -= CUGO_LD2_COUNT_MAX;
  }

  if (diff_R < -CUGO_LD2_COUNT_MAX / 2) {
    diff_R += CUGO_LD2_COUNT_MAX;
  } else if (diff_R > CUGO_LD2_COUNT_MAX / 2) {
    diff_R -= CUGO_LD2_COUNT_MAX;
  }

  cugo_current_count_L += diff_L;
  cugo_current_count_R += diff_R;
  cugo_prev_encoder_L = encoderL;
  cugo_prev_encoder_R = encoderR;
}

void ld2_set_feedback(unsigned char freq_index, unsigned char kindof_data) {  //freq 0:10[hz] 1:50[hz] 2:100[hz] kindof_data 1:Mode 2:CMD_RPM 4:CurrentRPM 8:AveCurrentRPM 128:EncorderData
  unsigned char frame[10] = { 0xFF, 0x0F, freq_index, kindof_data, 0, 0, 0, 0, 0, 0 };
  cugo_ld2_feedback_hz = cugo_ld2_index_tofreq[freq_index];
  cugo_ld2_feedback_dutation = 1000 / cugo_ld2_feedback_hz;
  ld2_write_cmd(frame);
}

void ld2_set_control_mode(unsigned char mode) {  //mode 0x00:RC_mode 0x01:CMD_Mode
  unsigned char frame[10] = { 0xFF, 0x00, mode, 0, 0, 0, 0, 0, 0, 0 };
  cugo_runmode = mode;
  ld2_write_cmd(frame);
}

void cugo_reset() {
  watchdog_reboot(0, 0, 0);
  while (true)
    ;
}
