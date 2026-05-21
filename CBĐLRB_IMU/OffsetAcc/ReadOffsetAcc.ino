#include <Wire.h>

#define MPU_ADDR 0x68
#define SAMPLE_COUNT 200 // Yêu cầu đo 200 mẫu mỗi vị trí [cite: 98]

struct RawData {
  int16_t ax, ay, az, temp, gx, gy, gz;
};

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission();
}

RawData readRaw() {
  RawData d;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
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
  Wire.begin(21, 22);
  mpuInit();
  Serial.println("\n--- CHUONG TRINH HIEU CHINH ACCELEROMETER ---");
  Serial.println("Dat cam bien vao 1 trong 6 vi tri chuan.");
  Serial.println("Sau do, go phim bat ky (vd: '1') roi nhan Enter/Send de do 200 mau.");
}

void loop() {
  if (Serial.available() > 0) {
    // Đọc bỏ ký tự người dùng vừa nhập
    while (Serial.available() > 0) Serial.read(); 
    
    Serial.println("\nDang thu thap 200 mau. Vui long giu yen...");
    double sum_ax = 0, sum_ay = 0, sum_az = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
      RawData r = readRaw();
      sum_ax += (r.ax / 16384.0);
      sum_ay += (r.ay / 16384.0);
      sum_az += (r.az / 16384.0);
      delay(5);
    }

    Serial.println("=> HOAN TAT! Ghi lai cac gia tri Mean nay vao giay:");
    Serial.printf("Mean Ax = %.5f g\n", sum_ax / SAMPLE_COUNT);
    Serial.printf("Mean Ay = %.5f g\n", sum_ay / SAMPLE_COUNT);
    Serial.printf("Mean Az = %.5f g\n", sum_az / SAMPLE_COUNT);
    Serial.println("--------------------------------------------------");
    Serial.println("Hay xoay cam bien sang vi tri tiep theo va go phim bat ky de tiep tuc...");
  }
}
