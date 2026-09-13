#include <WiFi.h>
#include <PubSubClient.h>

// 1. ESTRUCTURA Y CONSTANTES AL PRINCIPIO
#define TIPO_FCM 0 
#define TIPO_RM  1 
#define TIPO_KUM 2 
#define TIPO_LCM 3 

struct MensajeIoT {
  uint8_t nodeID;   
  uint8_t type;     
  char payload[16]; 
  uint8_t psn;      
};
// --- MOTOR DE GENERACIÓN DE LLAVES ---
uint64_t tablaLlaves[10]; // Almacenará 10 llaves de 64 bits

// Función de mezcla (fs): genera la llave embrión P0
uint64_t funcion_s(uint64_t primo, uint64_t semilla) {
  return primo ^ semilla; // Operación XOR
}

// Función de generación (fg): calcula la llave final
uint64_t funcion_g(uint64_t p0, uint64_t primo_q) {
  return (p0 * primo_q) ^ (p0 << 3); 
}

// Función de mutación (fm): altera la semilla
uint64_t funcion_m(uint64_t semilla, uint64_t primo_q) {
  return semilla + primo_q + 17; // Alteración matemática
}

// Rutina que construye toda la tabla en cascada
void generarTabla(int P, int Q, int S) {
  Serial.println("\n--- CALCULANDO TABLA DE LLAVES ---");
  uint64_t semillaActual = S;
  
  for(int i = 0; i < 10; i++) {
    uint64_t P0 = funcion_s(P, semillaActual);
    tablaLlaves[i] = funcion_g(P0, Q);
    semillaActual = funcion_m(semillaActual, Q);
    
    // Imprimir los 64 bits dividiendo en dos partes de 32 bits
    uint32_t alta = tablaLlaves[i] >> 32;
    uint32_t baja = tablaLlaves[i] & 0xFFFFFFFF;
    Serial.print("Llave "); Serial.print(i); Serial.print(": ");
    Serial.print(alta, HEX); Serial.println(baja, HEX);
  }
  Serial.println("----------------------------------\n");
}
// --- CIFRADO Y DESCIFRADO POLIMÓRFICO ---
// Función 1: XOR con la llave (reversible aplicando XOR de nuevo)
void aplicar_f1(char* payload, uint64_t llave) {
  uint8_t* bytesLlave = (uint8_t*)&llave;
  for(int i = 0; i < 16; i++) {
    payload[i] = payload[i] ^ bytesLlave[i % 8];
  }
}

// Función 2: Suma con la llave (reversible aplicando Resta)
void aplicar_f2(char* payload, uint64_t llave) {
  uint8_t* bytesLlave = (uint8_t*)&llave;
  for(int i = 0; i < 16; i++) {
    payload[i] = payload[i] + bytesLlave[i % 8];
  }
}
void revertir_f2(char* payload, uint64_t llave) {
  uint8_t* bytesLlave = (uint8_t*)&llave;
  for(int i = 0; i < 16; i++) {
    payload[i] = payload[i] - bytesLlave[i % 8];
  }
}

// El PSN altera la secuencia de las funciones para cifrar
void cifrarPolimorfico(char* payload, uint8_t psn) {
  if (psn % 2 == 0) {
     aplicar_f1(payload, tablaLlaves[0]);
     aplicar_f2(payload, tablaLlaves[1]);
  } else {
     aplicar_f2(payload, tablaLlaves[0]);
     aplicar_f1(payload, tablaLlaves[1]);
  }
}

// El Receptor lee el PSN y aplica las funciones en orden inverso
void descifrarPolimorfico(char* payload, uint8_t psn) {
  if (psn % 2 == 0) {
     revertir_f2(payload, tablaLlaves[1]);
     aplicar_f1(payload, tablaLlaves[0]); // f1 es reversible consigo misma
  } else {
     aplicar_f1(payload, tablaLlaves[1]);
     revertir_f2(payload, tablaLlaves[0]);
  }
}
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "test.mosquitto.org"; 

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado con éxito");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "Transmisor-DSS101-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("conectado al broker MQTT");
    } else {
      Serial.print("falló, código=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}
int contadorMensajes = 0; // Llevará la cuenta de la secuencia
void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
if (!client.connected()) reconnect();
  client.loop();

  if (contadorMensajes == 0) {
    // 1. Mensaje de contacto inicial
    int P = 13, Q = 19, S = random(100, 999);
    generarTabla(P, Q, S); 
    MensajeIoT msg; msg.nodeID = 1; msg.type = TIPO_FCM; msg.psn = 0;
    sprintf(msg.payload, "%d,%d,%d", P, Q, S);
    Serial.println("\n[1] Enviando FCM (Semillas iniciales)...");
    client.publish("DSS101/comunicacion", (uint8_t*)&msg, sizeof(msg));
    contadorMensajes++;
    delay(4000);
  }
  else if (contadorMensajes >= 1 && contadorMensajes <= 2) {
    // 2. Mensajes Regulares
    MensajeIoT msg; msg.nodeID = 1; msg.type = TIPO_RM; msg.psn = random(1, 15);
    strcpy(msg.payload, "Secreto 1!");
    Serial.println("[2] Enviando Mensaje Regular (RM)...");
    cifrarPolimorfico(msg.payload, msg.psn);
    client.publish("DSS101/comunicacion", (uint8_t*)&msg, sizeof(msg));
    contadorMensajes++;
    delay(4000);
  }
  else if (contadorMensajes == 3) {
    // 3. Actualización de Llaves (Nuevos números primos)
    int P = 17, Q = 23, S = random(100, 999); 
    generarTabla(P, Q, S); // Recalcula la tabla
    MensajeIoT msg; msg.nodeID = 1; msg.type = TIPO_KUM; msg.psn = 0;
    sprintf(msg.payload, "%d,%d,%d", P, Q, S);
    Serial.println("\n[3] Enviando KUM (Actualizacion de llaves)...");
    client.publish("DSS101/comunicacion", (uint8_t*)&msg, sizeof(msg));
    contadorMensajes++;
    delay(4000);
  }
  else if (contadorMensajes >= 4 && contadorMensajes <= 5) {
    // 4. Mensajes Regulares con las LLAVES NUEVAS
    MensajeIoT msg; msg.nodeID = 1; msg.type = TIPO_RM; msg.psn = random(1, 15);
    strcpy(msg.payload, "Nuevo Secreto!");
    Serial.println("[4] Enviando RM con llaves nuevas...");
    cifrarPolimorfico(msg.payload, msg.psn);
    client.publish("DSS101/comunicacion", (uint8_t*)&msg, sizeof(msg));
    contadorMensajes++;
    delay(4000);
  }
  else if (contadorMensajes == 6) {
    // 5. Cierre de conexión
    MensajeIoT msg; msg.nodeID = 1; msg.type = TIPO_LCM; msg.psn = 0;
    strcpy(msg.payload, "Fin");
    Serial.println("\n[5] Enviando LCM (Fin de comunicacion)...");
    client.publish("DSS101/comunicacion", (uint8_t*)&msg, sizeof(msg));
    contadorMensajes++;
    delay(4000);
  }
  delay(50);
}