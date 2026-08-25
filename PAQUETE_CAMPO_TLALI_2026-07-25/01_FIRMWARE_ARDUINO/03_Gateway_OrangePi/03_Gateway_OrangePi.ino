#include <Arduino.h>
#include <heltec_unofficial.h>

// Deben coincidir con el nodo de invernadero.
constexpr float FRECUENCIA_MHZ = 915.0f;
constexpr float ANCHO_BANDA_KHZ = 125.0f;
constexpr uint8_t FACTOR_PROPAGACION = 9;
constexpr uint8_t CODIFICACION = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t POTENCIA_DBM = 17;
constexpr uint32_t TIMEOUT_RECEPCION_MS = 1200;
constexpr char ID_RADIO_INVERNADERO[] = "G1";
constexpr char ID_RADIO_ACTUADORES[] = "A1";
constexpr char ID_NODO_INVERNADERO[] = "tlali-npk-01";
constexpr char ID_NODO_ACTUADORES[] = "tlali-actuadores-01";
constexpr uint8_t VERSION_PROTOCOLO = 1;

constexpr uint8_t VALIDO_SUELO = 1 << 0;
constexpr uint8_t VALIDO_AMBIENTE = 1 << 1;
constexpr uint8_t VALIDO_LUZ = 1 << 2;
constexpr uint8_t VALIDO_TANQUE_1 = 1 << 0;
constexpr uint8_t VALIDO_TANQUE_2 = 1 << 1;
constexpr uint8_t VALIDO_TEMPERATURA = 1 << 2;
constexpr uint8_t VALIDO_PH = 1 << 3;
constexpr uint8_t VALIDO_TDS = 1 << 4;

struct LecturaRadio {
  uint32_t secuencia = 0;
  uint32_t uptimeMs = 0;
  int humedadSueloX10 = 0;
  int temperaturaSueloX10 = 0;
  int conductividad = 0;
  int phX100 = 0;
  int nitrogeno = 0;
  int fosforo = 0;
  int potasio = 0;
  int humedadAmbienteX10 = 0;
  int temperaturaAmbienteX10 = 0;
  int luminosidadX10 = 0;
  uint8_t validez = 0;
};

struct LecturaActuadores {
  uint32_t secuencia = 0;
  uint32_t uptimeMs = 0;
  int distanciaTanque1X10 = 0;
  int distanciaTanque2X10 = 0;
  int voltajePhMv = 0;
  int voltajeTdsMv = 0;
  int temperaturaX100 = 0;
  uint8_t reles = 0;
  uint8_t validez = 0;
};

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

bool validarCrc(const String& trama) {
  int separador = trama.lastIndexOf('|');
  if (separador <= 0) {
    return false;
  }

  uint16_t esperado = strtoul(trama.substring(separador + 1).c_str(), nullptr, 16);
  uint16_t calculado = crc16Ccitt(
      reinterpret_cast<const uint8_t*>(trama.c_str()),
      size_t(separador));
  return esperado == calculado;
}

bool decodificarTrama(const String& trama, LecturaRadio& lectura) {
  if (!validarCrc(trama)) {
    return false;
  }

  unsigned version = 0;
  char nodo[8] = {};
  unsigned long secuencia = 0;
  unsigned long uptime = 0;
  unsigned validez = 0;
  unsigned crcIgnorado = 0;

  int campos = sscanf(
      trama.c_str(),
      "D|%u|%7[^|]|%lu|%lu|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%u|%x",
      &version,
      nodo,
      &secuencia,
      &uptime,
      &lectura.humedadSueloX10,
      &lectura.temperaturaSueloX10,
      &lectura.conductividad,
      &lectura.phX100,
      &lectura.nitrogeno,
      &lectura.fosforo,
      &lectura.potasio,
      &lectura.humedadAmbienteX10,
      &lectura.temperaturaAmbienteX10,
      &lectura.luminosidadX10,
      &validez,
      &crcIgnorado);

  if (campos != 16 ||
      version != VERSION_PROTOCOLO ||
      strcmp(nodo, ID_RADIO_INVERNADERO) != 0) {
    return false;
  }

  lectura.secuencia = uint32_t(secuencia);
  lectura.uptimeMs = uint32_t(uptime);
  lectura.validez = uint8_t(validez);
  return true;
}

