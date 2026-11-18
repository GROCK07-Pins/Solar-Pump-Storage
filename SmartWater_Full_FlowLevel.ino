#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <time.h>
#include <PZEM017v1.h>

// Konfigurasi rahasia (URL API, password AP).
// Salin secrets.example.h -> secrets.h lalu isi nilainya.
// secrets.h TIDAK ikut di-commit (lihat .gitignore).
#include "secrets.h"

// =====================================================
// SMART WATER & ENERGY
// ESP32 FINAL
//
// VALVE:
// K1 = 3-WAY BLUE  = OPEN
// K2 = 3-WAY BROWN = CLOSE
// K3 = 2-WAY BLUE  = OPEN
// K4 = 2-WAY BROWN = CLOSE
//
// K1 + K3 BOLEH ON BERSAMA
//
// TIMER VALVE = 15 DETIK
// TANPA delay()
// MENGGUNAKAN millis()
//
// FLAG POSISI:
// 3-WAY = OPEN / CLOSED
// 2-WAY = OPEN / CLOSED
// =====================================================


// =====================================================
// API
// =====================================================

// Nilai diambil dari secrets.h
const char* API_DATA =
SECRET_API_DATA_URL;

const char* API_CONTROL =
SECRET_API_CONTROL_URL;


// =====================================================
// WIFI MANAGER
// =====================================================

#define WIFI_CONFIG_BUTTON 0

const char* WIFI_AP_NAME =
"SMART-WATER-SETUP";

// Minimal 8 karakter (syarat WPA2). Diambil dari secrets.h
const char* WIFI_AP_PASSWORD =
SECRET_WIFI_AP_PASSWORD;

WiFiManager wifiManager;


// =====================================================
// WIFI RECONNECT
// =====================================================

unsigned long lastWiFiCheck = 0;

const unsigned long WIFI_CHECK_INTERVAL =
5000;


// =====================================================
// NTP
// =====================================================

const long GMT_OFFSET_SEC =
7 * 3600;

const int DAYLIGHT_OFFSET_SEC =
0;

bool timeValid = false;

unsigned long lastNTPRetry = 0;

const unsigned long NTP_RETRY_INTERVAL =
60000;


// =====================================================
// KELEMBAPAN
// =====================================================

#define HUMIDITY_PIN 34

const int HUMIDITY_DRY =
4095;

const int HUMIDITY_WET =
1900;

float humidity =
0;

int humidityRaw =
4095;


// =====================================================
// ULTRASONIC
// =====================================================

#define TRIG_PIN 25
#define ECHO_PIN 26

const float TINGGI_TOREN =
125.0;

const float JARAK_MIN =
20.0;

const float TINGGI_MAKS =
105.0;

float distance =
-1; // -1 means no valid echo in the latest sample batch.

float tinggiAir =
0;

float persenAir =
0;


// =====================================================
// FLOW
// =====================================================

#define FLOW1_PIN 27
#define FLOW2_PIN 33

volatile unsigned long flow1PulseCount =
0;

volatile unsigned long flow2PulseCount =
0;


// -----------------------------------------------------
// FLOW SENSOR
//
// Sesuaikan dengan sensor yang digunakan.
//
// FS300A uses provisional 5.5 Hz per L/min (330 pulses/liter).
// Tetapi untuk pembacaan lebih stabil kita juga
// melakukan debounce dan minimum pulse.
// -----------------------------------------------------

const float FLOW_FACTOR =
5.5; // FS300A: 330 pulses/liter; provisional, verify with measured water volume.


// Minimum waktu antar pulsa.
// Mencegah noise sangat cepat dianggap pulsa.
// 1000 us = 1 ms
const unsigned long FLOW_DEBOUNCE_US =
1000;


float flowRate1 =
0;

float flowRate2 =
0;


// =====================================================
// RPM
// =====================================================

#define RPM_PIN 32

volatile unsigned long rpmPulseCount =
0;

const float RPM_PULSE_PER_REV =
1.0;


// Debounce RPM
const unsigned long RPM_DEBOUNCE_US =
2000;


// Batas maksimum RPM.
// Kalau pembacaan melebihi ini dianggap noise.
const float MAX_RPM =
10000.0;

float rpmGenerator =
0;


// =====================================================
// ISR
// =====================================================

volatile unsigned long lastFlow1Interrupt =
0;

volatile unsigned long lastFlow2Interrupt =
0;

volatile unsigned long lastRPMInterrupt =
0;


// =====================================================
// PZEM
// =====================================================

#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17

HardwareSerial SerialPZEM(2);

PZEM017v1 pzem1(
  &SerialPZEM,
  0x01
);

PZEM017v1 pzem2(
  &SerialPZEM,
  0x02
);

PZEM017v1 pzem3(
  &SerialPZEM,
  0x03
);

PZEM017v1 pzem4(
  &SerialPZEM,
  0x04
);


float pzemVoltage[4] = {
  0,
  0,
  0,
  0
};

float pzemCurrent[4] = {
  0,
  0,
  0,
  0
};

float pzemPower[4] = {
  0,
  0,
  0,
  0
};

