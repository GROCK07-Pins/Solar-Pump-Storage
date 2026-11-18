# Wiring & Pinout

Semua nomor pin di bawah adalah nomor **GPIO** ESP32 (bukan nomor kaki fisik), sesuai
`#define` di `SmartWater_Full_FlowLevel.ino`.

## Ringkasan Pin

| GPIO | Arah | Fungsi | Konstanta | Catatan |
|---|---|---|---|---|
| 0 | IN (pull-up) | Tombol reset WiFi | `WIFI_CONFIG_BUTTON` | Tombol **BOOT** di DevKit; tahan ≥ 5 s saat boot |
| 34 | IN (analog) | Sensor kelembapan | `HUMIDITY_PIN` | ADC1, *input-only*, aman dipakai bersama WiFi |
| 25 | OUT | Ultrasonik TRIG | `TRIG_PIN` | |
| 26 | IN | Ultrasonik ECHO | `ECHO_PIN` | Turunkan ke 3,3 V bila sensor 5 V |
| 27 | IN (pull-up, FALLING) | Flow sensor 1 | `FLOW1_PIN` | Interrupt |
| 33 | IN (pull-up, FALLING) | Flow sensor 2 | `FLOW2_PIN` | Interrupt |
| 32 | IN (pull-up, FALLING) | Sensor RPM | `RPM_PIN` | Interrupt |
| 16 | IN | RS485 RX (UART2) | `PZEM_RX_PIN` | Ke pin **RO/TXD** modul RS485 |
| 17 | OUT | RS485 TX (UART2) | `PZEM_TX_PIN` | Ke pin **DI/RXD** modul RS485 |
| 23 | OUT | Relay K1 — 3-way biru (BUKA) | `RELAY1_PIN` | Active LOW |
| 22 | OUT | Relay K2 — 3-way coklat (TUTUP) | `RELAY2_PIN` | Active LOW |
| 21 | OUT | Relay K3 — 2-way biru (BUKA) | `RELAY3_PIN` | Active LOW |
| 19 | OUT | Relay K4 — 2-way coklat (TUTUP) | `RELAY4_PIN` | Active LOW |
| 18 | OUT | Relay K5 | `RELAY5_PIN` | Active LOW |
| 5 | OUT | Relay K6 | `RELAY6_PIN` | Active LOW, *strapping pin* (lihat catatan) |
| 4 | OUT | Relay K7 | `RELAY7_PIN` | Active LOW |
| 13 | OUT | Relay K8 | `RELAY8_PIN` | Active LOW |

## Diagram Blok

```
                    +-----------------------+
  Kelembapan  ----> | GPIO34                |
  Ultrasonik  ----> | GPIO25 TRIG           |
              <---- | GPIO26 ECHO*          |
  Flow 1      ----> | GPIO27                |          +---------------------+
  Flow 2      ----> | GPIO33     ESP32      | GPIO23 -> | IN1  K1  3-way BIRU |
  RPM         ----> | GPIO32                | GPIO22 -> | IN2  K2  3-way COKL |
                    |                       | GPIO21 -> | IN3  K3  2-way BIRU |
  RS485 RO    ----> | GPIO16 (RX2)          | GPIO19 -> | IN4  K4  2-way COKL |
  RS485 DI    <---- | GPIO17 (TX2)          | GPIO18 -> | IN5  K5             |
                    |                       | GPIO5  -> | IN6  K6             |
  Tombol BOOT ----> | GPIO0                 | GPIO4  -> | IN7  K7             |
                    |                       | GPIO13 -> | IN8  K8             |
                    +-----------------------+          +---------------------+
                                                         Modul relay 8ch (active LOW)

  * ECHO 5 V -> pakai pembagi tegangan ke 3,3 V
```

## Bus RS485 — 4× PZEM-017

```
ESP32 GPIO17 (TX) ---> DI/RXD  +--------------+  A+ ----+-------+-------+-------+
ESP32 GPIO16 (RX) <--- RO/TXD  | Modul RS485  |         |       |       |       |
3V3 / 5V ------------> VCC     | (auto-dir)   |  B- ----+-------+-------+-------+
GND -----------------> GND     +--------------+       PZEM#1  PZEM#2  PZEM#3  PZEM#4
                                                       0x01    0x02    0x03    0x04
```

- Semua PZEM berbagi satu bus (UART2, **9600 baud, 8N1**).
- **Setiap PZEM wajib punya alamat unik** (0x01–0x04) *sebelum* dipasang bersamaan.
  Atur satu per satu (hanya satu PZEM di bus) dengan `setAddress()` dari library
  PZEM-017-v1, lalu pasang semuanya.
- Firmware **tidak** mengatur pin DE/RE, jadi gunakan modul RS485 dengan kontrol arah
  otomatis.
- **PZEM #3 (alamat 0x03)** dipakai sebagai sumber `batteryVoltage`.

| PZEM | Alamat | Indeks array | Peruntukan |
|---|---|---|---|
| #1 | 0x01 | `pzem*[0]` | *(isi sesuai instalasi)* |
| #2 | 0x02 | `pzem*[1]` | *(isi sesuai instalasi)* |
| #3 | 0x03 | `pzem*[2]` | **Baterai** — dipakai untuk `batteryVoltage` & `batteryPercent` |
| #4 | 0x04 | `pzem*[3]` | *(isi sesuai instalasi)* |

## Valve Motor

Setiap valve memiliki kabel **biru** (buka) dan **coklat** (tutup). Relay hanya memberi
daya selama 15 detik — cukup untuk motor valve bergerak penuh — lalu mati.

| Valve | Relay BUKA (biru) | Relay TUTUP (coklat) |
|---|---|---|
| 3-way | K1 (GPIO23) | K2 (GPIO22) |
| 2-way | K3 (GPIO21) | K4 (GPIO19) |

Interlock di firmware mencegah biru & coklat aktif bersamaan, tetapi tetap disarankan
memakai wiring yang aman secara fisik (misalnya kontak NO/NC yang tidak bisa memberi daya
ke dua arah sekaligus).

## Catatan Perangkat Keras

- **Level tegangan 3,3 V.** GPIO ESP32 tidak toleran 5 V. Untuk ECHO ultrasonik dan sinyal
  sensor flow/RPM yang keluar 5 V, gunakan pembagi tegangan (mis. 1 kΩ / 2 kΩ) atau level
  shifter.
- **GPIO34** adalah *input-only* dan tidak punya pull-up internal. Termasuk ADC1, jadi tetap
  bisa dibaca saat WiFi aktif (ADC2 tidak bisa).
- **GPIO5** adalah *strapping pin* dan dapat berkedip saat boot, sehingga relay K6 mungkin
  "klik" sesaat ketika ESP32 dinyalakan. Jangan pasang beban kritis di K6.
- **GPIO0** harus HIGH saat boot normal. Tombol BOOT bawaan DevKit sudah sesuai.
- **Catu daya terpisah** untuk relay/valve dan ESP32 sangat disarankan, dengan **GND
  disatukan**. Lonjakan arus motor valve dapat me-reset ESP32 bila satu sumber.
- Relay *active LOW*: `LOW` = ON, `HIGH` = OFF. Saat `setup()`, semua relay langsung diset
  OFF.
