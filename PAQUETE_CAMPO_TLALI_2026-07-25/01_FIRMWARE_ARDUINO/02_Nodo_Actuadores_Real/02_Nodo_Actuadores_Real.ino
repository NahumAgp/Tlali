#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>
#define HELTEC_NO_DISPLAY
#include <heltec_unofficial.h>

// Segunda prueba del nodo de actuadores: dos sensores RCWL-1655
// configurados en su modo GPIO predeterminado (TRIG/ECHO).
constexpr uint8_t PIN_ULTRASONICO_1_TRIG = 4;
constexpr uint8_t PIN_ULTRASONICO_1_ECHO = 5;
constexpr uint8_t PIN_ULTRASONICO_2_TRIG = 6;
constexpr uint8_t PIN_ULTRASONICO_2_ECHO = 7;
constexpr uint8_t PIN_PH = 1;
constexpr uint8_t PIN_TDS = 2;
constexpr uint8_t PIN_RELE_1 = 33;
constexpr uint8_t PIN_RELE_2 = 34;
constexpr uint8_t PIN_DS18B20 = 47;
constexpr uint8_t PIN_OLED_SDA = 17;
constexpr uint8_t PIN_OLED_SCL = 18;
constexpr uint8_t PIN_OLED_RST = 21;
constexpr uint8_t PIN_VEXT = 36;
constexpr uint8_t DIRECCION_OLED = 0x3C;
constexpr uint8_t NIVEL_RELE_ACTIVO = LOW;
constexpr uint8_t NIVEL_RELE_APAGADO = HIGH;
constexpr uint32_t TIMEOUT_ECHO_US = 30000;
constexpr uint32_t INTERVALO_LECTURA_MS = 500;
constexpr uint32_t SEPARACION_SENSORES_MS = 100;
constexpr float DISTANCIA_MINIMA_CM = 19.0f;
constexpr uint8_t MUESTRAS_PH = 20;
constexpr uint8_t MUESTRAS_TDS = 30;
// Divisor: 10 kohm entre Po y ADC, 20 kohm entre ADC y GND.
constexpr float FACTOR_DIVISOR_PH = 1.5f;
constexpr char ID_NODO[] = "tlali-actuadores-01";
constexpr uint8_t VERSION_TRAMA = 1;
constexpr float FRECUENCIA_MHZ = 915.0f;
constexpr float ANCHO_BANDA_KHZ = 125.0f;
constexpr uint8_t FACTOR_PROPAGACION = 9;
constexpr uint8_t CODIFICACION = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t POTENCIA_DBM = 17;
constexpr uint32_t TIMEOUT_ACK_MS = 1800;
constexpr uint32_t INTERVALO_ROTACION_OLED_MS = 60UL * 1000UL;
constexpr char ID_RADIO[] = "A1";
constexpr uint8_t VERSION_PROTOCOLO = 1;
constexpr uint8_t VALIDO_TANQUE_1 = 1 << 0;
constexpr uint8_t VALIDO_TANQUE_2 = 1 << 1;
constexpr uint8_t VALIDO_TEMPERATURA = 1 << 2;
constexpr uint8_t VALIDO_PH = 1 << 3;
constexpr uint8_t VALIDO_TDS = 1 << 4;
constexpr float PH_VOLTAJE_MINIMO_V = 0.02f;
constexpr float PH_VOLTAJE_MAXIMO_V = 4.90f;
constexpr float TDS_VOLTAJE_MINIMO_V = 0.02f;
constexpr float TDS_VOLTAJE_MAXIMO_V = 3.25f;

OneWire busTemperatura(PIN_DS18B20);
DallasTemperature sensorTemperatura(&busTemperatura);
Adafruit_SSD1306 oled(128, 64, &Wire, PIN_OLED_RST);

struct EstadoNodoActuadores {
  float distanciaTanque1Cm = -1.0f;
  float distanciaTanque2Cm = -1.0f;
  float voltajePh = 0.0f;
  float voltajeTds = 0.0f;
  float temperaturaSolucionC = 0.0f;
  bool rele1Activo = false;
  bool rele2Activo = false;
  bool temperaturaValida = false;
  bool phSenalValida = false;
  bool tdsSenalValida = false;
};

