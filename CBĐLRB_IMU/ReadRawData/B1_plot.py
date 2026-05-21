import serial, time
import numpy as np
import matplotlib.pyplot as plt

PORT = "COM9"
BAUD = 115200
N = 500

try:
    ser = serial.Serial(PORT, BAUD, timeout=2)
    time.sleep(2) # Đợi ESP32 khởi động lại
    ser.flushInput()
except Exception as e:
    print(f"Không thể mở cổng {PORT}. Hãy tắt Serial Monitor trên Arduino IDE đi nhé!")
    exit()

data_phys = []

print("==================================================")
print(f"Đang chờ dữ liệu từ {PORT}...")
print("!!! BẤM NÚT 'EN' (hoặc 'RST') TRÊN MẠCH ESP32 NGAY BÂY GIỜ !!!")
print("==================================================")

while len(data_phys) < N:
    line = ser.readline().decode(errors='ignore').strip()
    
    # Bỏ qua dòng trống
    if not line:
        continue
        
    try:
        vals = list(map(float, line.split(',')))
        if len(vals) == 12:
            data_phys.append(vals[6:]) # Chỉ lấy 6 cột dữ liệu vật lý (gia tốc và gyro)
            
            # In tiến độ cho dễ theo dõi
            if len(data_phys) % 50 == 0:
                print(f" Đã thu thập {len(data_phys)} / {N} mẫu...")
    except ValueError:
        # Bỏ qua các dòng chữ text (header, footer)
        pass

ser.close()
print("\nHoàn tất thu thập! Đang xử lý đồ thị...")

# Xử lý mảng dữ liệu
phys_arr = np.array(data_phys)
ax, ay, az = phys_arr[:,0], phys_arr[:,1], phys_arr[:,2]
gx, gy, gz = phys_arr[:,3], phys_arr[:,4], phys_arr[:,5]

# Vẽ đồ thị khảo sát
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
fig.suptitle('Đặc tính Tĩnh của MPU6050 (Gia tốc và Con quay)', fontsize=14)

ax1.plot(ax, color='r', label='Ax')
ax1.plot(ay, color='g', label='Ay')
ax1.plot(az, color='b', label='Az')
ax1.axhline(1.0, color='k', linestyle='--', label='Lý tưởng Az=1g')
ax1.set_title("Gia tốc kế (Accelerometer - Đơn vị: g)")
ax1.set_ylabel('g')
ax1.legend(loc='upper right')
ax1.grid(True)

ax2.plot(gx, color='r', label='Gx')
ax2.plot(gy, color='g', label='Gy')
ax2.plot(gz, color='b', label='Gz')
ax2.set_title("Con quay hồi chuyển (Gyroscope - Đơn vị: độ/giây)")
ax2.set_xlabel('Mẫu đo (Sample)')
ax2.set_ylabel('dps')
ax2.legend(loc='upper right')
ax2.grid(True)

plt.tight_layout()
plt.savefig('Dac_Tinh_Tinh.png', dpi=150)
plt.show()