float pzemEnergy[4] = {
  0,
  0,
  0,
  0
};


bool pzemOK[4] = {
  false,
  false,
  false,
  false
};


// =====================================================
// RELAY
// =====================================================

#define RELAY1_PIN 23
#define RELAY2_PIN 22

#define RELAY3_PIN 21
#define RELAY4_PIN 19

#define RELAY5_PIN 18
#define RELAY6_PIN 5
#define RELAY7_PIN 4
#define RELAY8_PIN 13


// Relay active LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH


// =====================================================
// VALVE TIMER
// =====================================================

const unsigned long VALVE_PULSE_TIME =
15000;


// =====================================================
// RELAY STATE
//
// relayPhysical = kondisi pin sebenarnya.
// =====================================================

bool relayPhysical[8] = {
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false
};


// =====================================================
// TIMER MASING-MASING RELAY
// =====================================================

unsigned long relayStartTime[8] = {
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0
};


bool relayTimerActive[8] = {
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false
};


// =====================================================
// FLAG POSISI VALVE
//
// INI BUKAN status relay.
//
// Relay hanya ON selama 15 detik.
// Flag tetap menyimpan posisi valve.
//
// =====================================================

String valve3Position =
"CLOSED";

String valve2Position =
"CLOSED";


// =====================================================
// COMMAND SERVER
// =====================================================

bool serverRelay[8] = {
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false
};


String currentMode =
"MANUAL";


// =====================================================
// EDGE COMMAND
//
// Digunakan agar relay tidak terus restart 15 detik
// hanya karena server masih mengirim true.
//
// Ketika command berubah:
// false -> true = jalankan pulse.
//
// Setelah pulse selesai, relay OFF.
// =====================================================

bool previousServerRelay[8] = {
  false,
  false,
  false,
  false,
  false,
  false,
  false,
  false
};


// =====================================================
// TIMER LOOP
// =====================================================

unsigned long lastSensorRead =
0;

unsigned long lastDataSend =
0;

unsigned long lastControlRead =
0;

unsigned long lastPulseSample =
0;


// =====================================================
// INTERVAL
// =====================================================

const unsigned long SENSOR_INTERVAL =
500;

const unsigned long DATA_INTERVAL =
3000;

const unsigned long CONTROL_INTERVAL =
500;


// =====================================================
// ISR FLOW 1
// =====================================================

void IRAM_ATTR countFlow1Pulse()
{
  unsigned long now =
    micros();

  if(
    now -
    lastFlow1Interrupt >=
    FLOW_DEBOUNCE_US
  )
  {
    flow1PulseCount++;

    lastFlow1Interrupt =
      now;
  }
}


// =====================================================
// ISR FLOW 2
// =====================================================

void IRAM_ATTR countFlow2Pulse()
{
  unsigned long now =
    micros();

  if(
    now -
    lastFlow2Interrupt >=
    FLOW_DEBOUNCE_US
  )
  {
    flow2PulseCount++;

    lastFlow2Interrupt =
      now;
  }
}


// =====================================================
// ISR RPM
// =====================================================

void IRAM_ATTR countRPMPulse()
{
  unsigned long now =
    micros();

  if(
    now -
    lastRPMInterrupt >=
    RPM_DEBOUNCE_US
  )
  {
    rpmPulseCount++;

    lastRPMInterrupt =
      now;
  }
}


// =====================================================
// HUMIDITY
// =====================================================

void readHumidity()
{
  long total =
    0;

  const int samples =
    5;


  for(
    int i = 0;
    i < samples;
    i++
  )
  {
    total +=
      analogRead(
        HUMIDITY_PIN
      );
  }


  humidityRaw =
    total / samples;


  humidity =
    (
      (
        float
      )(
        HUMIDITY_DRY -
        humidityRaw
      )
      /
      (
        HUMIDITY_DRY -
        HUMIDITY_WET
      )
    )
    * 100.0;


  humidity =
    constrain(
      humidity,
      0,
      100
    );
}


// =====================================================
// ULTRASONIC
// =====================================================

// Explicit declaration for calls before the timer function definition.
void updateRelayTimers();