EstadoNodoActuadores datos;
uint32_t secuenciaTrama = 0;
uint32_t secuenciaLoRa = 0;
bool oledDisponible = false;
uint8_t paginaOled = 0;
uint32_t ultimaRotacionOledMs = 0;
uint8_t ultimoErrorOled = 0xFF;
bool ultimoEstadoCentralOled = false;
bool centralConectada = false;
int16_t ultimoRssiX10 = 0;
int16_t ultimoSnrX10 = 0;

uint16_t crc16Ccitt(const uint8_t* data, size_t longitud) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < longitud; i++) {
    crc ^= uint16_t(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
  }
  return crc;
}

bool validarCrcLoRa(const String& trama) {
  int separador = trama.lastIndexOf('|');
  if (separador <= 0) {
    return false;
  }

  uint16_t esperado =
      strtoul(trama.substring(separador + 1).c_str(), nullptr, 16);
  uint16_t calculado = crc16Ccitt(
      reinterpret_cast<const uint8_t*>(trama.c_str()),
      size_t(separador));
  return esperado == calculado;
}

void configurarLoRa() {
  RADIOLIB_OR_HALT(radio.begin());
  RADIOLIB_OR_HALT(radio.setFrequency(FRECUENCIA_MHZ));
  RADIOLIB_OR_HALT(radio.setBandwidth(ANCHO_BANDA_KHZ));
  RADIOLIB_OR_HALT(radio.setSpreadingFactor(FACTOR_PROPAGACION));
  RADIOLIB_OR_HALT(radio.setCodingRate(CODIFICACION));
  RADIOLIB_OR_HALT(radio.setSyncWord(SYNC_WORD));
  RADIOLIB_OR_HALT(radio.setPreambleLength(8));
  RADIOLIB_OR_HALT(radio.setCRC(true));
  RADIOLIB_OR_HALT(radio.setOutputPower(POTENCIA_DBM));
}

bool crearTramaLoRa(char* salida, size_t capacidad) {
  uint8_t validez = 0;
  if (datos.distanciaTanque1Cm >= 0.0f) {
    validez |= VALIDO_TANQUE_1;
  }
  if (datos.distanciaTanque2Cm >= 0.0f) {
    validez |= VALIDO_TANQUE_2;
  }
  if (datos.temperaturaValida) {
    validez |= VALIDO_TEMPERATURA;
  }
  if (datos.phSenalValida) {
    validez |= VALIDO_PH;
  }
  if (datos.tdsSenalValida) {
    validez |= VALIDO_TDS;
  }

  uint8_t reles =
      (datos.rele1Activo ? 1 : 0) |
      (datos.rele2Activo ? 2 : 0);
  char contenido[180];
  int escritos = snprintf(
      contenido,
      sizeof(contenido),
      "B|%u|%s|%lu|%lu|%d|%d|%d|%d|%d|%u|%u",
      VERSION_PROTOCOLO,
      ID_RADIO,
      static_cast<unsigned long>(secuenciaLoRa),
      static_cast<unsigned long>(millis()),
      int(lroundf(datos.distanciaTanque1Cm * 10.0f)),
      int(lroundf(datos.distanciaTanque2Cm * 10.0f)),
      int(lroundf(datos.voltajePh * 1000.0f)),
      int(lroundf(datos.voltajeTds * 1000.0f)),
      int(lroundf(datos.temperaturaSolucionC * 100.0f)),
      reles,
      validez);

  if (escritos <= 0 || size_t(escritos) >= sizeof(contenido)) {
    return false;
  }

  uint16_t crc = crc16Ccitt(
      reinterpret_cast<const uint8_t*>(contenido),
      size_t(escritos));
  int total = snprintf(salida, capacidad, "%s|%04X", contenido, crc);
  return total > 0 && size_t(total) < capacidad;
}

