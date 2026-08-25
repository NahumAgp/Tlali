#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Wire.h>
#define HELTEC_NO_DISPLAY
#include <heltec_unofficial.h>

HardwareSerial RS485(1);
TwoWire I2CLuz(1);

constexpr uint8_t PIN_RS485_RX = 7;  // RO mediante divisor
constexpr uint8_t PIN_RS485_TX = 6;  // DI
constexpr uint8_t PIN_RS485_DIR = 5; // Solo DE; /RE va permanentemente a GND
constexpr uint8_t PIN_DHT22 = 4;
constexpr uint8_t PIN_LUZ_SDA = 41;
constexpr uint8_t PIN_LUZ_SCL = 42;
constexpr uint8_t PIN_OLED_SDA = 17;
constexpr uint8_t PIN_OLED_SCL = 18;
constexpr uint8_t PIN_OLED_RST = 21;
constexpr uint8_t PIN_VEXT = 36;
constexpr uint8_t DIRECCION_OLED = 0x3C;
constexpr float FRECUENCIA_MHZ = 915.0f;
constexpr float ANCHO_BANDA_KHZ = 125.0f;
constexpr uint8_t FACTOR_PROPAGACION = 9;
constexpr uint8_t CODIFICACION = 7;
constexpr uint8_t SYNC_WORD = 0x12;
constexpr int8_t POTENCIA_DBM = 17;
constexpr uint32_t TIMEOUT_ACK_MS = 1800;
constexpr uint32_t INTERVALO_ROTACION_OLED_MS = 60UL * 1000UL;
constexpr char ID_RADIO[] = "G1";
constexpr uint8_t VERSION_PROTOCOLO = 1;
constexpr uint8_t VALIDO_SUELO = 1 << 0;
constexpr uint8_t VALIDO_AMBIENTE = 1 << 1;
constexpr uint8_t VALIDO_LUZ = 1 << 2;
uint8_t direccionBH1750 = 0;

DHT dht(PIN_DHT22, DHT22);
Adafruit_SSD1306 oled(128, 64, &Wire, PIN_OLED_RST);

struct MedicionesNodo {
  float humedadSuelo = 0;
  float temperaturaSuelo = 0;
  uint16_t conductividad = 0;
  float ph = 0;
  uint16_t nitrogeno = 0;
  uint16_t fosforo = 0;
  uint16_t potasio = 0;
  float humedadAmbiente = 0;
  float temperaturaAmbiente = 0;
  float luminosidad = 0;
  bool sueloValido = false;
  bool ambienteValido = false;
  bool luzValida = false;
};

MedicionesNodo datos;
uint32_t secuenciaLoRa = 0;
bool oledDisponible = false;
uint8_t paginaOled = 0;
uint32_t ultimaRotacionOledMs = 0;
uint8_t ultimoErrorOled = 0xFF;
bool ultimoEstadoCentralOled = false;
bool centralConectada = false;
int16_t ultimoRssiX10 = 0;
int16_t ultimoSnrX10 = 0;

bool enviarComandoBH1750(uint8_t comando) {
  if (direccionBH1750 == 0) {
    return false;
  }

  I2CLuz.beginTransmission(direccionBH1750);
  I2CLuz.write(comando);
  return I2CLuz.endTransmission() == 0;
}

uint8_t detectarDireccionBH1750() {
  const uint8_t direcciones[] = {0x23, 0x5C};

  for (uint8_t direccion : direcciones) {
    I2CLuz.beginTransmission(direccion);
    if (I2CLuz.endTransmission() == 0) {
      return direccion;
    }
  }

  return 0;
}

bool iniciarBH1750() {
  // Encender y reiniciar el registro de medicion del BH1750.
  if (!enviarComandoBH1750(0x01)) {
    return false;
  }
  delay(10);

  if (!enviarComandoBH1750(0x07)) {
    return false;
  }
  delay(10);
  return true;
}

