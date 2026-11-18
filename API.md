# Kontrak API ESP32 ↔ Backend

ESP32 berkomunikasi dengan backend (Cloudflare Worker) lewat dua endpoint HTTPS. URL-nya
diatur di `secrets.h`:

| Konstanta | Metode | Interval | Timeout (connect / respons) |
|---|---|---|---|
| `SECRET_API_DATA_URL` → `/api/data` | `POST` | 3 detik | 2 s / 4 s |
| `SECRET_API_CONTROL_URL` → `/api/control` | `GET` | 0,5 detik | 2 s / 3 s |

Saat ini **tidak ada autentikasi** (tanpa API key / token). Lihat bagian Keamanan di
[README](../README.md#keamanan).

---

## `POST /api/data` — ESP32 mengirim telemetri

Header: `Content-Type: application/json`

### Contoh body

```json
{
  "kelembapan": 42.5,
  "levelToren": 73.0,
  "torenDistance": 48.3,
  "flowRate1": 3.27,
  "flowRate2": 0.00,
  "rpmGenerator": 412,
  "batteryVoltage": 13.25,
  "batteryPercent": 70,
  "wifiSSID": "NamaWiFi",
  "wifiRSSI": -58,
  "pzem": [
    { "voltage": 18.20, "current": 1.05, "power": 19.10, "energy": 0.532, "status": "OK" },
    { "voltage": 0.00,  "current": 0.00, "power": 0.00,  "energy": 0.000, "status": "ERROR" },
    { "voltage": 13.25, "current": 0.80, "power": 10.60, "energy": 1.204, "status": "OK" },
    { "voltage": 12.10, "current": 0.40, "power": 4.80,  "energy": 0.311, "status": "OK" }
  ],
  "relay": {
    "relay1": true, "relay2": false, "relay3": false, "relay4": false,
    "relay5": false, "relay6": false, "relay7": false, "relay8": false
  },
  "mode": "MANUAL",
  "valve3Position": "OPEN",
  "valve2Position": "CLOSED",
  "espOnline": true
}
```

> ESP32 mengirim JSON *compact* (tanpa spasi/baris baru). Contoh di atas diformat agar mudah dibaca.

### Penjelasan field

| Field | Tipe | Satuan | Keterangan |
|---|---|---|---|
| `kelembapan` | number (1 desimal) | % | 0–100, dari ADC GPIO34 |
| `levelToren` | number (1 desimal) | % | Persentase level air. **`-1`** bila ultrasonik tidak mendapat echo. Field lama, dipertahankan untuk kompatibilitas |
| `torenDistance` | number (1 desimal) | cm | Jarak **mentah** sensor → permukaan air. **`-1`** bila tidak ada echo |
| `flowRate1`, `flowRate2` | number (2 desimal) | L/menit | |
| `rpmGenerator` | number (bulat) | RPM | `0` bila tidak ada pulsa atau terdeteksi noise (> `MAX_RPM`) |
| `batteryVoltage` | number (2 desimal) | V | Diambil dari **PZEM #3** (`pzem[2].voltage`) |
| `batteryPercent` | number (bulat) | % | Hasil tabel `calculateBatteryPercent()` |
| `wifiSSID` | string | — | `"tidak terhubung"` bila kosong; tanda `"` dan `\` di-escape |
| `wifiRSSI` | integer | dBm | |
| `pzem` | array[4] | — | Urutan = alamat 0x01, 0x02, 0x03, 0x04 |
| `pzem[].voltage` | number | V | |
| `pzem[].current` | number | A | |
| `pzem[].power` | number | W | |
| `pzem[].energy` | number (3 desimal) | kWh | Akumulasi energi di PZEM |
| `pzem[].status` | `"OK"` \| `"ERROR"` | — | Bila `ERROR`, **abaikan** nilai lain pada objek tersebut (bisa 0 atau nilai lama) |
| `relay.relay1`–`relay8` | boolean | — | Status **fisik** pin relay. K1–K4 hanya `true` selama pulse 15 detik |
| `mode` | `"AUTO"` \| `"MANUAL"` | — | Mode terakhir yang diterima dari `/api/control` |
| `valve3Position` | `"OPEN"` \| `"CLOSED"` | — | Flag posisi valve 3-way |
| `valve2Position` | `"OPEN"` \| `"CLOSED"` | — | Flag posisi valve 2-way |
| `espOnline` | boolean | — | Selalu `true` (penanda heartbeat) |

Respons: firmware menganggap kode **2xx** sebagai sukses. Kode lain dicetak ke Serial
beserta body respons; kode ≤ 0 dicetak sebagai pesan error `HTTPClient`.

---

## `GET /api/control` — ESP32 mengambil perintah

Firmware hanya memproses respons dengan **HTTP 200**.

### Contoh respons

```json
{"mode":"MANUAL","relay1":true,"relay2":false,"relay3":false,"relay4":false,"relay5":false,"relay6":true,"relay7":false,"relay8":false,"valve3Position":"OPEN","valve2Position":"CLOSED"}
```

Field `relayN` boleh berada di level atas atau di dalam objek lain (misalnya
`{"relay":{"relay1":true,...}}`), karena parser mencari potongan teks, bukan struktur JSON.

### Field yang dibaca

| Field | Nilai | Efek |
|---|---|---|
| `mode` | `"AUTO"` / `"MANUAL"` | Disimpan & dilaporkan kembali. Tidak mengubah perilaku relay |
| `relay1`–`relay4` | `true` / `false` | **Edge trigger**: transisi `false → true` memulai pulse 15 detik pada K1–K4 |
| `relay5`–`relay8` | `true` / `false` | **Level**: relay K5–K8 langsung ON/OFF mengikuti nilai |
| `valve3Position` | `"OPEN"` / `"CLOSED"` | Menimpa flag posisi valve 3-way |
| `valve2Position` | `"OPEN"` / `"CLOSED"` | Menimpa flag posisi valve 2-way |

Field yang **tidak ada** di respons tidak mengubah nilai sebelumnya.

### ⚠️ Aturan format (penting untuk backend)

Parser di firmware (`parseControlResponse()`) memakai `String.indexOf()` dengan pola teks
yang persis. Karena itu respons harus:

1. **Compact — tanpa spasi** di sekitar `:`.
   - ✅ `"relay1":true`
   - ❌ `"relay1": true` → tidak terbaca, nilai lama dipertahankan
2. Memakai boolean JSON asli, bukan string atau angka.
   - ✅ `true` / `false`
   - ❌ `"true"`, `1`, `0`
3. Huruf kapital persis: `"AUTO"`, `"MANUAL"`, `"OPEN"`, `"CLOSED"`.

Di Cloudflare Worker, `JSON.stringify(obj)` (tanpa argumen indentasi) sudah menghasilkan
format compact yang benar:

```js
return new Response(JSON.stringify(control), {
  headers: { "Content-Type": "application/json" },
});
```

### Cara backend memicu valve

Karena K1–K4 memakai deteksi tepi naik, satu perintah buka/tutup adalah urutan:

```
relay1:false  →  relay1:true  →  (tahan sampai ESP32 sempat polling)  →  relay1:false
```

- Nilai `true` harus bertahan minimal satu siklus polling yang berhasil (≥ 0,5 detik, lebih
  lama bila jaringan lambat). Menahan `true` lebih lama tidak mengulang pulse.
- Setelah itu kembalikan ke `false` agar perintah berikutnya terdeteksi sebagai tepi baru.
- Jika `relayX` masih `true` saat ESP32 restart, pulse akan langsung dijalankan sekali
  setelah boot.
- Interlock (biru vs coklat) ditangani firmware; backend tidak perlu mematikan pasangan
  relay secara manual.