bool decodificarTramaActuadores(
    const String& trama,
    LecturaActuadores& lectura) {
  if (!validarCrc(trama)) {
    return false;
  }

  unsigned version = 0;
  char nodo[8] = {};
  unsigned long secuencia = 0;
  unsigned long uptime = 0;
  unsigned reles = 0;
  unsigned validez = 0;
  unsigned crcIgnorado = 0;

  int campos = sscanf(
      trama.c_str(),
      "B|%u|%7[^|]|%lu|%lu|%d|%d|%d|%d|%d|%u|%u|%x",
      &version,
      nodo,
      &secuencia,
      &uptime,
      &lectura.distanciaTanque1X10,
      &lectura.distanciaTanque2X10,
      &lectura.voltajePhMv,
      &lectura.voltajeTdsMv,
      &lectura.temperaturaX100,
      &reles,
      &validez,
      &crcIgnorado);

  if (campos != 12 ||
      version != VERSION_PROTOCOLO ||
      strcmp(nodo, ID_RADIO_ACTUADORES) != 0) {
    return false;
  }

  lectura.secuencia = uint32_t(secuencia);
  lectura.uptimeMs = uint32_t(uptime);
  lectura.reles = uint8_t(reles);
  lectura.validez = uint8_t(validez);
  return true;
}

void imprimirValor(float valor, bool valido, uint8_t decimales) {
  if (valido) {
    Serial.print(valor, decimales);
  } else {
    Serial.print("null");
  }
}

void imprimirEntero(int valor, bool valido) {
  if (valido) {
    Serial.print(valor);
  } else {
    Serial.print("null");
  }
}

void enviarJsonOrangePi(
    const LecturaRadio& lectura,
    float rssi,
    float snr) {
  bool sueloValido = lectura.validez & VALIDO_SUELO;
  bool ambienteValido = lectura.validez & VALIDO_AMBIENTE;
  bool luzValida = lectura.validez & VALIDO_LUZ;

  Serial.print("TLALI_DATA:{\"v\":1,\"node\":\"");
  Serial.print(ID_NODO_INVERNADERO);
  Serial.print("\",\"type\":\"sensor\",\"seq\":");
  Serial.print(lectura.secuencia);
  Serial.print(",\"uptimeMs\":");
  Serial.print(lectura.uptimeMs);
  Serial.print(",\"data\":{\"soilMoisturePct\":");
  imprimirValor(lectura.humedadSueloX10 / 10.0f, sueloValido, 1);
  Serial.print(",\"soilTemperatureC\":");
  imprimirValor(lectura.temperaturaSueloX10 / 10.0f, sueloValido, 1);
  Serial.print(",\"conductivityUsCm\":");
  imprimirEntero(lectura.conductividad, sueloValido);
  Serial.print(",\"ph\":");
  imprimirValor(lectura.phX100 / 100.0f, sueloValido, 2);
  Serial.print(",\"nitrogenMgKg\":");
  imprimirEntero(lectura.nitrogeno, sueloValido);
  Serial.print(",\"phosphorusMgKg\":");
  imprimirEntero(lectura.fosforo, sueloValido);
  Serial.print(",\"potassiumMgKg\":");
  imprimirEntero(lectura.potasio, sueloValido);
  Serial.print(",\"airHumidityPct\":");
  imprimirValor(lectura.humedadAmbienteX10 / 10.0f, ambienteValido, 1);
  Serial.print(",\"airTemperatureC\":");
  imprimirValor(lectura.temperaturaAmbienteX10 / 10.0f, ambienteValido, 1);
  Serial.print(",\"lightLux\":");
  imprimirValor(lectura.luminosidadX10 / 10.0f, luzValida, 1);
  Serial.print("},\"valid\":{\"soil\":");
  Serial.print(sueloValido ? "true" : "false");
  Serial.print(",\"air\":");
  Serial.print(ambienteValido ? "true" : "false");
  Serial.print(",\"light\":");
  Serial.print(luzValida ? "true" : "false");
  Serial.print("},\"radio\":{\"rssiDbm\":");
  Serial.print(rssi, 1);
  Serial.print(",\"snrDb\":");
  Serial.print(snr, 1);
  Serial.println("}}");
}

