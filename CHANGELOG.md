# Changelog

Semua perubahan penting pada proyek ini dicatat di file ini.
Format mengikuti [Keep a Changelog](https://keepachangelog.com/id-ID/1.1.0/),
dan versi mengikuti [Semantic Versioning](https://semver.org/lang/id/).

## [1.0.0] - 2026-11-18

### Ditambahkan
- Firmware ESP32 `SmartWater_Full_FlowLevel.ino`:
  - Level toren dengan ultrasonik (median 5 sampel), kirim jarak mentah + persentase.
  - 2 sensor flow FS300A dan sensor RPM generator berbasis interrupt + debounce.
  - Sensor kelembapan analog (GPIO34).
  - 4× PZEM-017 pada satu bus RS485; PZEM #3 sebagai tegangan baterai.
  - Kontrol valve 3-way & 2-way: pulse 15 detik non-blocking, edge trigger, interlock biru/coklat.
  - Relay umum K5–K8.
  - WiFiManager (captive portal) + auto-reconnect, sinkronisasi NTP (WIB).
  - Kirim telemetri ke `/api/data` dan ambil perintah dari `/api/control`.
- Dokumentasi: README, wiring, kontrak API, panduan kalibrasi.
- Workflow GitHub Actions untuk uji kompilasi.

### Diubah
- URL backend dan password Access Point dipindahkan dari sketch ke `secrets.h`
  (di-ignore Git); template publik di `secrets.example.h`.