void readUltrasonic()
{
  // Same median method as the standalone sketch tested by the user.
  // Keep FULL-system wiring: TRIG 25, ECHO 26.
  float samples[5];
  int valid = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(15);
    digitalWrite(TRIG_PIN, LOW);
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
    if (duration > 0) {
      samples[valid++] = duration * 0.0343f / 2.0f;
    }
    // Preserve the tested 30 ms gap, servicing relay timers during the wait.
    unsigned long gapStart = millis();
    do {
      updateRelayTimers();
      delay(1);
    } while (millis() - gapStart < 30);
  }
  if (valid == 0) {
    distance = -1;
    tinggiAir = -1;
    persenAir = -1;
    return;
  }
  for (int i = 0; i < valid - 1; i++) {
    for (int j = i + 1; j < valid; j++) {
      if (samples[j] < samples[i]) {
        float temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }
  distance = samples[valid / 2];
  // Existing geometry retained pending measurement: empty 125, full 20 cm.
  // Dashboard prioritizes measured distance and labels percentage provisional.
  tinggiAir = constrain(TINGGI_TOREN - distance, 0.0f, TINGGI_MAKS);
  persenAir = constrain(tinggiAir / TINGGI_MAKS * 100.0f, 0.0f, 100.0f);
}


// =====================================================
// FLOW + RPM
// =====================================================

void readPulseSensors()
{
  unsigned long now =
    millis();


  unsigned long elapsed =
    now -
    lastPulseSample;


  if(
    elapsed <
    1000
  )
  {
    return;
  }


  noInterrupts();

  unsigned long f1 =
    flow1PulseCount;

  unsigned long f2 =
    flow2PulseCount;

  unsigned long rpmPulse =
    rpmPulseCount;


  flow1PulseCount =
    0;

  flow2PulseCount =
    0;

  rpmPulseCount =
    0;

  interrupts();


  float seconds =
    elapsed / 1000.0;


  // ===================================================
  // FLOW
  // ===================================================

  if(
    f1 == 0
  )
  {
    flowRate1 =
      0;
  }

  else
  {
    flowRate1 =
      (
        f1 /
        seconds
      )
      /
      FLOW_FACTOR;
  }


  if(
    f2 == 0
  )
  {
    flowRate2 =
      0;
  }

  else
  {
    flowRate2 =
      (
        f2 /
        seconds
      )
      /
      FLOW_FACTOR;
  }


  // ===================================================
  // RPM
  // ===================================================

  if(
    rpmPulse == 0
  )
  {
    rpmGenerator =
      0;
  }

  else
  {
    float revolutions =
      rpmPulse /
      RPM_PULSE_PER_REV;


    rpmGenerator =
      (
        revolutions /
        seconds
      )
      * 60.0;


    // -------------------------------------------------
    // Anti-noise
    // -------------------------------------------------

    if(
      rpmGenerator >
      MAX_RPM
    )
    {
      rpmGenerator =
        0;
    }
  }


  lastPulseSample =
    now;
}


// =====================================================
// PZEM
// =====================================================

void readOnePZEM(
  int index,
  PZEM017v1 &pzem
)
{
  float v =
    pzem.voltage();

  float c =
    pzem.current();

  float p =
    pzem.power();

  float e =
    pzem.energy();


  if(
    isnan(v) ||
    isnan(c) ||
    isnan(p) ||
    isnan(e)
  )
  {
    pzemOK[index] =
      false;

    pzemVoltage[index] =
      0;

    pzemCurrent[index] =
      0;

    pzemPower[index] =
      0;

    pzemEnergy[index] =
      0;

    return;
  }


  // ---------------------------------------------------
  // Basic validation
  // ---------------------------------------------------

  if(
    v < 0 ||
    c < 0 ||
    p < 0 ||
    e < 0
  )
  {
    pzemOK[index] =
      false;

    return;
  }


  pzemOK[index] =
    true;


  pzemVoltage[index] =
    v;

  pzemCurrent[index] =
    c;

  pzemPower[index] =
    p;

  pzemEnergy[index] =
    e;
}


// =====================================================
// READ SEMUA PZEM
// =====================================================

void readPZEM()
{
  readOnePZEM(
    0,
    pzem1
  );

  delay(50);


  readOnePZEM(
    1,
    pzem2
  );

  delay(50);


  readOnePZEM(
    2,
    pzem3
  );

  delay(50);


  readOnePZEM(
    3,
    pzem4
  );


  Serial.println();
  Serial.println(
    "========== PZEM =========="
  );


  for(
    int i = 0;
    i < 4;
    i++
  )
  {
    Serial.print(
      "PZEM "
    );

    Serial.print(
      i + 1
    );

    Serial.print(
      " : "
    );


    if(
      pzemOK[i]
    )
    {
      Serial.print(
        "V="
      );

      Serial.print(
        pzemVoltage[i],
        2
      );

      Serial.print(
        " I="
      );

      Serial.print(
        pzemCurrent[i],
        2
      );

      Serial.print(
        " P="
      );

      Serial.print(
        pzemPower[i],
        2
      );

      Serial.print(
        " E="
      );

      Serial.println(
        pzemEnergy[i],
        3
      );
    }

    else
    {
      Serial.println(
        "ERROR"
      );
    }
  }


  Serial.println(
    "==========================="
  );
}


// =====================================================
// RELAY PIN
// =====================================================

int getRelayPin(
  int index
)
{
  switch(index)
  {
    case 0:
      return RELAY1_PIN;

    case 1:
      return RELAY2_PIN;

    case 2:
      return RELAY3_PIN;

    case 3:
      return RELAY4_PIN;

    case 4:
      return RELAY5_PIN;

    case 5:
      return RELAY6_PIN;

    case 6:
      return RELAY7_PIN;

    case 7:
      return RELAY8_PIN;
  }


  return -1;
}


// =====================================================
// RELAY OFF
// =====================================================

void turnRelayOff(
  int index
)
{
  int pin =
    getRelayPin(index);


  if(
    pin < 0
  )
  {
    return;
  }


  digitalWrite(
    pin,
    RELAY_OFF
  );


  relayPhysical[index] =
    false;

  relayTimerActive[index] =
    false;
}


// =====================================================
// START RELAY PULSE
// =====================================================

bool startRelayPulse(
  int index
)
{
  int pin =
    getRelayPin(index);


  if(
    pin < 0
  )
  {
    return false;
  }


  // ===================================================
  // K1 / K2
  // 3-WAY
  // ===================================================

  if(
    index == 0
  )
  {
    // K2 harus OFF
    turnRelayOff(1);
  }


  if(
    index == 1
  )
  {
    // K1 harus OFF
    turnRelayOff(0);
  }


  // ===================================================
  // K3 / K4
  // 2-WAY
  // ===================================================

  if(
    index == 2
  )
  {
    // K4 harus OFF
    turnRelayOff(3);
  }


  if(
    index == 3
  )
  {
    // K3 harus OFF
    turnRelayOff(2);
  }


  // ===================================================
  // BLUE / BROWN SAFETY
  //
  // K1 + K3 = BLUE + BLUE = BOLEH
  //
  // K2/K4 = BROWN
  //
  // Brown tidak boleh aktif bersamaan dengan blue.
  // ===================================================

  bool requestedBlue =
    (
      index == 0 ||
      index == 2
    );


  bool requestedBrown =
    (
      index == 1 ||
      index == 3
    );


  if(
    requestedBlue
  )
  {
    // Matikan semua brown
    turnRelayOff(1);
    turnRelayOff(3);
  }


  if(
    requestedBrown
  )
  {
    // Matikan semua blue
    turnRelayOff(0);
    turnRelayOff(2);
  }


  // ===================================================
  // START
  // ===================================================

  digitalWrite(
    pin,
    RELAY_ON
  );


  relayPhysical[index] =
    true;


  relayTimerActive[index] =
    true;


  relayStartTime[index] =
    millis();


  Serial.print(
    "RELAY K"
  );

  Serial.print(
    index + 1
  );

  Serial.println(
    " ON - TIMER 15 DETIK"
  );


  return true;
}


// =====================================================
// UPDATE RELAY TIMER
// =====================================================
//
// TANPA delay()
// =====================================================

void updateRelayTimers()
{
  unsigned long now =
    millis();


  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    if(
      !relayTimerActive[i]
    )
    {
      continue;
    }


    if(
      now -
      relayStartTime[i]
      >=
      VALVE_PULSE_TIME
    )
    {
      turnRelayOff(i);


      Serial.print(
        "RELAY K"
      );

      Serial.print(
        i + 1
      );

      Serial.println(
        " OFF - 15 DETIK SELESAI"
      );
    }
  }
}