bool procesarAckLoRa(const String& ack, uint32_t secuenciaEsperada) {
  if (!validarCrcLoRa(ack)) {
    return false;
  }

  unsigned version = 0;
  char nodo[8] = {};
  unsigned long secuenciaAck = 0;
  int rssiX10 = 0;
  int snrX10 = 0;
  unsigned crcIgnorado = 0;
  int campos = sscanf(
      ack.c_str(),
      "A|%u|%7[^|]|%lu|%d|%d|%x",
      &version,
      nodo,
      &secuenciaAck,
      &rssiX10,
      &snrX10,
      &crcIgnorado);

  if (campos != 6 ||
      version != VERSION_PROTOCOLO ||
      strcmp(nodo, ID_RADIO) != 0 ||
      secuenciaAck != secuenciaEsperada) {
    return false;
  }

  Serial.printf(
      "ACK %lu | RSSI %.1f | SNR %.1f\n",
      secuenciaAck,
      rssiX10 / 10.0f,
      snrX10 / 10.0f);
  ultimoRssiX10 = int16_t(rssiX10);
  ultimoSnrX10 = int16_t(snrX10);
  return true;
}

void enviarDatosLoRa() {
  char trama[200];
  if (!crearTramaLoRa(trama, sizeof(trama))) {
    Serial.println("No se pudo crear la trama LoRa");
    centralConectada = false;
    return;
  }

  uint32_t secuenciaEnviada = secuenciaLoRa++;
  int16_t estado = radio.transmit(
      reinterpret_cast<const uint8_t*>(trama),
      strlen(trama));
  if (estado != RADIOLIB_ERR_NONE) {
    Serial.printf("Fallo LoRa TX: %d\n", estado);
    centralConectada = false;
    return;
  }

  String ack;
  estado = radio.receive(ack, 0, TIMEOUT_ACK_MS);
  if (estado == RADIOLIB_ERR_NONE) {
    centralConectada = procesarAckLoRa(ack, secuenciaEnviada);
    if (!centralConectada) {
      Serial.println("ACK LoRa invalido");
    }
  } else if (estado == RADIOLIB_ERR_RX_TIMEOUT) {
    centralConectada = false;
    Serial.println("Sin ACK del gateway");
  } else {
    centralConectada = false;
    Serial.printf("Fallo LoRa RX: %d\n", estado);
  }
}

float medirDistanciaCm(uint8_t pinTrig, uint8_t pinEcho) {
  digitalWrite(pinTrig, LOW);
  delayMicroseconds(5);

  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(12);
  digitalWrite(pinTrig, LOW);

  unsigned long duracionPulso = pulseIn(
      pinEcho,
      HIGH,
      TIMEOUT_ECHO_US);

  if (duracionPulso == 0) {
    return -1.0f;
  }

  // Distancia = tiempo de ida y vuelta * velocidad del sonido / 2.
  return duracionPulso * 0.0343f / 2.0f;
}

void mostrarDistancia(uint8_t numeroSensor, float distanciaCm) {
  if (distanciaCm < 0.0f) {
    Serial.printf("Sensor %u: sin respuesta\n", numeroSensor);
  } else if (distanciaCm < DISTANCIA_MINIMA_CM) {
    Serial.printf(
        "Sensor %u: %.1f cm (dentro de la zona ciega)\n",
        numeroSensor,
        distanciaCm);
  } else {
    Serial.printf("Sensor %u: %.1f cm\n", numeroSensor, distanciaCm);
  }
}

float promediarVoltaje(uint8_t pin, uint8_t muestras) {
  uint32_t sumaMilivoltios = 0;

  for (uint8_t muestra = 0; muestra < muestras; muestra++) {
    sumaMilivoltios += analogReadMilliVolts(pin);
    delayMicroseconds(250);
  }

  return (sumaMilivoltios / float(muestras)) / 1000.0f;
}

float leerVoltajePhModulo() {
  return promediarVoltaje(PIN_PH, MUESTRAS_PH) * FACTOR_DIVISOR_PH;
}

float leerVoltajeTds() {
  return promediarVoltaje(PIN_TDS, MUESTRAS_TDS);
}

