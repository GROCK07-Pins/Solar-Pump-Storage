# Panduan Kalibrasi

Semua konstanta kalibrasi berada di bagian atas `SmartWater_Full_FlowLevel.ino`. Setelah
mengubah nilai, upload ulang firmware lalu cek hasilnya di Serial Monitor (115200 baud).

| Sensor | Konstanta | Nilai saat ini | Status |
|---|---|---|---|
| Kelembapan | `HUMIDITY_DRY`, `HUMIDITY_WET` | 4095, 1900 | Perlu dicek |
| Ultrasonik | `TINGGI_TOREN`, `TINGGI_MAKS`, `JARAK_MIN` | 125, 105, 20 cm | **Provisional** |
| Flow | `FLOW_FACTOR` | 5.5 Hz per L/menit | **Provisional** |
| RPM | `RPM_PULSE_PER_REV`, `MAX_RPM` | 1, 10000 | Sesuaikan |
| Baterai | tabel `calculateBatteryPercent()` | kurva LiFePO4 12,8 V | Sesuaikan jenis baterai |

---

## 1. Sensor Kelembapan

Rumus di firmware:

```
humidity = (HUMIDITY_DRY - RAW) / (HUMIDITY_DRY - HUMIDITY_WET) × 100   (dibatasi 0–100 %)
```

Langkah:

1. Biarkan sensor di udara / media kering. Catat nilai `RAW=` di Serial Monitor
   → isi ke `HUMIDITY_DRY`.
2. Celupkan sensor ke air / media jenuh air (hanya bagian yang boleh basah). Catat `RAW=`
   → isi ke `HUMIDITY_WET`.
3. Upload ulang. Kondisi kering harus mendekati 0 %, basah mendekati 100 %.

> Nilai `4095` adalah batas atas ADC 12-bit. Bila sensor kering memang membaca 4095, ADC
> sedang jenuh — cek tegangan catu sensor (sebaiknya 3,3 V).

## 2. Ultrasonik (Level Toren)

Rumus di firmware:

```
tinggiAir = TINGGI_TOREN - distance            (dibatasi 0 … TINGGI_MAKS)
persenAir = tinggiAir / TINGGI_MAKS × 100
```

```
   ┌── sensor ──┐  ─┬─────────────┬─
   │            │   │ JARAK_MIN   │
   │ ~~~~~~~~~~ │  ─┼─ (penuh)    │
   │            │   │             │ TINGGI_TOREN
   │   air      │   │ TINGGI_MAKS │
   │            │   │             │
   └────────────┘  ─┴─────────────┴─  dasar toren
```

Langkah:

1. Ukur jarak **sensor → dasar toren** (cm) → `TINGGI_TOREN`.
2. Ukur jarak **sensor → permukaan air saat toren penuh** (cm) → `JARAK_MIN`.
3. Hitung `TINGGI_MAKS = TINGGI_TOREN − JARAK_MIN`.
4. Bandingkan nilai `Toren RAW` di Serial Monitor dengan pengukuran meteran di beberapa level.

Catatan:

- `JARAK_MIN` saat ini **tidak dipakai langsung** dalam perhitungan; yang dipakai adalah
  `TINGGI_MAKS`. Jaga agar `TINGGI_MAKS = TINGGI_TOREN − JARAK_MIN` tetap konsisten.
- Pastikan `JARAK_MIN` lebih besar dari *blind zone* sensor (HC-SR04 ± 2–3 cm,
  JSN-SR04T waterproof ± 20–25 cm).
- Jika `Toren RAW : TIDAK TERBACA`, kelima sampel tidak mendapat echo (timeout 30 ms ≈ jarak
  > 5 m). Cek wiring ECHO/TRIG, catu daya, dan posisi sensor (harus tegak lurus ke air).

## 3. Sensor Flow (FS300A)

Rumus di firmware:

```
flowRate [L/menit] = frekuensi pulsa [Hz] / FLOW_FACTOR
```

`FLOW_FACTOR = 5.5` setara **330 pulsa per liter** (5,5 × 60). Nilai ini dari datasheet dan
masih perlu diverifikasi dengan air sungguhan.

