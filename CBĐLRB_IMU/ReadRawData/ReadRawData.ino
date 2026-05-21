#include <Wire.h>

#define MPU_ADDR 0x68
#define SAMPLE_COUNT 500

// Cấu trúc dữ liệu theo thanh ghi MPU6500 [cite: 79, 80]
struct RawData {
  int16_t ax, ay, az;
  int16_t temp;
  int16_t gx, gy, gz;
};

// Sử dụng biến toàn cục để tránh lỗi tràn bộ nhớ Stack trên ESP32
float samples_ax[SAMPLE_COUNT], samples_ay[SAMPLE_COUNT], samples_az[SAMPLE_COUNT];
float samples_gx[SAMPLE_COUNT], samples_gy[SAMPLE_COUNT], samples_gz[SAMPLE_COUNT];

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // Thoát khỏi chế độ Sleep
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); // ACCEL_CONFIG
  Wire.write(0x00); // Thiết lập ±2g [cite: 85]
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); // GYRO_CONFIG
  Wire.write(0x00); // Thiết lập ±250 dps [cite: 85]
  Wire.endTransmission();
}

RawData readRaw() {
  RawData d;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // Thanh ghi bắt đầu ACCEL_XOUT_H 
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true); // Đọc 14 byte liên tiếp 

  // Ghép byte High và Low (signed 16-bit) [cite: 80]
  d.ax = (Wire.read() << 8) | Wire.read();
  d.ay = (Wire.read() << 8) | Wire.read();
  d.az = (Wire.read() << 8) | Wire.read();
  d.temp = (Wire.read() << 8) | Wire.read();
  d.gx = (Wire.read() << 8) | Wire.read();
  d.gy = (Wire.read() << 8) | Wire.read();
  d.gz = (Wire.read() << 8) | Wire.read();
  return d;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Chân I2C mặc định của ESP32
  mpuInit();
  delay(500);

  double sum_ax = 0, sum_ay = 0, sum_az = 0;
  double sum_gx = 0, sum_gy = 0, sum_gz = 0;

  // Header định dạng CSV để Python xử lý
  Serial.println("raw_ax,raw_ay,raw_az,raw_gx,raw_gy,raw_gz,acc_x,acc_y,acc_z,gy_x,gy_y,gy_z");

  // Vòng lặp 1: Thu thập dữ liệu và tính tổng
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    RawData r = readRaw();

    // Quy đổi sang đơn vị vật lý [cite: 85, 86, 90]
    samples_ax[i] = r.ax / 16384.0;
    samples_ay[i] = r.ay / 16384.0;
    samples_az[i] = r.az / 16384.0;
    samples_gx[i] = r.gx / 131.0;
    samples_gy[i] = r.gy / 131.0;
    samples_gz[i] = r.gz / 131.0;

    // Xuất 12 cột dữ liệu
    Serial.printf("%d,%d,%d,%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", 
                  r.ax, r.ay, r.az, r.gx, r.gy, r.gz, 
                  samples_ax[i], samples_ay[i], samples_az[i],
                  samples_gx[i], samples_gy[i], samples_gz[i]);

    sum_ax += samples_ax[i]; sum_ay += samples_ay[i]; sum_az += samples_az[i];
    sum_gx += samples_gx[i]; sum_gy += samples_gy[i]; sum_gz += samples_gz[i];
    delay(5); // Tần số lấy mẫu ~200Hz
  }

  // Tính giá trị trung bình (Mean / Zero-offset) 
  float mean_ax = sum_ax / SAMPLE_COUNT;
  float mean_ay = sum_ay / SAMPLE_COUNT;
  float mean_az = sum_az / SAMPLE_COUNT;
  float mean_gx = sum_gx / SAMPLE_COUNT;
  float mean_gy = sum_gy / SAMPLE_COUNT;
  float mean_gz = sum_gz / SAMPLE_COUNT;

  // Vòng lặp 2: Tính toán độ lệch chuẩn (STD - Noise) 
  double var_ax = 0, var_ay = 0, var_az = 0;
  double var_gx = 0, var_gy = 0, var_gz = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    var_ax += pow(samples_ax[i] - mean_ax, 2);
    var_ay += pow(samples_ay[i] - mean_ay, 2);
    var_az += pow(samples_az[i] - mean_az, 2);
    var_gx += pow(samples_gx[i] - mean_gx, 2);
    var_gy += pow(samples_gy[i] - mean_gy, 2);
    var_gz += pow(samples_gz[i] - mean_gz, 2);
  }

  float std_ax = sqrt(var_ax / SAMPLE_COUNT);
  float std_ay = sqrt(var_ay / SAMPLE_COUNT);
  float std_az = sqrt(var_az / SAMPLE_COUNT);
  float std_gx = sqrt(var_gx / SAMPLE_COUNT);
  float std_gy = sqrt(var_gy / SAMPLE_COUNT);
  float std_gz = sqrt(var_gz / SAMPLE_COUNT);

  // Xuất kết quả thống kê để làm báo cáo Đặc tính tĩnh 
  Serial.println("\n--- KET QUA THONG KE DAC TINH TINH ---");
  
  Serial.println("[ACCELEROMETER]");
  Serial.printf("Mean (Zero-offset) [g] : Ax=%.5f, Ay=%.5f, Az=%.5f\n", mean_ax, mean_ay, mean_az);
  Serial.printf("Noise (STD)        [g] : Ax=%.5f, Ay=%.5f, Az=%.5f\n", std_ax, std_ay, std_az);
  
  Serial.println("\n[GYROSCOPE]");
  Serial.printf("Mean (Drift Rate) [dps]: Gx=%.5f, Gy=%.5f, Gz=%.5f\n", mean_gx, mean_gy, mean_gz);
  Serial.printf("Noise (STD)       [dps]: Gx=%.5f, Gy=%.5f, Gz=%.5f\n", std_gx, std_gy, std_gz);
}

void loop() {}