void leerTemperaturaSolucion() {
  sensorTemperatura.requestTemperatures();
  float temperaturaC = sensorTemperatura.getTempCByIndex(0);

  datos.temperaturaValida =
      temperaturaC != DEVICE_DISCONNECTED_C &&
      isfinite(temperaturaC) &&
      temperaturaC >= -55.0f &&
      temperaturaC <= 125.0f;

  if (datos.temperaturaValida) {
    datos.temperaturaSolucionC = temperaturaC;
    Serial.printf("Temperatura DS18B20: %.2f C\n", temperaturaC);
  } else {
    Serial.println("DS18B20 sin respuesta: revisa cableado y resistencia");
  }
}

void apagarReles() {
  digitalWrite(PIN_RELE_1, NIVEL_RELE_APAGADO);
  digitalWrite(PIN_RELE_2, NIVEL_RELE_APAGADO);
  datos.rele1Activo = false;
  datos.rele2Activo = false;
}

bool reiniciarOled() {
  pinMode(PIN_OLED_RST, OUTPUT);
  digitalWrite(PIN_OLED_RST, LOW);
  delay(20);
  digitalWrite(PIN_OLED_RST, HIGH);
  delay(20);

  bool iniciada = oled.begin(
      SSD1306_SWITCHCAPVCC, DIRECCION_OLED, false, false);
  if (iniciada) {
    oled.dim(false);
    oled.ssd1306_command(SSD1306_DISPLAYON);
  }
  return iniciada;
}

void mostrarPantallaOled() {
  // La OLED se inicializa una sola vez en setup(). Una transmision I2C vacia
  // puede dar un falso negativo en algunos SSD1306 y no debe usarse para
  // decidir que hay que reiniciar fisicamente la pantalla.
  digitalWrite(PIN_VEXT, LOW);
  if (!oledDisponible) {
    return;
  }

  bool tanque1Valido = datos.distanciaTanque1Cm >= 0.0f;
  bool tanque2Valido = datos.distanciaTanque2Cm >= 0.0f;
  uint8_t errores = 0;
  if (!tanque1Valido) {
    errores |= 1 << 0;
  }
  if (!tanque2Valido) {
    errores |= 1 << 1;
  }
  if (!datos.temperaturaValida) {
    errores |= 1 << 2;
  }
  if (!datos.phSenalValida) {
    errores |= 1 << 3;
  }
  if (!datos.tdsSenalValida) {
    errores |= 1 << 4;
  }

  uint32_t ahora = millis();
  bool cambioEstado =
      errores != ultimoErrorOled ||
      centralConectada != ultimoEstadoCentralOled;
  bool tocaRotar =
      ahora - ultimaRotacionOledMs >= INTERVALO_ROTACION_OLED_MS;

  if (!cambioEstado && !tocaRotar) {
    return;
  }

  if (cambioEstado) {
    paginaOled = 0;
    ultimoErrorOled = errores;
    ultimoEstadoCentralOled = centralConectada;
  } else {
    paginaOled = (paginaOled + 1) % 3;
  }
  ultimaRotacionOledMs = ahora;

  oled.dim(false);
  oled.ssd1306_command(SSD1306_DISPLAYON);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("TLALI ACTUADORES");

  if (centralConectada) {
    oled.printf("CENTRAL: OK %.0f dBm\n", ultimoRssiX10 / 10.0f);
  } else {
    oled.println("CENTRAL: SIN ACK");
  }

  // Mantener los fallos visibles tambien en las paginas de mediciones.
  if (paginaOled != 0 && errores != 0) {
    oled.print("ERR:");
    if (!tanque1Valido) {
      oled.print(" T1");
    }
    if (!tanque2Valido) {
      oled.print(" T2");
    }
    if (!datos.temperaturaValida) {
      oled.print(" TMP");
    }
    if (!datos.phSenalValida) {
      oled.print(" PH");
    }
    if (!datos.tdsSenalValida) {
      oled.print(" TDS");
    }
    oled.println();
  }

  if (paginaOled == 0) {
    if (errores == 0) {
      oled.println("SENSORES: OK");
    } else {
      oled.println("ERROR DE SENSOR:");
      if (!tanque1Valido) {
        oled.println("Revisa ULTRASON 1");
      }
      if (!tanque2Valido) {
        oled.println("Revisa ULTRASON 2");
      }
      if (!datos.temperaturaValida) {
        oled.println("Revisa DS18B20");
      }
      if (!datos.phSenalValida) {
        oled.println("Revisa PH / ADC");
      }
      if (!datos.tdsSenalValida) {
        oled.println("Revisa TDS / ADC");
      }
    }
  } else if (paginaOled == 1) {
    oled.println("NIVELES / TEMP");
    if (tanque1Valido) {
      oled.printf("Tanque 1: %.1f cm\n", datos.distanciaTanque1Cm);
    } else {
      oled.println("Tanque 1: ERROR");
    }
    if (tanque2Valido) {
      oled.printf("Tanque 2: %.1f cm\n", datos.distanciaTanque2Cm);
    } else {
      oled.println("Tanque 2: ERROR");
    }
    if (datos.temperaturaValida) {
      oled.printf("Temp: %.2f C\n", datos.temperaturaSolucionC);
    } else {
      oled.println("Temp: ERROR");
    }
  } else {
    oled.println("SOLUCION / RELES");
    oled.printf("pH ADC: %.3f V\n", datos.voltajePh);
    oled.printf("TDS ADC: %.3f V\n", datos.voltajeTds);
    oled.printf(
        "R1:%s R2:%s\n",
        datos.rele1Activo ? "ON" : "OFF",
        datos.rele2Activo ? "ON" : "OFF");
  }

  oled.display();
  oled.ssd1306_command(SSD1306_DISPLAYON);
}

