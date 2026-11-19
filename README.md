# HOUSE INNOVATION — IoT Starter Kit

Paket ini berisi:
- `web/index.html`: dashboard web yang berkomunikasi dengan broker MQTT melalui WebSocket TLS.
- `esp32_firmware/house_innovation_esp32.ino`: firmware ESP32 untuk membaca sensor dan mengendalikan 3 lampu DC.

## Arsitektur
Browser/HP -- MQTT over WebSocket TLS (8884) --> MQTT broker <-- MQTT over TLS (8883) -- ESP32
Topic:
- `houseinnovation/rumah-miniatur-01/telemetry`
- `houseinnovation/rumah-miniatur-01/status`
- `houseinnovation/rumah-miniatur-01/command`

## A. Perangkat yang disarankan
1. ESP32 DevKit V1 (ESP-WROOM-32)
2. DHT22
3. Modul sensor air dengan output digital (DO)
4. MQ-2 smoke/gas module
5. Magnetic reed switch
6. Relay 3/4 channel yang kompatibel 3.3V logic (periksa spesifikasi input)
7. Buzzer aktif 3.3V/5V sesuai modul
8. Lampu miniatur DC tegangan rendah + catu daya yang sesuai
9. Breadboard dan kabel jumper

Mulai dari lampu DC 5V/12V, bukan listrik PLN.

## B. Wiring default firmware
| Komponen | Pin ESP32 |
|---|---:|
| DHT22 DATA | GPIO 4 |
| MQ-2 AO | GPIO 34 (ADC1) |
| Water sensor DO | GPIO 27 |
| Reed switch | GPIO 26 ke GND |
| Buzzer | GPIO 25 |
| Relay ruang tamu IN1 | GPIO 16 |
| Relay kamar IN2 | GPIO 17 |
| Relay teras IN3 | GPIO 18 |
| Semua GND | GND bersama (sesuai rancangan catu daya) |

DHT22: VCC ke 3V3, GND ke GND, DATA ke GPIO4. Jika sensor DHT22 bare (bukan modul), gunakan resistor pull-up 4.7k–10k antara DATA dan 3V3.

Reed switch: satu kabel ke GPIO26, satu ke GND. Firmware menganggap switch tertutup saat pintu tertutup.

Water module: gunakan DO. Firmware default menganggap LOW = basah. Uji dan ubah `WATER_WET_IS_LOW` bila logika modul berbeda.

MQ-2: modul biasanya diberi 5V dan dapat menghasilkan AO mendekati 5V. **Jangan sambungkan AO langsung ke ESP32.** Gunakan pembagi tegangan yang dihitung sesuai modul agar tegangan ke GPIO34 selalu di bawah 3.3V. MQ-2 perlu waktu pemanasan dan kalibrasi; angka threshold di firmware hanya ambang demo.

Relay: firmware default menganggap relay active LOW. Uji relay tanpa beban lampu terlebih dahulu. Pastikan input relay kompatibel dengan level logika ESP32 3.3V.

## C. Siapkan broker MQTT (contoh: HiveMQ Cloud)
1. Buat cluster gratis/akun pada HiveMQ Cloud.
2. Buat username dan password khusus prototipe.
3. Catat hostname cluster.
4. Browser memakai hostname tersebut dan port WebSocket TLS `8884`.
5. ESP32 memakai hostname tersebut dan port TLS MQTT `8883`.
6. Untuk penggunaan yang lebih aman, buat ACL agar akun browser hanya bisa subscribe telemetry/status dan publish command; akun ESP32 hanya publish telemetry/status dan subscribe command. Jika provider tidak mendukung ACL per user pada paket yang dipakai, gunakan akun khusus untuk prototipe dan jangan publikasikan kredensial.

## D. Jalankan aplikasi web
1. Buka `web/index.html` di Chrome/Edge. Jika browser membatasi file lokal, jalankan server lokal:
   - Windows: buka terminal di folder `web`, jalankan `py -m http.server 8000`
   - Buka `http://localhost:8000`
2. Klik **Pengaturan koneksi**.
3. Isi host, port `8884`, username, password, dan Device ID `rumah-miniatur-01`.
4. Simpan & hubungkan.

Kredensial broker disimpan di localStorage browser. Jangan gunakan kredensial produksi pada komputer bersama.

## E. Upload firmware ESP32
1. Install Arduino IDE.
2. Install board package `esp32` by Espressif Systems.
3. Install library PubSubClient, ArduinoJson, DHT sensor library, dan Adafruit Unified Sensor.
4. Buka file `.ino`.
5. Isi Wi-Fi SSID/password, MQTT host, username/password.
6. Pilih board ESP32 Dev Module dan port COM yang benar.
7. Upload, lalu buka Serial Monitor 115200 baud.
8. Setelah ESP32 tersambung, dashboard akan menerima data tiap 5 detik.

**Catatan TLS:** firmware starter menggunakan `secureClient.setInsecure()` untuk mempermudah uji koneksi awal. Koneksi tetap terenkripsi, tetapi sertifikat broker tidak diverifikasi. Jangan gunakan untuk instalasi permanen. Untuk deployment, pasang root CA broker dan gunakan `secureClient.setCACert(...)`.

## F. Uji bertahap
1. Uji ESP32 online dan data suhu/kelembapan tampil.
2. Uji reed switch dengan membuka/menutup magnet.
3. Uji water sensor dengan sedikit air pada area sensornya.
4. Uji MQ-2 hanya dengan prosedur aman dan tanpa membuat api/asap berbahaya.
5. Uji relay tanpa beban, lalu gunakan lampu DC.
6. Uji kendali lampu dari dashboard.
7. Aktifkan Away Mode. Firmware menjalankan pola lampu pada jam 18.00–22.59 WIB dan jadwalnya tetap berjalan lokal setelah perintah diterima.

## Batasan versi awal
- Belum ada login pengguna, push notification saat browser ditutup, histori cloud, grafik energi, maupun kontrol banyak rumah.
- Away Mode menggunakan pola acak terjadwal sederhana, bukan AI dan bukan jaminan rumah terlihat berpenghuni.
- Nilai MQ-2 yang ditampilkan adalah ADC mentah, bukan ppm.
- Status sensor dan alarm ini tidak boleh dipakai sebagai sistem keselamatan bangunan nyata.
