# Waterpump IoT (ESP32 + Node-RED)

## 1) ESP32 Firmware
- Mở `esp32/firmware.ino`
- Sửa: WIFI_SSID/PASS, MQTT_HOST/PORT/USER/PASS
- Nạp vào ESP32. GPIO:
  - RELAY_PIN = 26 (LED test: HIGH = ON)
  - HC-SR04: TRIG=23, ECHO=22 (ECHO qua chia áp 2:1 về 3.3V)
- AUTO: ON < 5cm, OFF >= 18cm
- OFFSET_CM: bù khoảng cách cảm biến -> miệng bồn

## 2) Node-RED on Render
- Push repo lên GitHub
- Render -> New Web Service -> chọn repo
- Runtime: Docker | Add Disk mount `/data`
- Env Vars:
  - NR_ADMIN_USER / NR_ADMIN_HASH (bcrypt)
  - NR_HTTP_USER  / NR_HTTP_HASH  (bcrypt)
- Sau deploy: mở `https://<app>.onrender.com/ui`
- Import `nodered/flows.json` (hoặc paste flow hiện có)
- Chỉnh MQTT node trỏ tới MQTT Cloud của bạn

## 3) MQTT Cloud
- Tạo EMQX Cloud/HiveMQ Cloud free
- Lấy host/port/user/pass
- Node-RED & ESP32 cùng trỏ về broker này
