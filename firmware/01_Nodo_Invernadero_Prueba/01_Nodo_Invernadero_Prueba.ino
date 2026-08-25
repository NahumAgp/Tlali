#include <Arduino.h>
#include <heltec_unofficial.h>

// Ambos equipos deben usar exactamente los mismos parametros.
constexpr float FRECUENCIA_MHZ = 915.0f;
constexpr float ANCHO_BANDA_KHZ = 125.0f;
constexpr uint8_t FACTOR_PROPAGACION = 9;
constexpr uint8_t CODIFICACION = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t POTENCIA_DBM = 2;  // Potencia baja para pruebas de mesa.
constexpr uint32_t INTERVALO_ENVIO_MS = 5000;
constexpr uint32_t TIMEOUT_ACK_MS = 1800;
constexpr char ID_RADIO[] = "G1";
constexpr char ID_NODO[] = "tlali-npk-01";
constexpr uint8_t VERSION_PROTOCOLO = 1;

constexpr uint8_t VALIDO_SUELO = 1 << 0;
constexpr uint8_t VALIDO_AMBIENTE = 1 << 1;
constexpr uint8_t VALIDO_LUZ = 1 << 2;

struct DatosInvernadero {
  float humedadSueloPct = 45.0f;
  float temperaturaSueloC = 23.0f;
  uint16_t conductividadUsCm = 850;
  float ph = 6.4f;
  uint16_t nitrogenoMgKg = 55;
  uint16_t fosforoMgKg = 35;
  uint16_t potasioMgKg = 90;
  float humedadAmbientePct = 65.0f;
  float temperaturaAmbienteC = 25.0f;
  float luminosidadLux = 1800.0f;
  uint8_t validez = VALIDO_SUELO | VALIDO_AMBIENTE | VALIDO_LUZ;
};

DatosInvernadero datos;
uint32_t secuencia = 0;
uint32_t ultimoEnvio = 0;

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

float limitar(float valor, float minimo, float maximo) {
  return min(max(valor, minimo), maximo);
}

float variar(float actual, float minimo, float maximo, float pasoMaximo) {
  float fraccion = random(-1000, 1001) / 1000.0f;
  return limitar(actual + fraccion * pasoMaximo, minimo, maximo);
}

uint16_t variarEntero(
    uint16_t actual,
    uint16_t minimo,
    uint16_t maximo,
    int16_t pasoMaximo) {
  int32_t nuevo = int32_t(actual) + random(-pasoMaximo, pasoMaximo + 1);
  return uint16_t(constrain(nuevo, int32_t(minimo), int32_t(maximo)));
}

void actualizarDatosSimulados() {
  // Caminata aleatoria: produce tendencias suaves en vez de saltos irreales.
  datos.humedadSueloPct =
      variar(datos.humedadSueloPct, 28.0f, 78.0f, 1.2f);
  datos.temperaturaSueloC =
      variar(datos.temperaturaSueloC, 16.0f, 34.0f, 0.35f);
  datos.conductividadUsCm =
      variarEntero(datos.conductividadUsCm, 350, 1800, 35);
  datos.ph = variar(datos.ph, 5.4f, 7.4f, 0.05f);
  datos.nitrogenoMgKg =
      variarEntero(datos.nitrogenoMgKg, 20, 120, 3);
  datos.fosforoMgKg =
      variarEntero(datos.fosforoMgKg, 10, 90, 2);
  datos.potasioMgKg =
      variarEntero(datos.potasioMgKg, 30, 180, 4);
  datos.humedadAmbientePct =
      variar(datos.humedadAmbientePct, 35.0f, 92.0f, 1.5f);
  datos.temperaturaAmbienteC =
      variar(datos.temperaturaAmbienteC, 14.0f, 42.0f, 0.45f);
  datos.luminosidadLux =
      variar(datos.luminosidadLux, 0.0f, 35000.0f, 650.0f);
}

bool crearTramaRadio(char* salida, size_t capacidad) {
  char contenido[220];
  int escritos = snprintf(
      contenido,
      sizeof(contenido),
      "D|%u|%s|%lu|%lu|%d|%d|%u|%u|%u|%u|%u|%d|%d|%lu|%u",
      VERSION_PROTOCOLO,
      ID_RADIO,
      static_cast<unsigned long>(secuencia),
      static_cast<unsigned long>(millis()),
      int(lroundf(datos.humedadSueloPct * 10.0f)),
      int(lroundf(datos.temperaturaSueloC * 10.0f)),
      datos.conductividadUsCm,
      unsigned(lroundf(datos.ph * 100.0f)),
      datos.nitrogenoMgKg,
      datos.fosforoMgKg,
      datos.potasioMgKg,
      int(lroundf(datos.humedadAmbientePct * 10.0f)),
      int(lroundf(datos.temperaturaAmbienteC * 10.0f)),
      static_cast<unsigned long>(lroundf(datos.luminosidadLux * 10.0f)),
      datos.validez);

  if (escritos <= 0 || size_t(escritos) >= sizeof(contenido)) {
    return false;
  }

  uint16_t crc = crc16Ccitt(
      reinterpret_cast<const uint8_t*>(contenido),
      size_t(escritos));
  int total = snprintf(salida, capacidad, "%s|%04X", contenido, crc);
  return total > 0 && size_t(total) < capacidad;
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

bool procesarAck(const String& ack, uint32_t secuenciaEsperada) {
  if (!validarCrc(ack)) {
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

  both.printf(
      "ACK %lu | RSSI %.1f | SNR %.1f\n",
      secuenciaAck,
      rssiX10 / 10.0f,
      snrX10 / 10.0f);
  return true;
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

void enviarLectura() {
  actualizarDatosSimulados();

  char trama[240];
  if (!crearTramaRadio(trama, sizeof(trama))) {
    both.println("Error creando trama");
    return;
  }

  uint32_t secuenciaEnviada = secuencia++;
  both.printf(
      "TX %lu | suelo %.1f%% | aire %.1fC\n",
      static_cast<unsigned long>(secuenciaEnviada),
      datos.humedadSueloPct,
      datos.temperaturaAmbienteC);

  int16_t estado = radio.transmit(
      reinterpret_cast<const uint8_t*>(trama),
      strlen(trama));
  if (estado != RADIOLIB_ERR_NONE) {
    both.printf("Fallo TX: %d\n", estado);
    return;
  }

  String ack;
  estado = radio.receive(ack, 0, TIMEOUT_ACK_MS);
  if (estado == RADIOLIB_ERR_NONE) {
    if (!procesarAck(ack, secuenciaEnviada)) {
      both.println("ACK invalido");
    }
  } else if (estado == RADIOLIB_ERR_RX_TIMEOUT) {
    both.println("Sin ACK del gateway");
  } else {
    both.printf("Fallo RX ACK: %d\n", estado);
  }
}

void setup() {
  heltec_setup();
  randomSeed(esp_random());
  configurarRadio();

  both.println("TLALI - NODO 1 SIMULADO");
  both.printf("%s | %.1f MHz\n", ID_NODO, FRECUENCIA_MHZ);

  // Provoca un envio inmediato al entrar por primera vez al loop.
  ultimoEnvio = millis() - INTERVALO_ENVIO_MS;
}

void loop() {
  heltec_loop();
  uint32_t ahora = millis();
  if (ahora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = ahora;
    enviarLectura();
  }
}