void imprimirDistanciaJson(float distanciaCm) {
  if (distanciaCm >= 0.0f) {
    Serial.print(distanciaCm, 1);
  } else {
    Serial.print("null");
  }
}

// Trama NDJSON para gateway/API. Los voltajes se conservan sin convertir
// hasta que pH y TDS hayan sido calibrados con soluciones de referencia.
void enviarTramaDatos() {
  Serial.print("TLALI_DATA:{\"v\":");
  Serial.print(VERSION_TRAMA);
  Serial.print(",\"node\":\"");
  Serial.print(ID_NODO);
  Serial.print("\",\"type\":\"actuator\",\"seq\":");
  Serial.print(secuenciaTrama++);
  Serial.print(",\"uptimeMs\":");
  Serial.print(millis());
  Serial.print(",\"data\":{\"tank1DistanceCm\":");
  imprimirDistanciaJson(datos.distanciaTanque1Cm);
  Serial.print(",\"tank2DistanceCm\":");
  imprimirDistanciaJson(datos.distanciaTanque2Cm);
  Serial.print(",\"phVoltageV\":");
  Serial.print(datos.voltajePh, 3);
  Serial.print(",\"tdsVoltageV\":");
  Serial.print(datos.voltajeTds, 3);
  Serial.print(",\"solutionTemperatureCelsius\":");
  if (datos.temperaturaValida) {
    Serial.print(datos.temperaturaSolucionC, 2);
  } else {
    Serial.print("null");
  }
  Serial.print(",\"relay1On\":");
  Serial.print(datos.rele1Activo ? "true" : "false");
  Serial.print(",\"relay2On\":");
  Serial.print(datos.rele2Activo ? "true" : "false");
  Serial.print("},\"valid\":{\"tank1\":");
  Serial.print(datos.distanciaTanque1Cm >= 0.0f ? "true" : "false");
  Serial.print(",\"tank2\":");
  Serial.print(datos.distanciaTanque2Cm >= 0.0f ? "true" : "false");
  Serial.print(",\"temperature\":");
  Serial.print(datos.temperaturaValida ? "true" : "false");
  Serial.println(",\"phCalibrated\":false,\"tdsCalibrated\":false}}");
}

