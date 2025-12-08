#include <Arduino.h>
#include "CugoSDK.h"
#include "MotorLogic.h"
#include "SerialProtocol.h"
#include <Servo.h>
#include <PacketSerial.h>

uint8_t packetBinaryBufferSerial[SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE];
PacketSerial packetSerial;

// 受信バッファ
#define RECV_HEADER_PRODUCT_ID_PTR 0  // ヘッダ プロダクトID
#define RECV_HEADER_CHECKSUM_PTR 6    // ヘッダ チェックサム
#define TARGET_RPM_L_PTR 0            // 左モータ目標RPM
#define TARGET_RPM_R_PTR 4            // 右モータ目標RPM

// 送信バッファ
#define SEND_ENCODER_L_PTR 0  // 左エンコーダ回転数
#define SEND_ENCODER_R_PTR 4  // 右エンコーダ回転数

unsigned long long current_time = 0, prev_time_10ms = 0, prev_time_100ms, prev_time_1000ms;  // オーバーフローしても問題ないが64bit確保

// FAIL SAFE COUNT
int COM_FAIL_COUNT = 0;

void stop_motor_immediately() {
  cugo_rpm_direct_instructions(0, 0);
}

void check_failsafe() {
  // 100msごとに通信の有無を確認
  // 5回連続(0.5秒)ROSからの通信が来なかった場合、直ちにロボットを停止する
  COM_FAIL_COUNT++;
  if (COM_FAIL_COUNT > 5) {
    stop_motor_immediately();
  }
}

void job_10ms() {
  // nothing
}

void job_100ms() {
  check_failsafe();
  // エンコーダカウントをSDKから取得
  ld2_get_cmd();
}

void job_1000ms() {
  //nothing
}

void set_motor_cmd_binary(uint8_t* reciev_buf, int size, float max_rpm) {
  if (size > 0) {
    MotorRPM reciev_rpm, clamped_rpm;
    reciev_rpm.left = read_float_from_buf(reciev_buf, SERIAL_HEADER_SIZE, TARGET_RPM_L_PTR);
    reciev_rpm.right = read_float_from_buf(reciev_buf, SERIAL_HEADER_SIZE, TARGET_RPM_R_PTR);

    // 物理的最高速以上のときは、モータの最高速に丸める
    bool rotation_clanp_logic = true;  // 回転成分を優先した丸めアルゴリズムを有効化。falseにすると上限rpmだけに注目したシンプルなロジックに切り替わる。
    if (rotation_clanp_logic) {        // 回転成分を優先して残し、直進方向を減らす方法で速度上限以上の速度を丸める。曲がりきれず激突することを防止する。
      clamped_rpm = clamp_rpm_rotation_priority(reciev_rpm, max_rpm);
    } else {
      clamped_rpm = clamp_rpm_simple(reciev_rpm, max_rpm);
    }
    cugo_rpm_direct_instructions(clamped_rpm.left, clamped_rpm.right);
    /*  モータに指令値を無事セットできたら、通信失敗カウンタをリセット
        毎回リセットすることで通常通信できる。
        10Hzで通信しているので、100msJOBでカウンタアップ。
    */
    COM_FAIL_COUNT = 0;
  } else {
    cugo_rpm_direct_instructions(0.0, 0.0);
  }
}

void onSerialPacketReceived(const uint8_t* buffer, size_t size) {
  uint8_t tempBuffer[size];
  memcpy(tempBuffer, buffer, size);
  // バッファにたまったデータを抜き出して制御に適用
  uint16_t product_id = read_uint16_t_from_header(tempBuffer, SERIAL_HEADER_SIZE, RECV_HEADER_PRODUCT_ID_PTR);
  float max_rpm = check_max_rpm(product_id);
  // チェックサムの確認
  uint16_t recv_checksum = read_uint16_t_from_header(tempBuffer, SERIAL_HEADER_SIZE, RECV_HEADER_CHECKSUM_PTR);
  // ボディ部分へのポインタを取得
  const uint8_t* body_ptr = tempBuffer + SERIAL_HEADER_SIZE;
  // ボディ部分(64バイト)だけを渡してチェックサムを計算
  uint16_t calc_checksum = calculate_checksum(body_ptr, SERIAL_BIN_BUFF_SIZE);

  if (recv_checksum != calc_checksum) {
    //Serial.println("Packet integrity check failed");
  } else {
    set_motor_cmd_binary(tempBuffer, size, max_rpm);
  }

  // 送信ボディの作成
  uint8_t send_body[SERIAL_BIN_BUFF_SIZE];
  // 送信ボディの初期化
  memset(send_body, 0, sizeof(send_body));

  // ボディへ送信データの書き込み
  write_int_to_buf(send_body, SEND_ENCODER_L_PTR, cugo_current_count_L);
  write_int_to_buf(send_body, SEND_ENCODER_R_PTR, cugo_current_count_R);

  // チェックサムの計算
  uint16_t checksum = calculate_checksum(send_body, SERIAL_BIN_BUFF_SIZE);
  uint16_t send_len = SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE;
  // 送信ヘッダの作成
  uint16_t localPort = 8888;
  uint16_t send_header[4] = { localPort, 8888, send_len, checksum };

  // 送信パケットの作成
  uint8_t send_packet[send_len];
  create_serial_packet(send_packet, send_header, send_body);
  packetSerial.send(send_packet, send_len);
}

void setup() {
  //プロポでラジコンモード切替時に初期化したい場合はtrue、初期化しない場合はfalse
  cugo_switching_reset = false;

  cugo_init();  //初期設定
  packetSerial.begin(115200);
  packetSerial.setStream(&Serial);
  packetSerial.setPacketHandler(&onSerialPacketReceived);

  // Serialバッファをカラにしてから実行を開始する
  delay(100);
  while (Serial.available() > 0) {
    Serial.read();
  }
}

void loop() {
  current_time = micros();

  if (current_time - prev_time_10ms > 10000) {
    job_10ms();
    prev_time_10ms = current_time;
  }

  if (current_time - prev_time_100ms > 100000) {
    job_100ms();
    prev_time_100ms = current_time;
  }

  if (current_time - prev_time_1000ms > 1000000) {
    job_1000ms();
    prev_time_1000ms = current_time;
  }

  //// シリアル通信でコマンドを受信するとき
  packetSerial.update();
  // 受信バッファのオーバーフローチェック(optional).
  if (packetSerial.overflow()) {
    //Serial.print("serial packet overflow!!");
  }
}
