#include <Wire.h>
#include <math.h>

#define MPU_ADDR 0x68
#define SAMPLE_COUNT 500

// --- 1. CỐ ĐỊNH BIẾN HIỆU CHỈNH GIA TỐC (ĐÃ ĐO THỰC NGHIỆM Ở BƯỚC TRƯỚC) ---
const float OFFSET_AX = 0.035075;
const float OFFSET_AY = 0.024395;
const float OFFSET_AZ = 0.029585;

// --- 2. NGƯỠNG ĐÁNH GIÁ (THEO BAREM TÀI LIỆU HƯỚNG DẪN) ---
const float GYRO_STATIONARY_THRESH = 0.15; // Ngưỡng STD để xác định mạch đứng yên (dps)
const float TEMP_DEVIATION_THRESH = 3.0;   // Ngưỡng lệch nhiệt độ để kích hoạt hiệu chỉnh lại (°C)

// Biến lưu trữ hằng số hiệu chỉnh động cho Gyro
float gyro_offset_x = 0, gyro_offset_y = 0, gyro_offset_z = 0;
float base_temperature = 0; // Nhiệt độ gốc lúc hiệu chỉnh xong

struct RawData {
  int16_t ax, ay, az;
  int16_t temp;
  int16_t gx, gy, gz;
};

// Mảng lưu dữ liệu phục vụ tính STD
float sample_gx[SAMPLE_COUNT], sample_gy[SAMPLE_COUNT], sample_gz[SAMPLE_COUNT];

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(); // Thoát Sleep
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(); // Accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00); Wire.endTransmission(); // Gyro ±250 dps
}

RawData readRaw() {
  RawData d;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
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

// Hàm quy đổi nhiệt độ chip MPU6500 sang độ C
float getTemperatureCelsius(int16_t raw_temp) {
  return (raw_temp / 333.87) + 21.0;
}

// --- THUẬT TOÁN 1: TỰ ĐỘNG HIỆU CHỈNH GYRO KHI PHÁT HIỆN TRẠNG THÁI YÊN ---
void autoCalibrateGyro() {
  bool is_stationary = false;
  
  while (!is_stationary) {
    Serial.println("\n[CALIBRATION] Dang quet trang thai tinh... Vui long giu yen mach!");
    double sum_gx = 0, sum_gy = 0, sum_gz = 0;
    double sum_temp = 0;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
      RawData r = readRaw();
      sample_gx[i] = r.gx / 131.0;
      sample_gy[i] = r.gy / 131.0;
      sample_gz[i] = r.gz / 131.0;
      
      sum_gx += sample_gx[i];
      sum_gy += sample_gy[i];
      sum_gz += sample_gz[i];
      sum_temp += getTemperatureCelsius(r.temp);
      delay(5);
    }

    // Tính Mean ban đầu
    float mean_gx = sum_gx / SAMPLE_COUNT;
    float mean_gy = sum_gy / SAMPLE_COUNT;
    float mean_gz = sum_gz / SAMPLE_COUNT;

    // Tính STD để kiểm tra độ tĩnh
    double var_gx = 0, var_gy = 0, var_gz = 0;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
      var_gx += pow(sample_gx[i] - mean_gx, 2);
      var_gy += pow(sample_gy[i] - mean_gy, 2); // Sử dụng cấu trúc biến phương sai tương tự bước khảo sát tĩnh
      var_gz += pow(sample_gz[i] - mean_gz, 2);
    }
    float std_gx = sqrt(var_gx / SAMPLE_COUNT);
    float std_gy = sqrt(var_gy / SAMPLE_COUNT);
    float std_gz = sqrt(var_gz / SAMPLE_COUNT);

    Serial.printf("[CHECK] Độ rung STD hiên tai: Gx=%.4f, Gy=%.4f, Gz=%.4f\n", std_gx, std_gy, std_gz);

    // Điều kiện phát hiện trạng thái yên: Nhiễu ngẫu nhiên của cả 3 trục phải nhỏ hơn ngưỡng cho phép
    if (std_gx < GYRO_STATIONARY_THRESH && std_gy < GYRO_STATIONARY_THRESH && std_gz < GYRO_STATIONARY_THRESH) {
      is_stationary = true;
      gyro_offset_x = mean_gx;
      gyro_offset_y = mean_gy;
      gyro_offset_z = mean_gz;
      base_temperature = sum_temp / SAMPLE_COUNT;
      
      Serial.println("==================================================");
      Serial.println("[SUCCESS] Da thiet lap xong Offset Gyro!");
      Serial.printf("Offsets: Gx=%.5f, Gy=%.5f, Gz=%.5f\n", gyro_offset_x, gyro_offset_y, gyro_offset_z);
      Serial.printf("Nhiet do moi truong on dinh: %.2f °C\n", base_temperature);
      Serial.println("==================================================");
    } else {
      Serial.println("[WARNING] Mach bi rung hoac dich chuyen! Dang quet lai sau 1 giay...");
      delay(1000);
    }
  }
}