// =====================================================
// UPDATE FLAG VALVE
// =====================================================

void updateValveFlagsFromCommand()
{
  // ---------------------------------------------------
  // 3-WAY
  // ---------------------------------------------------

  if(
    serverRelay[0] &&
    !previousServerRelay[0]
  )
  {
    valve3Position =
      "OPEN";

    Serial.println(
      "FLAG 3-WAY = OPEN"
    );
  }


  if(
    serverRelay[1] &&
    !previousServerRelay[1]
  )
  {
    valve3Position =
      "CLOSED";

    Serial.println(
      "FLAG 3-WAY = CLOSED"
    );
  }


  // ---------------------------------------------------
  // 2-WAY
  // ---------------------------------------------------

  if(
    serverRelay[2] &&
    !previousServerRelay[2]
  )
  {
    valve2Position =
      "OPEN";

    Serial.println(
      "FLAG 2-WAY = OPEN"
    );
  }


  if(
    serverRelay[3] &&
    !previousServerRelay[3]
  )
  {
    valve2Position =
      "CLOSED";

    Serial.println(
      "FLAG 2-WAY = CLOSED"
    );
  }
}


// =====================================================
// APPLY SERVER COMMAND
// =====================================================

void applyServerCommands()
{
  // ===================================================
  // K1
  // ===================================================

  if(
    serverRelay[0] &&
    !previousServerRelay[0]
  )
  {
    startRelayPulse(0);
  }


  // ===================================================
  // K2
  // ===================================================

  if(
    serverRelay[1] &&
    !previousServerRelay[1]
  )
  {
    startRelayPulse(1);
  }


  // ===================================================
  // K3
  // ===================================================

  if(
    serverRelay[2] &&
    !previousServerRelay[2]
  )
  {
    startRelayPulse(2);
  }


  // ===================================================
  // K4
  // ===================================================

  if(
    serverRelay[3] &&
    !previousServerRelay[3]
  )
  {
    startRelayPulse(3);
  }


  // ===================================================
  // K5-K8
  //
  // Manual biasa.
  // ===================================================

  for(
    int i = 4;
    i < 8;
    i++
  )
  {
    if(
      serverRelay[i]
      !=
      previousServerRelay[i]
    )
    {
      int pin =
        getRelayPin(i);


      digitalWrite(
        pin,
        serverRelay[i]
          ? RELAY_ON
          : RELAY_OFF
      );


      relayPhysical[i] =
        serverRelay[i];
    }
  }


  // ===================================================
  // SIMPAN COMMAND LAMA
  // ===================================================

  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    previousServerRelay[i] =
      serverRelay[i];
  }
}