void enviarJsonActuadores(
    const LecturaActuadores& lectura,
    float rssi,
    float snr) {
  bool tanque1Valido = lectura.validez & VALIDO_TANQUE_1;
  bool tanque2Valido = lectura.validez & VALIDO_TANQUE_2;
  bool temperaturaValida = lectura.validez & VALIDO_TEMPERATURA;
  bool phValido = lectura.validez & VALIDO_PH;
  bool tdsValido = lectura.validez & VALIDO_TDS;

  Serial.print("TLALI_DATA:{\"v\":1,\"node\":\"");
  Serial.print(ID_NODO_ACTUADORES);
  Serial.print("\",\"type\":\"actuator\",\"seq\":");
  Serial.print(lectura.secuencia);
  Serial.print(",\"uptimeMs\":");
  Serial.print(lectura.uptimeMs);
  Serial.print(",\"data\":{\"tank1DistanceCm\":");
  imprimirValor(
      lectura.distanciaTanque1X10 / 10.0f,
      tanque1Valido,
      1);
  Serial.print(",\"tank2DistanceCm\":");
  imprimirValor(
      lectura.distanciaTanque2X10 / 10.0f,
      tanque2Valido,
      1);
  Serial.print(",\"phVoltageV\":");
  imprimirValor(lectura.voltajePhMv / 1000.0f, phValido, 3);
  Serial.print(",\"tdsVoltageV\":");
  imprimirValor(lectura.voltajeTdsMv / 1000.0f, tdsValido, 3);
  Serial.print(",\"solutionTemperatureCelsius\":");
  imprimirValor(
      lectura.temperaturaX100 / 100.0f,
      temperaturaValida,
      2);
  Serial.print(",\"relay1On\":");
  Serial.print((lectura.reles & 1) ? "true" : "false");
  Serial.print(",\"relay2On\":");
  Serial.print((lectura.reles & 2) ? "true" : "false");
  Serial.print("},\"valid\":{\"tank1\":");
  Serial.print(tanque1Valido ? "true" : "false");
  Serial.print(",\"tank2\":");
  Serial.print(tanque2Valido ? "true" : "false");
  Serial.print(",\"temperature\":");
  Serial.print(temperaturaValida ? "true" : "false");
  Serial.print(",\"phSignal\":");
  Serial.print(phValido ? "true" : "false");
  Serial.print(",\"tdsSignal\":");
  Serial.print(tdsValido ? "true" : "false");
  Serial.print(",\"phCalibrated\":false,\"tdsCalibrated\":false");
  Serial.print("},\"radio\":{\"rssiDbm\":");
  Serial.print(rssi, 1);
  Serial.print(",\"snrDb\":");
  Serial.print(snr, 1);
  Serial.println("}}");
}

bool crearAck(
    const char* idRadio,
    uint32_t secuencia,
    float rssi,
    float snr,
    char* salida,
    size_t capacidad) {
  char contenido[100];
  int escritos = snprintf(
      contenido,
      sizeof(contenido),
      "A|%u|%s|%lu|%d|%d",
      VERSION_PROTOCOLO,
      idRadio,
      static_cast<unsigned long>(secuencia),
      int(lroundf(rssi * 10.0f)),
      int(lroundf(snr * 10.0f)));

  if (escritos <= 0 || size_t(escritos) >= sizeof(contenido)) {
    return false;
  }

  uint16_t crc = crc16Ccitt(
      reinterpret_cast<const uint8_t*>(contenido),
      size_t(escritos));
  int total = snprintf(salida, capacidad, "%s|%04X", contenido, crc);
  return total > 0 && size_t(total) < capacidad;
}

void enviarAck(
    const char* idRadio,
    uint32_t secuencia,
    float rssi,
    float snr) {
  // Da tiempo al nodo remoto para cambiar de TX a RX.
  delay(60);
  char ack[110];
  if (!crearAck(idRadio, secuencia, rssi, snr, ack, sizeof(ack))) {
    return;
  }

  int16_t estado = radio.transmit(
      reinterpret_cast<const uint8_t*>(ack),
      strlen(ack));
  if (estado != RADIOLIB_ERR_NONE) {
    both.printf("Fallo ACK %s: %d\n", idRadio, estado);
  }
}

void configurarRadio() {
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

void setup() {
  heltec_setup();
  configurarRadio();

  both.println("TLALI - GATEWAY ORANGE PI");
  both.printf("RX %.1f MHz | USB 115200\n", FRECUENCIA_MHZ);
}

void loop() {
  heltec_loop();

  String trama;
  int16_t estado = radio.receive(trama, 0, TIMEOUT_RECEPCION_MS);
  if (estado == RADIOLIB_ERR_RX_TIMEOUT) {
    return;
  }
  if (estado != RADIOLIB_ERR_NONE) {
    both.printf("Fallo RX: %d\n", estado);
    return;
  }

  float rssi = radio.getRSSI();
  float snr = radio.getSNR();
  if (trama.startsWith("D|")) {
    LecturaRadio lectura;
    if (!decodificarTrama(trama, lectura)) {
      both.println("Trama G1 descartada: formato o CRC");
      return;
    }

    enviarJsonOrangePi(lectura, rssi, snr);
    both.printf(
        "RX G1 %lu | %.1f dBm | %.1f dB\n",
        static_cast<unsigned long>(lectura.secuencia),
        rssi,
        snr);
    enviarAck(
        ID_RADIO_INVERNADERO,
        lectura.secuencia,
        rssi,
        snr);
    return;
  }

  if (trama.startsWith("B|")) {
    LecturaActuadores lectura;
    if (!decodificarTramaActuadores(trama, lectura)) {
      both.println("Trama A1 descartada: formato o CRC");
      return;
    }

    enviarJsonActuadores(lectura, rssi, snr);
    both.printf(
        "RX A1 %lu | %.1f dBm | %.1f dB\n",
        static_cast<unsigned long>(lectura.secuencia),
        rssi,
        snr);
    enviarAck(
        ID_RADIO_ACTUADORES,
        lectura.secuencia,
        rssi,
        snr);
    return;
  }

  both.println("Trama descartada: tipo desconocido");
}