void setup() {
  heltec_setup();
  delay(1500);
  randomSeed(esp_random());
  configurarLoRa();

  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW);
  delay(100);
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 100000);
  oledDisponible = reiniciarOled();
  if (oledDisponible) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("TLALI ACTUADORES");
    oled.println("Iniciando nodo...");
    oled.println("CENTRAL: ESPERANDO");
    oled.display();
  }

  pinMode(PIN_ULTRASONICO_1_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONICO_1_ECHO, INPUT);
  pinMode(PIN_ULTRASONICO_2_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONICO_2_ECHO, INPUT);
  pinMode(PIN_PH, INPUT);
  pinMode(PIN_TDS, INPUT);

  // Escribir primero el estado seguro evita un pulso al configurar salidas.
  digitalWrite(PIN_RELE_1, NIVEL_RELE_APAGADO);
  digitalWrite(PIN_RELE_2, NIVEL_RELE_APAGADO);
  pinMode(PIN_RELE_1, OUTPUT);
  pinMode(PIN_RELE_2, OUTPUT);
  apagarReles();
  digitalWrite(PIN_ULTRASONICO_1_TRIG, LOW);
  digitalWrite(PIN_ULTRASONICO_2_TRIG, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_PH, ADC_11db);
  analogSetPinAttenuation(PIN_TDS, ADC_11db);
  sensorTemperatura.begin();
  // 10 bits: resolucion de 0.25 C y conversion maxima de 187.5 ms.
  sensorTemperatura.setResolution(10);

  Serial.println();
  Serial.println("================================");
  Serial.println("TLALI - NODO DE ACTUADORES");
  Serial.println("Nodo de actuadores: modo seguro");
  Serial.println("Sensor 1: TRIG 4 | ECHO 5");
  Serial.println("Sensor 2: TRIG 6 | ECHO 7");
  Serial.println("pH Po: divisor 10k/20k | ADC GPIO 1");
  Serial.println("El pH requiere calibracion; se muestra voltaje");
  Serial.println("TDS A: conexion directa | ADC GPIO 2");
  Serial.println("El TDS requiere calibracion; se muestra voltaje");
  Serial.println("DS18B20: DATA GPIO 47 | pull-up 4.7k a 3.3 V");
  Serial.println("Rele 1: GPIO 33 | Rele 2: GPIO 34");
  Serial.printf("LoRa nodo A1 listo en %.1f MHz\n", FRECUENCIA_MHZ);
  Serial.println("Bomba y ventiladores bloqueados: APAGADOS");
  Serial.println("Pendiente definir voltaje, corriente y protecciones");
  Serial.println("================================");
}

void loop() {
  heltec_loop();
  datos.distanciaTanque1Cm = medirDistanciaCm(
      PIN_ULTRASONICO_1_TRIG,
      PIN_ULTRASONICO_1_ECHO);

  // Evita que el segundo sensor escuche el eco emitido por el primero.
  delay(SEPARACION_SENSORES_MS);

  datos.distanciaTanque2Cm = medirDistanciaCm(
      PIN_ULTRASONICO_2_TRIG,
      PIN_ULTRASONICO_2_ECHO);

  mostrarDistancia(1, datos.distanciaTanque1Cm);
  mostrarDistancia(2, datos.distanciaTanque2Cm);
  datos.voltajePh = leerVoltajePhModulo();
  datos.voltajeTds = leerVoltajeTds();
  datos.phSenalValida =
      isfinite(datos.voltajePh) &&
      datos.voltajePh >= PH_VOLTAJE_MINIMO_V &&
      datos.voltajePh <= PH_VOLTAJE_MAXIMO_V;
  datos.tdsSenalValida =
      isfinite(datos.voltajeTds) &&
      datos.voltajeTds >= TDS_VOLTAJE_MINIMO_V &&
      datos.voltajeTds <= TDS_VOLTAJE_MAXIMO_V;
  leerTemperaturaSolucion();
  Serial.printf("pH - voltaje en Po: %.3f V\n", datos.voltajePh);
  Serial.printf("TDS - voltaje en A: %.3f V\n", datos.voltajeTds);
  if (!datos.phSenalValida) {
    Serial.println("ERROR pH: revisa modulo, divisor y ADC GPIO 1");
  }
  if (!datos.tdsSenalValida) {
    Serial.println("ERROR TDS: revisa modulo y ADC GPIO 2");
  }
  enviarDatosLoRa();
  mostrarPantallaOled();
  enviarTramaDatos();
  Serial.println("--------------------------------");

  // Periodo distinto y jitter para compartir el canal con el nodo G1.
  delay(5000 + random(0, 1501));
}