// --- THUẬT TOÁN 2: KIỂM TRA CHẤT LƯỢNG SAU HIỆU CHỈNH ---
void verifyCalibration() {
  Serial.println("[VERIFY] Dang kiem tra lai chat luong tin hieu sau khi bu sai so...");
  delay(500);
  
  double sum_gx = 0, sum_gy = 0, sum_gz = 0;
  double sum_ax = 0, sum_ay = 0, sum_az = 0;
  int check_samples = 100;

  for (int i = 0; i < check_samples; i++) {
    RawData r = readRaw();
    
    sum_gx += ((r.gx / 131.0) - gyro_offset_x);
    sum_gy += ((r.gy / 131.0) - gyro_offset_y);
    sum_gz += ((r.gz / 131.0) - gyro_offset_z);

    sum_ax += (r.ax / 16384.0) + OFFSET_AX;
    sum_ay += (r.ay / 16384.0) + OFFSET_AY;
    sum_az += (r.az / 16384.0) + OFFSET_AZ;
    delay(5);
  }

  float ver_gx = fabs(sum_gx / check_samples);
  float ver_gy = fabs(sum_gy / check_samples);
  float ver_gz = fabs(sum_gz / check_samples);
  
  float ax_cal = sum_ax / check_samples;
  float ay_cal = sum_ay / check_samples;
  float az_cal = sum_az / check_samples;
  
  // Tính tổng trị tuyệt đối vector gia tốc |a|
  float total_a = sqrt(ax_cal*ax_cal + ay_cal*ay_cal + az_cal*az_cal);

  Serial.println("\n--- KET QUA KIEM TRA SAU HIEU CHINH ---");
  Serial.printf("Sai so Gyro trung binh (Yeu cau < 0.05 dps):\n -> Gx=%.5f, Gy=%.5f, Gz=%.5f\n", ver_gx, ver_gy, ver_gz);
  Serial.printf("Tong vector gia toc |a| (Yeu cau bám sat 1.0g): %.5f g\n", total_a);

  // Điều kiện kiểm tra
  if (ver_gx < 0.05 && ver_gy < 0.05 && ver_gz < 0.05 && fabs(total_a - 1.0) < 0.1) {
    Serial.println("[STATUS] ĐAT CHUAN! Hệ thong tin hieu san sang.");
  } else {
    Serial.println("[STATUS] CANH BAO: Thong so sau hiệu chỉnh chua toi uu. Kiem tra lai do gá cơ khi.");
  }
  Serial.println("--------------------------------------------------\n");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  mpuInit();
  delay(500);
  
  // Chạy chu trình tự động hiệu chỉnh và kiểm chứng khi khởi động
  autoCalibrateGyro();
  verifyCalibration();
}

void loop() {
  RawData r = readRaw();
  
  // Lấy giá trị vật lý đã được làm sạch
  float ax_cal = (r.ax / 16384.0) + OFFSET_AX;
  float ay_cal = (r.ay / 16384.0) + OFFSET_AY;
  float az_cal = (r.az / 16384.0) + OFFSET_AZ;
  
  float gx_cal = (r.gx / 131.0) - gyro_offset_x;
  float gy_cal = (r.gy / 131.0) - gyro_offset_y;
  float gz_cal = (r.gz / 131.0) - gyro_offset_z;
  
  float current_temp = getTemperatureCelsius(r.temp);

  // --- THUẬT TOÁN 3: KIỂM TRA BIẾN ĐỘNG NHIỆT ĐỘ (TEMPERATURE COMPENSATION) ---
  if (fabs(current_temp - base_temperature) > TEMP_DEVIATION_THRESH) {
    Serial.printf("\n[ALERT] Nhiet do thay doi vuot nguong (Delta T = %.2f °C). Kích hoat hieu chinh lai Gyro...\n", fabs(current_temp - base_temperature));
    autoCalibrateGyro();
    verifyCalibration();
  }

  // In dữ liệu ra Serial theo định dạng sạch phục vụ tính toán góc ở phần tiếp theo
  Serial.printf("Acc_Cal:%.3f,%.3f,%.3f | Gyro_Cal:%.3f,%.3f,%.3f | Temp:%.1f\n", 
                ax_cal, ay_cal, az_cal, gx_cal, gy_cal, gz_cal, current_temp);
                
  delay(20); // Chu kỳ lấy mẫu loop chính ~50Hz
}
