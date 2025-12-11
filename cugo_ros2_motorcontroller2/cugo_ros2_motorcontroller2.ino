#include "CugoSDK.h"
#include <Servo.h>
#include <PacketSerial.h>

#define SERIAL_BIN_BUFF_SIZE 64
#define SERIAL_HEADER_SIZE 8
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

// モーターのRPM値を格納するための構造体
struct MotorRPM {
  float left;
  float right;
};

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

void write_float_to_buf(uint8_t* buf, const int TARGET, float val) {
  uint8_t* val_ptr = reinterpret_cast<uint8_t*>(&val);
  memmove(buf + TARGET, val_ptr, sizeof(float));
}

void write_int_to_buf(uint8_t* buf, const int TARGET, int val) {
  uint8_t* val_ptr = reinterpret_cast<uint8_t*>(&val);
  memmove(buf + TARGET, val_ptr, sizeof(int));
}

void write_bool_to_buf(uint8_t* buf, const int TARGET, bool val) {
  uint8_t* val_ptr = reinterpret_cast<uint8_t*>(&val);
  memmove(buf + TARGET, val_ptr, sizeof(bool));
}

float read_float_from_buf(uint8_t* buf, const int TARGET) {
  float val = *reinterpret_cast<float*>(buf + SERIAL_HEADER_SIZE + TARGET);
  return val;
}

int read_int_from_buf(uint8_t* buf, const int TARGET) {
  int val = *reinterpret_cast<int*>(buf + SERIAL_HEADER_SIZE + TARGET);
  return val;
}

bool read_bool_from_buf(uint8_t* buf, const int TARGET) {
  bool val = *reinterpret_cast<bool*>(buf + SERIAL_HEADER_SIZE + TARGET);
  return val;
}

uint8_t read_uint8_t_from_buf(uint8_t* buf, const int TARGET) {
  uint8_t val = *reinterpret_cast<uint8_t*>(buf + SERIAL_HEADER_SIZE + TARGET);
  return val;
}

uint16_t read_uint16_t_from_header(uint8_t* buf, const int TARGET) {
  if (TARGET >= SERIAL_HEADER_SIZE - 1) return 0;
  uint16_t val = *reinterpret_cast<uint16_t*>(buf + TARGET);
  return val;
}

void set_motor_cmd_binary(uint8_t* reciev_buf, int size, float max_rpm) {
  if (size > 0) {
    MotorRPM reciev_rpm, clamped_rpm;
    reciev_rpm.left = read_float_from_buf(reciev_buf, TARGET_RPM_L_PTR);
    reciev_rpm.right = read_float_from_buf(reciev_buf, TARGET_RPM_R_PTR);

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

MotorRPM clamp_rpm_simple(MotorRPM target_rpm, float max_rpm) {
  MotorRPM new_rpm = target_rpm;
  if (abs(target_rpm.left) >= max_rpm) {
    new_rpm.left = target_rpm.left / abs(target_rpm.left) * max_rpm;
  }
  // 右モータ速度を監視。上限を超えてたら上限速度に丸める。
  if (abs(target_rpm.right) >= max_rpm) {
    new_rpm.right = target_rpm.right / abs(target_rpm.right) * max_rpm;
  }
  return new_rpm;
}

MotorRPM clamp_rpm_rotation_priority(MotorRPM target_rpm, float max_rpm) {
  // L/Rモータ速度を監視して、上限速度を超えると上限速度にクランプするコードを使用していた
  // すると、直進しながら急旋回する動きで、ロボットを回転させる速度がクランプロジックで消えてしまうことがあった
  // これにより、回避指示をしているのに障害物に激突することがあった
  // ここでは、L/Rのrpmを直進成分と回転成分を抜き出し、直進だけ減速させ回転成分を殺さないように速度上限クランプを行う処理をおこなう。

  // --- ステップ1: 目標RPMを並進速度(v_trans)と角速度(v_rot)に分解 ---
  float v_trans = (target_rpm.right + target_rpm.left) / 2.0f;
  float v_rot = (target_rpm.right - target_rpm.left) / 2.0f;

  // --- ステップ2: 角速度(v_rot)自体の上限処理 ---
  // そもそも角速度が速すぎると、並進速度を0にしても片輪が上限を超えてしまう。（並進、回転両方上限以上のとき）
  // そのため、最初にv_rotを[-max_rpm, max_rpm]の範囲に丸める。
  float clamped_v_rot = max(-max_rpm, min(max_rpm, v_rot));

  // --- ステップ3: 回転(clamped_v_rot)を維持するために、並進(v_trans)の上限を計算 ---
  // ここでは、上限処理済みの `clamped_v_rot` を使用する。
  float v_trans_limit = max_rpm - std::abs(clamped_v_rot);

  // --- ステップ4: 並進速度(v_trans)を上限値(v_trans_limit)に丸める ---
  // v_trans_limitは常に0以上になるため、よりシンプルなclamp処理が可能。
  float clamped_v_trans = max(-v_trans_limit, min(v_trans_limit, v_trans));

  // --- ステップ5: 丸めたv_transとv_rotから、最終的なRPMを再計算 ---
  // ここでも、上限処理済みの `clamped_v_rot` を使用する。
  MotorRPM new_rpm;
  new_rpm.left = clamped_v_trans - clamped_v_rot;
  new_rpm.right = clamped_v_trans + clamped_v_rot;

  return new_rpm;
}

uint16_t calculate_checksum(const void* data, size_t size, size_t start = 0) {
  uint32_t sum = 0;  // 桁あふれを考慮して32bitで計算
  const uint8_t* bytes = static_cast<const uint8_t*>(data);

  for (size_t i = start; i < size; i += 2) {
    // リトルエンディアンのバイト列から正しく16bit整数を復元
    // (上位バイト << 8) | 下位バイト
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

void create_serial_packet(uint8_t* packet, uint16_t* header, uint8_t* body) {
  size_t offset = 0;
  memmove(packet, header, sizeof(uint8_t) * SERIAL_HEADER_SIZE);
  offset += sizeof(uint8_t) * SERIAL_HEADER_SIZE;
  memmove(packet + offset, body, sizeof(uint8_t) * SERIAL_BIN_BUFF_SIZE);
}

float check_max_rpm(int product_id){
  if(product_id == 0){
    return CUGOV4_MAX_MOTOR_RPM;
  } else if(product_id == 1){
    return CUGOV3i_MAX_MOTOR_RPM;
  } else {
    return CUGOV4_MAX_MOTOR_RPM;
  }
}

void onSerialPacketReceived(const uint8_t* buffer, size_t size) {
  uint8_t tempBuffer[size];
  memcpy(tempBuffer, buffer, size);
  // バッファにたまったデータを抜き出して制御に適用
  uint16_t product_id = read_uint16_t_from_header(tempBuffer, RECV_HEADER_PRODUCT_ID_PTR);
  float max_rpm = check_max_rpm(product_id);
  // チェックサムの確認
  uint16_t recv_checksum = read_uint16_t_from_header(tempBuffer, RECV_HEADER_CHECKSUM_PTR);
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