void leerLuminosidad() {
  datos.luzValida = false;
  Serial.println("\n---------- GY-302 LUZ -----------");

  // 0x20: una medicion nueva de alta resolucion. Esperar su conversion.
  if (!enviarComandoBH1750(0x20)) {
    Serial.println("BH1750 no acepta el comando; intentando recuperacion");
    direccionBH1750 = detectarDireccionBH1750();
    if (direccionBH1750 == 0 ||
        !iniciarBH1750() ||
        !enviarComandoBH1750(0x20)) {
      Serial.println("BH1750 sigue sin responder: revisa VCC, SDA, SCL y GND");
      return;
    }
    Serial.printf(
        "BH1750 recuperado en direccion 0x%02X\n",
        direccionBH1750);
  }
  delay(180);

  uint8_t recibidos = I2CLuz.requestFrom(direccionBH1750, uint8_t(2));
  if (recibidos != 2) {
    while (I2CLuz.available()) {
      I2CLuz.read();
    }
    Serial.println("BH1750 sin respuesta: revisa VCC, SDA, SCL, ADDR y GND");
    return;
  }

  uint16_t valorCrudo =
      (uint16_t(I2CLuz.read()) << 8) | uint16_t(I2CLuz.read());
  float lux = valorCrudo / 1.2f;

  datos.luminosidad = lux;
  datos.luzValida = true;

  Serial.printf("Dato crudo BH1750:   %u\n", valorCrudo);
  Serial.printf("Luminosidad:         %.1f lux\n", lux);
}