Langkah (metode wadah ukur):

1. Alirkan air dengan debit stabil ke wadah bervolume diketahui (mis. ember 10 L).
2. Catat waktu pengisian *t* (detik) dan volume *V* (liter).
   Debit sebenarnya: `Q_ukur = V / (t / 60)` L/menit.
3. Catat rata-rata `Flow 1` / `Flow 2` di Serial Monitor selama pengisian → `Q_tampil`.
4. Hitung faktor baru:

   ```
   FLOW_FACTOR_baru = FLOW_FACTOR_lama × Q_tampil / Q_ukur
   ```

5. Ulangi pada 2–3 debit berbeda, ambil rata-ratanya.

> Debounce `FLOW_DEBOUNCE_US = 1000` µs membatasi frekuensi maksimum 1 000 Hz
> (± 180 L/menit pada faktor 5,5) — jauh di atas kapasitas FS300A, jadi aman.

## 4. Sensor RPM

```
RPM = (pulsa / RPM_PULSE_PER_REV) / detik × 60
```

- Isi `RPM_PULSE_PER_REV` dengan jumlah magnet / tanda reflektif per putaran poros.
- Pembacaan di atas `MAX_RPM` dianggap noise dan dilaporkan `0`.
- Debounce `RPM_DEBOUNCE_US = 2000` µs → maksimum 500 pulsa/detik
  (30 000 RPM pada 1 pulsa/putaran). Kurangi debounce bila memakai banyak magnet dan RPM
  tinggi.
- Verifikasi dengan tachometer digital bila tersedia.

## 5. Baterai

`batteryVoltage` diambil dari **PZEM #3** (alamat 0x03). Persentase dihitung dengan tabel:

| Tegangan ≥ (V) | % |
|---|---|
| 13,6 | 100 |
| 13,4 | 99 |
| 13,3 | 90 |
| 13,2 | 70 |
| 13,1 | 40 |
| 13,0 | 30 |
| 12,9 | 20 |
| 12,8 | 17 |
| 12,5 | 14 |
| 12,0 | 9 |
| < 12,0 | 0 |

Tabel ini sesuai kurva tegangan **istirahat** baterai **LiFePO4 12,8 V (4S)**.

- Untuk aki **lead-acid / VRLA 12 V**, ganti tabel di `calculateBatteryPercent()` dengan
  kurva lead-acid (rentang kira-kira 11,8–12,7 V).
- Tegangan saat dicas atau berbeban berat tidak mencerminkan kapasitas sebenarnya;
  persentase paling akurat saat baterai istirahat.

## 6. PZEM-017

- **Alamat**: setiap unit harus diset unik (0x01–0x04) dengan `setAddress()` — lakukan
  satu per satu dengan hanya satu PZEM di bus.
- **Tipe shunt**: firmware tidak mengatur shunt. Set sekali per unit dengan
  `setShuntType()` sesuai shunt yang dipasang: `0x0000` = 100 A, `0x0001` = 50 A,
  `0x0002` = 200 A, `0x0003` = 300 A.
- **Reset energi**: gunakan `resetEnergy()` bila ingin mengulang akumulasi kWh dari nol.

Contoh sketch sekali-jalan untuk mengatur alamat & shunt (upload, jalankan, lalu kembalikan
firmware utama):

```cpp
#include <PZEM017v1.h>

HardwareSerial SerialPZEM(2);
PZEM017v1 pzem(&SerialPZEM, 0xF8);   // 0xF8 = alamat broadcast/umum

void setup() {
  Serial.begin(115200);
  SerialPZEM.begin(9600, SERIAL_8N1, 16, 17);
  delay(500);

  Serial.println(pzem.setAddress(0x03)   ? "Alamat OK" : "Alamat GAGAL");
  Serial.println(pzem.setShuntType(0x0000) ? "Shunt OK" : "Shunt GAGAL");
}

void loop() {}
```

> Hanya **satu** PZEM yang boleh tersambung saat menjalankan sketch ini.