// =====================================================
// PARSE CONTROL RESPONSE
// =====================================================

void parseControlResponse(
  String response
)
{
  // ===================================================
  // MODE
  // ===================================================

  if(
    response.indexOf(
      "\"mode\":\"AUTO\""
    ) >= 0
  )
  {
    currentMode =
      "AUTO";
  }


  if(
    response.indexOf(
      "\"mode\":\"MANUAL\""
    ) >= 0
  )
  {
    currentMode =
      "MANUAL";
  }


  // ===================================================
  // RELAY
  // ===================================================

  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    String trueKey =
      "\"relay" +
      String(i + 1) +
      "\":true";


    String falseKey =
      "\"relay" +
      String(i + 1) +
      "\":false";


    if(
      response.indexOf(
        trueKey
      ) >= 0
    )
    {
      serverRelay[i] =
        true;
    }

    else if(
      response.indexOf(
        falseKey
      ) >= 0
    )
    {
      serverRelay[i] =
        false;
    }
  }


  // ===================================================
  // FLAG DARI WORKER
  // ===================================================

  if(
    response.indexOf(
      "\"valve3Position\":\"OPEN\""
    ) >= 0
  )
  {
    valve3Position =
      "OPEN";
  }

  else if(
    response.indexOf(
      "\"valve3Position\":\"CLOSED\""
    ) >= 0
  )
  {
    valve3Position =
      "CLOSED";
  }


  if(
    response.indexOf(
      "\"valve2Position\":\"OPEN\""
    ) >= 0
  )
  {
    valve2Position =
      "OPEN";
  }

  else if(
    response.indexOf(
      "\"valve2Position\":\"CLOSED\""
    ) >= 0
  )
  {
    valve2Position =
      "CLOSED";
  }


  // ===================================================
  // UPDATE FLAG SEBELUM COMMAND
  // ===================================================

  updateValveFlagsFromCommand();


  // ===================================================
  // APPLY
  // ===================================================

  applyServerCommands();
}


// =====================================================
// GET CONTROL
// =====================================================

void getControlFromAPI()
{
  if(
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    return;
  }


  HTTPClient http;


  http.begin(
    API_CONTROL
  );


  http.setConnectTimeout(
    2000
  );

  http.setTimeout(
    3000
  );


  int code =
    http.GET();


  if(
    code == 200
  )
  {
    String response =
      http.getString();


    parseControlResponse(
      response
    );


    Serial.println();
    Serial.println(
      "========== CONTROL =========="
    );


    Serial.print(
      "Mode: "
    );

    Serial.println(
      currentMode
    );


    Serial.print(
      "3-WAY FLAG: "
    );

    Serial.println(
      valve3Position
    );


    Serial.print(
      "2-WAY FLAG: "
    );

    Serial.println(
      valve2Position
    );


    Serial.print(
      "K1="
    );

    Serial.print(
      serverRelay[0]
        ? "ON"
        : "OFF"
    );


    Serial.print(
      " K2="
    );

    Serial.print(
      serverRelay[1]
        ? "ON"
        : "OFF"
    );


    Serial.print(
      " K3="
    );

    Serial.print(
      serverRelay[2]
        ? "ON"
        : "OFF"
    );


    Serial.print(
      " K4="
    );

    Serial.print(
      serverRelay[3]
        ? "ON"
        : "OFF"
    );


    Serial.println();


    Serial.println(
      "=============================="
    );
  }

  else
  {
    Serial.print(
      "CONTROL ERROR: "
    );

    Serial.println(
      code
    );
  }


  http.end();
}


// =====================================================
// BATTERY
// =====================================================

float calculateBatteryPercent(
  float voltage
)
{
  if(
    voltage >= 14.2
  )
    return 100;

  if(
    voltage >= 13.6
  )
    return 100;

  if(
    voltage >= 13.4
  )
    return 99;

  if(
    voltage >= 13.3
  )
    return 90;

  if(
    voltage >= 13.2
  )
    return 70;

  if(
    voltage >= 13.1
  )
    return 40;

  if(
    voltage >= 13.0
  )
    return 30;

  if(
    voltage >= 12.9
  )
    return 20;

  if(
    voltage >= 12.8
  )
    return 17;

  if(
    voltage >= 12.5
  )
    return 14;

  if(
    voltage >= 12.0
  )
    return 9;


  return 0;
}


// =====================================================
// SEND DATA
// =====================================================