uint16_t crcModbus(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

uint16_t readU16(const uint8_t* response, uint8_t position) {
  return (uint16_t(response[position]) << 8) |
         response[position + 1];
}

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
  if (datos.sueloValido) {
    validez |= VALIDO_SUELO;
  }
  if (datos.ambienteValido) {
    validez |= VALIDO_AMBIENTE;
  }
  if (datos.luzValida) {
    validez |= VALIDO_LUZ;
  }

  char contenido[220];
  int escritos = snprintf(
      contenido,
      sizeof(contenido),
      "D|%u|%s|%lu|%lu|%d|%d|%u|%u|%u|%u|%u|%d|%d|%lu|%u",
      VERSION_PROTOCOLO,
      ID_RADIO,
      static_cast<unsigned long>(secuenciaLoRa),
      static_cast<unsigned long>(millis()),
      int(lroundf(datos.humedadSuelo * 10.0f)),
      int(lroundf(datos.temperaturaSuelo * 10.0f)),
      datos.conductividad,
      unsigned(lroundf(datos.ph * 100.0f)),
      datos.nitrogeno,
      datos.fosforo,
      datos.potasio,
      int(lroundf(datos.humedadAmbiente * 10.0f)),
      int(lroundf(datos.temperaturaAmbiente * 10.0f)),
      static_cast<unsigned long>(lroundf(datos.luminosidad * 10.0f)),
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
  char trama[240];
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

void leerAmbiente() {
  datos.ambienteValido = false;
  float humedadAmbiente = dht.readHumidity();
  float temperaturaAmbiente = dht.readTemperature();

  Serial.println("\n--------- DHT22 AMBIENTE ---------");
  if (isnan(humedadAmbiente) || isnan(temperaturaAmbiente)) {
    Serial.println("DHT22 sin respuesta: revisa VCC, DATA y GND");
    return;
  }

  datos.humedadAmbiente = humedadAmbiente;
  datos.temperaturaAmbiente = temperaturaAmbiente;
  datos.ambienteValido = true;

  Serial.printf("Humedad ambiente:    %.1f %%\n", humedadAmbiente);
  Serial.printf("Temperatura ambiente: %.1f C\n", temperaturaAmbiente);
}

void leerSensor() {
  datos.sueloValido = false;
  // Dirección 1, función 03, registros 0000-0006.
  const uint8_t solicitud[] = {
    0x01, 0x03, 0x00, 0x00,
    0x00, 0x07, 0x04, 0x08
  };

  while (RS485.available()) {
    RS485.read();
  }

  Serial.printf("RX en reposo: %s\n",
                digitalRead(PIN_RS485_RX) ? "HIGH" : "LOW");

  // Transmitir.
  digitalWrite(PIN_RS485_DIR, HIGH);
  delayMicroseconds(200);

  RS485.write(solicitud, sizeof(solicitud));
  RS485.flush();

  // /RE permanece en GND, por lo que aqui recibimos el eco local.
  // Lo descartamos antes de esperar la respuesta real del sensor.
  while (RS485.available()) {
    RS485.read();
  }

  // Recibir.
  digitalWrite(PIN_RS485_DIR, LOW);

  uint8_t respuesta[19];
  size_t recibidos = 0;
  uint32_t inicio = millis();

  while ((millis() - inicio < 1500) && recibidos < sizeof(respuesta)) {
    if (RS485.available()) {
      respuesta[recibidos++] = RS485.read();
    }
  }

  // Mostrar siempre la trama recibida para diagnosticar direccion,
  // funcion, polaridad A/B, eco local y errores de CRC.
  Serial.printf("\nRX (%u bytes): ", (unsigned)recibidos);
  for (size_t i = 0; i < recibidos; i++) {
    if (respuesta[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(respuesta[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  if (recibidos != sizeof(respuesta)) {
    Serial.printf("Sin respuesta completa. Bytes recibidos: %u\n",
                  (unsigned)recibidos);
    return;
  }

  if (respuesta[0] != 0x01 ||
      respuesta[1] != 0x03 ||
      respuesta[2] != 0x0E) {
    Serial.println("Respuesta Modbus incorrecta");
    return;
  }

  uint16_t crcCalculado = crcModbus(respuesta, 17);
  uint16_t crcRecibido =
      uint16_t(respuesta[17]) |
      (uint16_t(respuesta[18]) << 8);

  if (crcCalculado != crcRecibido) {
    Serial.println("Error de CRC");
    return;
  }

  float humedad = readU16(respuesta, 3) / 10.0f;
  float temperatura =
      static_cast<int16_t>(readU16(respuesta, 5)) / 10.0f;
  uint16_t conductividad = readU16(respuesta, 7);
  float ph = readU16(respuesta, 9) / 10.0f;
  uint16_t nitrogeno = readU16(respuesta, 11);
  uint16_t fosforo = readU16(respuesta, 13);
  uint16_t potasio = readU16(respuesta, 15);

  datos.humedadSuelo = humedad;
  datos.temperaturaSuelo = temperatura;
  datos.conductividad = conductividad;
  datos.ph = ph;
  datos.nitrogeno = nitrogeno;
  datos.fosforo = fosforo;
  datos.potasio = potasio;
  datos.sueloValido = true;

  Serial.println("\n------ CWT-NPKPHCTH-S ------");
  Serial.printf("Humedad:       %.1f %%\n", humedad);
  Serial.printf("Temperatura:   %.1f °C\n", temperatura);
  Serial.printf("Conductividad: %u uS/cm\n", conductividad);
  Serial.printf("pH:            %.1f\n", ph);
  Serial.printf("Nitrógeno:     %u mg/kg\n", nitrogeno);
  Serial.printf("Fósforo:       %u mg/kg\n", fosforo);
  Serial.printf("Potasio:       %u mg/kg\n", potasio);
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
  // La OLED se inicializa una sola vez en setup(). No se sondea con una
  // transmision I2C vacia porque algunos SSD1306 la rechazan aunque funcionan
  // correctamente; ese falso negativo provocaba reinicios y pantalla negra.
  digitalWrite(PIN_VEXT, LOW);
  if (!oledDisponible) {
    return;
  }

  uint8_t errores = 0;
  if (!datos.sueloValido) {
    errores |= 1 << 0;
  }
  if (!datos.ambienteValido) {
    errores |= 1 << 1;
  }
  if (!datos.luzValida) {
    errores |= 1 << 2;
  }

  uint32_t ahora = millis();
  bool cambioEstado =
      errores != ultimoErrorOled ||
      centralConectada != ultimoEstadoCentralOled;
  bool tocaRotar =
      ahora - ultimaRotacionOledMs >= INTERVALO_ROTACION_OLED_MS;

  if (cambioEstado) {
    // Cualquier error nuevo o recuperacion se muestra de inmediato.
    paginaOled = 0;
    ultimoErrorOled = errores;
    ultimoEstadoCentralOled = centralConectada;
    ultimaRotacionOledMs = ahora;
    Serial.printf(
        "OLED actualizada: suelo=%s ambiente=%s luz=%s central=%s\n",
        datos.sueloValido ? "OK" : "ERROR",
        datos.ambienteValido ? "OK" : "ERROR",
        datos.luzValida ? "OK" : "ERROR",
        centralConectada ? "OK" : "SIN_ACK");
  } else if (tocaRotar) {
    paginaOled = (paginaOled + 1) % 4;
    ultimaRotacionOledMs = ahora;
  }

  // La pagina cambia cada minuto, pero su contenido se redibuja en cada
  // lectura para que nunca conserve un "OK" antiguo.
  oled.dim(false);
  oled.ssd1306_command(SSD1306_DISPLAYON);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);

  oled.println("TLALI INVERNADERO");
  if (centralConectada) {
    oled.printf("CENTRAL: OK %.0f dBm\n", ultimoRssiX10 / 10.0f);
  } else {
    oled.println("CENTRAL: SIN ACK");
  }

  // Mantener un resumen visible aun cuando no sea la pagina de diagnostico.
  if (paginaOled != 0 && errores != 0) {
    oled.print("ERR:");
    if (!datos.sueloValido) {
      oled.print(" NPK");
    }
    if (!datos.ambienteValido) {
      oled.print(" DHT");
    }
    if (!datos.luzValida) {
      oled.print(" LUZ");
    }
    oled.println();
  }

  switch (paginaOled) {
    case 0:
      if (errores == 0) {
        oled.println("SENSORES: OK");
      } else {
        oled.println("ERROR DE SENSOR:");
        if (!datos.sueloValido) {
          oled.println("Revisa SUELO RS485");
        }
        if (!datos.ambienteValido) {
          oled.println("Revisa DHT22");
        }
        if (!datos.luzValida) {
          oled.println("Revisa BH1750");
        }
      }
      break;

    case 1:
      oled.println("AMBIENTE: OK");
      if (!datos.ambienteValido || !datos.luzValida) {
        oled.println("Lectura incompleta");
      } else {
        oled.printf("Temp: %.1f C\n", datos.temperaturaAmbiente);
        oled.printf("Hum:  %.1f %%\n", datos.humedadAmbiente);
        oled.printf("Luz:  %.0f lux\n", datos.luminosidad);
      }
      break;

    case 2:
      if (!datos.sueloValido) {
        oled.println("SUELO: SIN LECTURA");
        oled.println("Revisa RS485");
      } else {
        oled.println("SUELO: OK");
        oled.printf("Hum:  %.1f %%\n", datos.humedadSuelo);
        oled.printf("Temp: %.1f C\n", datos.temperaturaSuelo);
        oled.printf("EC:   %u uS/cm\n", datos.conductividad);
        oled.printf("pH:   %.1f\n", datos.ph);
      }
      break;

    default:
      if (!datos.sueloValido) {
        oled.println("NPK: SIN LECTURA");
        oled.println("Revisa RS485");
      } else {
        oled.println("NUTRIENTES: OK");
        oled.printf("N: %u mg/kg\n", datos.nitrogeno);
        oled.printf("P: %u mg/kg\n", datos.fosforo);
        oled.printf("K: %u mg/kg\n", datos.potasio);
      }
      break;
  }

  oled.display();
  oled.ssd1306_command(SSD1306_DISPLAYON);
}

void mostrarPantallaOledSoloTitulo() {
  digitalWrite(PIN_VEXT, LOW);
  if (!oledDisponible) {
    return;
  }

  oled.dim(false);
  oled.ssd1306_command(SSD1306_DISPLAYON);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(13, 28);
  oled.print("TLALI INVERNADERO");
  oled.display();
  oled.ssd1306_command(SSD1306_DISPLAYON);
}

void consultarDireccionSensor() {
  // Consulta universal indicada por el fabricante para conocer el ID.
  const uint8_t solicitud[] = {
    0xFF, 0x03, 0x07, 0xD0,
    0x00, 0x01, 0x91, 0x59
  };

  while (RS485.available()) {
    RS485.read();
  }

  Serial.printf("RX en reposo: %s\n",
                digitalRead(PIN_RS485_RX) ? "HIGH" : "LOW");
  Serial.println("Consultando direccion universal...");

  digitalWrite(PIN_RS485_DIR, HIGH);
  delayMicroseconds(200);
  RS485.write(solicitud, sizeof(solicitud));
  RS485.flush();

  while (RS485.available()) {
    RS485.read();
  }

  digitalWrite(PIN_RS485_DIR, LOW);
  delayMicroseconds(2500);

  while (RS485.available()) {
    RS485.read();
  }

  uint8_t respuesta[7];
  size_t recibidos = 0;
  uint32_t inicio = millis();

  while ((millis() - inicio < 1500) && recibidos < sizeof(respuesta)) {
    if (RS485.available()) {
      respuesta[recibidos++] = RS485.read();
    }
  }

  Serial.printf("RX-ID (%u bytes): ", (unsigned)recibidos);
  for (size_t i = 0; i < recibidos; i++) {
    if (respuesta[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(respuesta[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  if (recibidos != sizeof(respuesta)) {
    Serial.println("El sensor no respondio a la consulta universal");
    return;
  }

  uint16_t crcCalculado = crcModbus(respuesta, 5);
  uint16_t crcRecibido =
      uint16_t(respuesta[5]) |
      (uint16_t(respuesta[6]) << 8);

  if (crcCalculado != crcRecibido) {
    Serial.println("Respuesta de ID con CRC incorrecto");
    return;
  }

  if (respuesta[1] != 0x03 || respuesta[2] != 0x02) {
    Serial.println("Formato de respuesta de ID incorrecto");
    return;
  }

  uint16_t direccion = readU16(respuesta, 3);
  Serial.printf("Direccion Modbus encontrada: %u\n", direccion);
}

void probarDriverRS485() {
  // Prueba estatica para medir la salida diferencial con multimetro.
  RS485.end();

  pinMode(PIN_RS485_TX, OUTPUT);
  pinMode(PIN_RS485_DIR, OUTPUT);
  pinMode(PIN_RS485_RX, INPUT_PULLUP);

  // Habilitar el transmisor. /RE queda deshabilitado porque esta unido a DE.
  digitalWrite(PIN_RS485_DIR, HIGH);

  digitalWrite(PIN_RS485_TX, HIGH);
  Serial.println("PRUEBA DI=HIGH: mide A-B durante 5 segundos");
  delay(5000);

  digitalWrite(PIN_RS485_TX, LOW);
  Serial.println("PRUEBA DI=LOW: mide A-B durante 5 segundos");
  delay(5000);
}

bool probarConfiguracion(uint32_t baud, uint8_t direccion) {
  RS485.end();
  delay(20);
  RS485.begin(baud, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);

  uint8_t solicitud[] = {
    direccion, 0x03, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00
  };

  uint16_t crcSolicitud = crcModbus(solicitud, 6);
  solicitud[6] = crcSolicitud & 0xFF;
  solicitud[7] = crcSolicitud >> 8;

  digitalWrite(PIN_RS485_DIR, LOW);
  while (RS485.available()) {
    RS485.read();
  }

  digitalWrite(PIN_RS485_DIR, HIGH);
  delayMicroseconds(200);
  RS485.write(solicitud, sizeof(solicitud));
  RS485.flush();

  while (RS485.available()) {
    RS485.read();
  }
  digitalWrite(PIN_RS485_DIR, LOW);

  uint8_t respuesta[16];
  size_t recibidos = 0;
  uint32_t inicio = millis();

  while (millis() - inicio < 500) {
    while (RS485.available() && recibidos < sizeof(respuesta)) {
      respuesta[recibidos++] = RS485.read();
    }
  }

  if (recibidos == 0) {
    return false;
  }

  Serial.printf("Respuesta en %lu baud, ID %u (%u bytes): ",
                (unsigned long)baud,
                direccion,
                (unsigned)recibidos);
  for (size_t i = 0; i < recibidos; i++) {
    if (respuesta[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(respuesta[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  if (recibidos < 5) {
    return false;
  }

  uint16_t crcCalculado = crcModbus(respuesta, recibidos - 2);
  uint16_t crcRecibido =
      uint16_t(respuesta[recibidos - 2]) |
      (uint16_t(respuesta[recibidos - 1]) << 8);

  if (crcCalculado != crcRecibido) {
    return false;
  }

  if (respuesta[0] != direccion) {
    return false;
  }

  // Aceptar respuesta normal 0x03 o excepcion 0x83: ambas prueban
  // que existe un dispositivo Modbus en esa direccion.
  return respuesta[1] == 0x03 || respuesta[1] == 0x83;
}

void escanearSensor() {
  const uint32_t velocidades[] = {2400, 4800, 9600};

  Serial.println("\n===== ESCANEO MODBUS CWT =====");
  for (uint32_t baud : velocidades) {
    Serial.printf("Probando %lu baud, IDs 1-10...\n",
                  (unsigned long)baud);

    for (uint8_t direccion = 1; direccion <= 10; direccion++) {
      if (probarConfiguracion(baud, direccion)) {
        Serial.printf("SENSOR ENCONTRADO: baud=%lu, ID=%u\n",
                      (unsigned long)baud,
                      direccion);
        digitalWrite(PIN_RS485_DIR, LOW);
        return;
      }
    }
  }

  digitalWrite(PIN_RS485_DIR, LOW);
  Serial.println("NO SE ENCONTRO SENSOR EN 2400/4800/9600, IDs 1-10");
}

void probarEcoLocal() {
  const uint8_t patron[] = {
    0x55, 0xAA, 0x00, 0xFF,
    0x12, 0x34, 0x56, 0x78
  };

  while (RS485.available()) {
    RS485.read();
  }

  // Para esta prueba /RE debe estar conectado permanentemente a GND.
  // GPIO 5 controla solamente DE.
  digitalWrite(PIN_RS485_DIR, HIGH);
  delayMicroseconds(200);
  RS485.write(patron, sizeof(patron));
  RS485.flush();
  digitalWrite(PIN_RS485_DIR, LOW);

  uint8_t eco[sizeof(patron)];
  size_t recibidos = 0;
  uint32_t inicio = millis();

  while ((millis() - inicio < 500) && recibidos < sizeof(eco)) {
    if (RS485.available()) {
      eco[recibidos++] = RS485.read();
    }
  }

  Serial.printf("ECO LOCAL (%u bytes): ", (unsigned)recibidos);
  for (size_t i = 0; i < recibidos; i++) {
    if (eco[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(eco[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  bool correcto = recibidos == sizeof(patron);
  for (size_t i = 0; correcto && i < sizeof(patron); i++) {
    correcto = eco[i] == patron[i];
  }

  Serial.println(correcto
      ? "ECO CORRECTO: transmisor y receptor funcionan"
      : "ECO INCORRECTO: revisar /RE, RO, DE, DI o el transceptor");
}

void probarReceptorEstatico() {
  RS485.end();

  pinMode(PIN_RS485_TX, OUTPUT);
  pinMode(PIN_RS485_DIR, OUTPUT);
  pinMode(PIN_RS485_RX, INPUT_PULLUP);

  // DE habilitado; /RE debe estar conectado directamente a GND.
  digitalWrite(PIN_RS485_DIR, HIGH);

  digitalWrite(PIN_RS485_TX, HIGH);
  delay(100);
  int roConDiHigh = digitalRead(PIN_RS485_RX);
  Serial.printf("DI=HIGH, lectura RO/GPIO7=%s. Mide RO durante 3 s\n",
                roConDiHigh ? "HIGH" : "LOW");
  delay(3000);

  digitalWrite(PIN_RS485_TX, LOW);
  delay(100);
  int roConDiLow = digitalRead(PIN_RS485_RX);
  Serial.printf("DI=LOW, lectura RO/GPIO7=%s. Mide RO durante 3 s\n",
                roConDiLow ? "HIGH" : "LOW");
  delay(3000);

  digitalWrite(PIN_RS485_DIR, LOW);

  Serial.println(roConDiHigh != roConDiLow
      ? "RECEPTOR CAMBIA DE ESTADO: RO funciona"
      : "RECEPTOR NO CAMBIA: revisar /RE, RO o el modulo");
}

void probarEcoTTL() {
  const uint8_t patron[] = {
    0x55, 0xAA, 0x00, 0xFF,
    0x12, 0x34, 0x56, 0x78
  };

  while (RS485.available()) {
    RS485.read();
  }

  RS485.write(patron, sizeof(patron));
  RS485.flush();

  uint8_t eco[sizeof(patron)];
  size_t recibidos = 0;
  uint32_t inicio = millis();

  while ((millis() - inicio < 500) && recibidos < sizeof(eco)) {
    if (RS485.available()) {
      eco[recibidos++] = RS485.read();
    }
  }

  Serial.printf("ECO TTL GPIO6->GPIO7 (%u bytes): ",
                (unsigned)recibidos);
  for (size_t i = 0; i < recibidos; i++) {
    if (eco[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(eco[i], HEX);
    Serial.print(' ');
  }
  Serial.println();

  bool correcto = recibidos == sizeof(patron);
  for (size_t i = 0; correcto && i < sizeof(patron); i++) {
    correcto = eco[i] == patron[i];
  }

  Serial.println(correcto
      ? "ECO TTL CORRECTO: UART1 y GPIO 6/7 funcionan"
      : "ECO TTL INCORRECTO: revisar puente GPIO6-GPIO7 o UART1");
}

void setup() {
  heltec_setup();
  delay(1500);
  randomSeed(esp_random());
  configurarLoRa();

  // Encender Vext y arrancar el bus interno de la OLED de la Heltec V3.
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
    oled.println("TLALI");
    oled.println("Iniciando nodo...");
    oled.display();
  }

  pinMode(PIN_RS485_DIR, OUTPUT);
  digitalWrite(PIN_RS485_DIR, LOW);

  RS485.begin(
    4800,
    SERIAL_8N1,
    PIN_RS485_RX,
    PIN_RS485_TX
  );

  // La biblioteca configura GPIO4 con el pull-up interno.
  dht.begin();

  // Bus I2C externo separado del bus que usara la OLED integrada.
  I2CLuz.begin(PIN_LUZ_SDA, PIN_LUZ_SCL, 100000);
  direccionBH1750 = detectarDireccionBH1750();
  bool luzDetectada = iniciarBH1750();

  // IMPORTANTE (ESP32 core 3.x): no llamar pinMode() sobre GPIO6/GPIO7
  // despues de RS485.begin(). Hacerlo desconecta el pin del periferico UART.

  Serial.println("Iniciando prueba del sensor CWT...");
  Serial.printf("LoRa nodo G1 listo en %.1f MHz\n", FRECUENCIA_MHZ);
  Serial.println(oledDisponible
      ? "OLED SSD1306 detectada en 0x3C"
      : "OLED SSD1306 NO detectada en 0x3C");
  if (luzDetectada) {
    Serial.printf("GY-302/BH1750 detectado en 0x%02X\n", direccionBH1750);
  } else {
    Serial.println("GY-302/BH1750 NO detectado en 0x23 ni 0x5C");
  }
}

void loop() {
  heltec_loop();
  leerSensor();
  leerAmbiente();
  leerLuminosidad();
  enviarDatosLoRa();
  mostrarPantallaOled();
  // El jitter reduce colisiones con el nodo de actuadores.
  delay(3000 + random(0, 1201));
}