void sendToAPI()
{
  if(
    WiFi.status() !=
    WL_CONNECTED
  )
  {
    return;
  }


  HTTPClient http;


  http.begin(
    API_DATA
  );


  http.setConnectTimeout(
    2000
  );

  http.setTimeout(
    4000
  );


  http.addHeader(
    "Content-Type",
    "application/json"
  );


  // ===================================================
  // WIFI
  // ===================================================

  String wifiSSID =
    WiFi.SSID();


  int wifiRSSI =
    WiFi.RSSI();


  if(
    wifiSSID.length() == 0
  )
  {
    wifiSSID =
      "tidak terhubung";
  }


  // ===================================================
  // BATTERY
  // ===================================================

  float batteryVoltage =
    pzemVoltage[2];


  float battery =
    calculateBatteryPercent(
      batteryVoltage
    );


  // ===================================================
  // JSON
  // ===================================================

  String json =
    "{";


  json +=
    "\"kelembapan\":" +
    String(
      humidity,
      1
    );


  // Legacy percentage retained for API compatibility.
  json +=
    ",\"levelToren\":" +
    String(
      persenAir,
      1
    );

  // RAW ultrasonic distance: sensor -> water surface (cm).
  json +=
    ",\"torenDistance\":" +
    String(
      distance,
      1
    );


  json +=
    ",\"flowRate1\":" +
    String(
      flowRate1,
      2
    );


  json +=
    ",\"flowRate2\":" +
    String(
      flowRate2,
      2
    );


  json +=
    ",\"rpmGenerator\":" +
    String(
      rpmGenerator,
      0
    );


  json +=
    ",\"batteryVoltage\":" +
    String(
      batteryVoltage,
      2
    );


  json +=
    ",\"batteryPercent\":" +
    String(
      battery,
      0
    );


  // ===================================================
  // WIFI
  // ===================================================

  String safeSSID = wifiSSID;
  safeSSID.replace("\\", "\\\\");
  safeSSID.replace("\"", "\\\"");

  json +=
    ",\"wifiSSID\":\"" +
    safeSSID +
    "\"";


  json +=
    ",\"wifiRSSI\":" +
    String(
      wifiRSSI
    );


  // ===================================================
  // PZEM
  // ===================================================

  json +=
    ",\"pzem\":[";


  for(
    int i = 0;
    i < 4;
    i++
  )
  {
    if(
      i > 0
    )
    {
      json += ",";
    }


    json +=
      "{";


    json +=
      "\"voltage\":" +
      String(
        pzemVoltage[i],
        2
      );


    json +=
      ",\"current\":" +
      String(
        pzemCurrent[i],
        2
      );


    json +=
      ",\"power\":" +
      String(
        pzemPower[i],
        2
      );


    json +=
      ",\"energy\":" +
      String(
        pzemEnergy[i],
        3
      );


    json +=
      ",\"status\":\"" +
      String(
        pzemOK[i]
          ? "OK"
          : "ERROR"
      ) +
      "\"";


    json +=
      "}";
  }


  json +=
    "]";


  // ===================================================
  // RELAY PHYSICAL
  // ===================================================

  json +=
    ",\"relay\":{";


  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    if(
      i > 0
    )
    {
      json += ",";
    }


    json +=
      "\"relay" +
      String(i + 1) +
      "\":" +
      String(
        relayPhysical[i]
          ? "true"
          : "false"
      );
  }


  json +=
    "}";


  // ===================================================
  // MODE
  // ===================================================

  json +=
    ",\"mode\":\"" +
    currentMode +
    "\"";


  // ===================================================
  // VALVE FLAG
  // ===================================================

  json +=
    ",\"valve3Position\":\"" +
    valve3Position +
    "\"";


  json +=
    ",\"valve2Position\":\"" +
    valve2Position +
    "\"";


  // ===================================================
  // ESP ONLINE
  // ===================================================

  json +=
    ",\"espOnline\":true";


  json +=
    "}";


  // ===================================================
  // POST
  // ===================================================

  int code =
    http.POST(
      json
    );


  Serial.print(
    "HTTP DATA: "
  );

  Serial.println(
    code
  );


  if (code <= 0) {
    Serial.println(HTTPClient::errorToString(code));
  } else if (code < 200 || code >= 300) {
    Serial.println(http.getString());
  }

  http.end();
}


// =====================================================
// WIFI RECONNECT
// =====================================================

void checkWiFi()
{
  unsigned long now =
    millis();


  if(
    now -
    lastWiFiCheck <
    WIFI_CHECK_INTERVAL
  )
  {
    return;
  }


  lastWiFiCheck =
    now;


  if(
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    return;
  }


  Serial.println();
  Serial.println(
    "WiFi terputus!"
  );


  Serial.println(
    "Mencoba reconnect..."
  );


  WiFi.disconnect(
    false
  );


  WiFi.begin();


  unsigned long start =
    millis();


  while(
    WiFi.status() !=
    WL_CONNECTED
    &&
    millis() -
    start <
    5000
  )
  {
    updateRelayTimers();

    delay(10);
  }


  if(
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    Serial.println(
      "WiFi reconnect BERHASIL"
    );


    Serial.print(
      "SSID: "
    );

    Serial.println(
      WiFi.SSID()
    );


    Serial.print(
      "RSSI: "
    );

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(
      " dBm"
    );
  }

  else
  {
    Serial.println(
      "WiFi reconnect gagal"
    );
  }
}


// =====================================================
// NTP
// =====================================================

void setupTime()
{
  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com"
  );


  struct tm timeinfo;


  unsigned long start =
    millis();


  while(
    !getLocalTime(
      &timeinfo
    )
  )
  {
    if(
      millis() -
      start >=
      10000
    )
    {
      Serial.println(
        "NTP timeout"
      );


      timeValid =
        false;


      lastNTPRetry =
        millis();


      return;
    }


    delay(250);
  }


  timeValid =
    true;


  Serial.println(
    "Waktu WIB OK"
  );
}


// =====================================================
// NTP RETRY
// =====================================================

void retryNTP()
{
  if(
    timeValid
  )
  {
    return;
  }


  unsigned long now =
    millis();


  if(
    now -
    lastNTPRetry <
    NTP_RETRY_INTERVAL
  )
  {
    return;
  }


  lastNTPRetry =
    now;


  struct tm timeinfo;


  if(
    getLocalTime(
      &timeinfo,
      2000
    )
  )
  {
    timeValid =
      true;


    Serial.println(
      "NTP reconnect OK"
    );
  }
}


// =====================================================
// SETUP WIFI
// =====================================================

bool setupWiFi()
{
  pinMode(
    WIFI_CONFIG_BUTTON,
    INPUT_PULLUP
  );


  wifiManager.setHostname(
    "smart-water-esp32"
  );


  wifiManager.setConnectTimeout(
    15
  );


  wifiManager.setConfigPortalTimeout(
    300
  );


  wifiManager.setMinimumSignalQuality(
    10
  );


  wifiManager.setWiFiAutoReconnect(
    true
  );


  wifiManager.setSaveConnect(
    true
  );


  bool forceConfig =
    false;


  if(
    digitalRead(
      WIFI_CONFIG_BUTTON
    ) == LOW
  )
  {
    unsigned long start =
      millis();


    while(
      digitalRead(
        WIFI_CONFIG_BUTTON
      ) == LOW
    )
    {
      if(
        millis() -
        start >=
        5000
      )
      {
        forceConfig =
          true;

        break;
      }


      delay(50);
    }
  }


  if(
    forceConfig
  )
  {
    wifiManager.resetSettings();


    delay(500);


    if(
      !wifiManager.startConfigPortal(
        WIFI_AP_NAME,
        WIFI_AP_PASSWORD
      )
    )
    {
      return false;
    }
  }

  else
  {
    if(
      !wifiManager.autoConnect(
        WIFI_AP_NAME,
        WIFI_AP_PASSWORD
      )
    )
    {
      return false;
    }
  }


  if(
    WiFi.status() ==
    WL_CONNECTED
  )
  {
    Serial.println();
    Serial.println(
      "WiFi TERHUBUNG!"
    );


    Serial.print(
      "SSID: "
    );

    Serial.println(
      WiFi.SSID()
    );


    Serial.print(
      "IP: "
    );

    Serial.println(
      WiFi.localIP()
    );


    Serial.print(
      "RSSI: "
    );

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(
      " dBm"
    );


    return true;
  }


  return false;
}


// =====================================================
// PRINT STATUS
// =====================================================

void printStatus()
{
  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    " SMART WATER & ENERGY"
  );

  Serial.println(
    " FINAL SENSOR + VALVE SYSTEM"
  );

  Serial.println(
    "======================================"
  );


  // WIFI

  Serial.print(
    "WiFi       : "
  );

  Serial.println(
    WiFi.status() ==
    WL_CONNECTED
      ? "CONNECTED"
      : "OFFLINE"
  );


  Serial.print(
    "SSID       : "
  );

  Serial.println(
    WiFi.SSID()
  );


  Serial.print(
    "RSSI       : "
  );

  Serial.print(
    WiFi.RSSI()
  );

  Serial.println(
    " dBm"
  );


  Serial.print(
    "IP         : "
  );

  Serial.println(
    WiFi.localIP()
  );


  // SENSOR

  Serial.println();

  Serial.print(
    "Humidity   : "
  );

  Serial.print(
    humidity,
    1
  );

  Serial.print(
    " % RAW="
  );

  Serial.println(
    humidityRaw
  );


  // Tampilkan RAW distance sensor, bukan persentase,
  // agar pembacaan ultrasonik bisa dicek langsung di Serial Monitor.
  Serial.print(
    "Toren RAW  : "
  );

  if(distance > 0.0)
  {
    Serial.print(
      distance,
      1
    );

    Serial.println(
      " cm (sensor -> permukaan air)"
    );
  }
  else
  {
    Serial.println(
      "TIDAK TERBACA"
    );
  }


  Serial.print(
    "Flow 1     : "
  );

  Serial.print(
    flowRate1,
    2
  );

  Serial.println(
    " L/min"
  );


  Serial.print(
    "Flow 2     : "
  );

  Serial.print(
    flowRate2,
    2
  );

  Serial.println(
    " L/min"
  );


  Serial.print(
    "RPM        : "
  );

  Serial.print(
    rpmGenerator,
    0
  );

  Serial.println(
    " RPM"
  );


  // VALVE

  Serial.println();

  Serial.print(
    "3-WAY FLAG : "
  );

  Serial.println(
    valve3Position
  );


  Serial.print(
    "2-WAY FLAG : "
  );

  Serial.println(
    valve2Position
  );


  // RELAY

  Serial.println();

  Serial.println(
    "RELAY:"
  );


  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    Serial.print(
      "K"
    );

    Serial.print(
      i + 1
    );

    Serial.print(
      " = "
    );

    Serial.println(
      relayPhysical[i]
        ? "ON"
        : "OFF"
    );
  }


  Serial.println(
    "======================================"
  );
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(500);


  // ===================================================
  // SENSOR PIN
  // ===================================================

  pinMode(
    HUMIDITY_PIN,
    INPUT
  );


  pinMode(
    TRIG_PIN,
    OUTPUT
  );


  pinMode(
    ECHO_PIN,
    INPUT
  );


  pinMode(
    FLOW1_PIN,
    INPUT_PULLUP
  );


  pinMode(
    FLOW2_PIN,
    INPUT_PULLUP
  );


  pinMode(
    RPM_PIN,
    INPUT_PULLUP
  );


  // ===================================================
  // INTERRUPT
  // ===================================================

  attachInterrupt(
    digitalPinToInterrupt(
      FLOW1_PIN
    ),
    countFlow1Pulse,
    FALLING
  );


  attachInterrupt(
    digitalPinToInterrupt(
      FLOW2_PIN
    ),
    countFlow2Pulse,
    FALLING
  );


  attachInterrupt(
    digitalPinToInterrupt(
      RPM_PIN
    ),
    countRPMPulse,
    FALLING
  );


  // ===================================================
  // PZEM
  // ===================================================

  SerialPZEM.begin(
    9600,
    SERIAL_8N1,
    PZEM_RX_PIN,
    PZEM_TX_PIN
  );


  // ===================================================
  // RELAY
  // ===================================================

  int relayPins[8] = {

    RELAY1_PIN,
    RELAY2_PIN,

    RELAY3_PIN,
    RELAY4_PIN,

    RELAY5_PIN,
    RELAY6_PIN,
    RELAY7_PIN,
    RELAY8_PIN
  };


  for(
    int i = 0;
    i < 8;
    i++
  )
  {
    pinMode(
      relayPins[i],
      OUTPUT
    );


    digitalWrite(
      relayPins[i],
      RELAY_OFF
    );


    relayPhysical[i] =
      false;

    relayTimerActive[i] =
      false;
  }


  // ===================================================
  // DEFAULT FLAG
  // ===================================================

  valve3Position =
    "CLOSED";

  valve2Position =
    "CLOSED";


  // ===================================================
  // WIFI
  // ===================================================

  if(
    !setupWiFi()
  )
  {
    delay(3000);

    ESP.restart();
  }


  // ===================================================
  // TIME
  // ===================================================

  setupTime();


  // ===================================================
  // TIMER
  // ===================================================

  noInterrupts();
  flow1PulseCount = 0;
  flow2PulseCount = 0;
  rpmPulseCount = 0;
  interrupts();
  lastPulseSample =
    millis();

  lastSensorRead =
    millis();

  lastDataSend =
    millis();

  lastControlRead =
    millis();


  // ===================================================
  // CONTROL AWAL
  // ===================================================

  getControlFromAPI();


  Serial.println();
  Serial.println(
    "ESP32 SIAP!"
  );

  Serial.println(
    "K1 = 3WAY BLUE OPEN"
  );

  Serial.println(
    "K2 = 3WAY BROWN CLOSE"
  );

  Serial.println(
    "K3 = 2WAY BLUE OPEN"
  );

  Serial.println(
    "K4 = 2WAY BROWN CLOSE"
  );

  Serial.println(
    "K1 + K3 BOLEH BERSAMA"
  );

  Serial.println(
    "TIMER VALVE = 15 DETIK"
  );
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  unsigned long now =
    millis();


  // ===================================================
  // TIMER RELAY
  // HARUS TERUS BERJALAN
  // ===================================================

  updateRelayTimers();


  // ===================================================
  // WIFI
  // ===================================================

  checkWiFi();


  // ===================================================
  // SENSOR
  // ===================================================

  if(
    now -
    lastSensorRead >=
    SENSOR_INTERVAL
  )
  {
    readHumidity();

    readUltrasonic();

    readPulseSensors();


    lastSensorRead =
      now;
  }


  // ===================================================
  // CONTROL
  // ===================================================

  if(
    now -
    lastControlRead >=
    CONTROL_INTERVAL
  )
  {
    getControlFromAPI();


    lastControlRead =
      now;
  }


  // ===================================================
  // DATA
  // ===================================================

  if(
    now -
    lastDataSend >=
    DATA_INTERVAL
  )
  {
    readPZEM();


    sendToAPI();


    printStatus();


    lastDataSend =
      now;
  }


  // ===================================================
  // NTP
  // ===================================================

  retryNTP();


  // ===================================================
  // LOOP
  // ===================================================

  delay(5);
}