# Implementacion-de-algoritmo-de-cifrado-polimorfico

Sistema de comunicación IoT seguro implementado en ESP32 con cifrado polimórfico OTP dinámico y protocolo MQTT (Simulación en Wokwi)

## Descripción

Implementación y simulación de un modelo criptográfico ligero para dispositivos IoT de recursos limitados. El sistema establece una comunicación punto a punto segura entre dos nodos ESP32 simulados en **Wokwi**, utilizando el protocolo **MQTT** y un motor de cifrado polimórfico basado en contraseñas de un solo uso (OTP) dinámicas con tablas de 64 bits.

---

## Características Principales

- **Generación Dinámica de Llaves:** Derivación de tablas en cascada mediante funciones de mezcla, generación y mutación a partir de números primos ($P, Q$) y semillas ($S$).

- **Cifrado Polimórfico:** Alteración secuencial del cifrado (XOR y Suma) basada en la paridad del Packet Sequence Number (PSN).

- **Ciclo de Vida:** Gestión completa mediante mensajes de control:
  - `FCM`: Establecimiento de contacto inicial.
  - `RM`: Transmisión de datos regulares cifrados.
  - `KUM`: Actualización de claves en caliente.
  - `LCM`: Cierre seguro y destrucción de llaves en memoria RAM.

---

## Descripción y Funcionamiento del Código

El código fuente está dividido lógicamente para gestionar la red, el motor criptográfico y el ciclo de vida de la sesión en ambos microcontroladores ESP32:

### 1. Estructura de Mensajería (`MensajeIoT`)

Ambos nodos comparten una estructura de datos ligera que prescinde de cabeceras de red complejas, conteniendo únicamente cuatro campos esenciales:

- `nodeID`: Identifica al dispositivo emisor.
- `type`: Define el propósito del paquete mediante cuatro estados clave (`TIPO_FCM` = 0, `TIPO_RM` = 1, `TIPO_KUM` = 2, `TIPO_LCM` = 3).
- `payload`: Arreglo de 16 bytes que transporta el texto secreto o los números semilla.
- `psn`: _Packet Sequence Number_, un número de secuencia que controla la mutación del cifrado.

### 2. Motor Matemático de Generación de Llaves (`generarTabla`)

Para evitar contraseñas estáticas, el sistema genera de forma determinista una tabla de 10 llaves de 64 bits en ambos nodos sin transmitirlas por la red:

- **`funcion_s(primo, semilla)`**: Aplica una operación a nivel de bits **XOR** (`^`) entre el número primo $P$ y la semilla para crear una llave embrión ($P_0$).
- **`funcion_g(p0, primo_q)`**: Multiplica la llave embrión por el segundo primo $Q$ y le aplica un desplazamiento de bits (`p0 << 3`) para estructurar la llave final de 64 bits.
- **`funcion_m(semilla, primo_q)`**: Muta la semilla sumándole el primo $Q$ y una constante (17) para asegurar que cada iteración de la tabla sea completamente distinta.

### 3. Cifrado y Descifrado Polimórfico (`cifrarPolimorfico` / `descifrarPolimorfico`)

El núcleo del modelo cambia la huella digital del cifrado en cada paquete basándose en el **PSN**:

- Si el PSN es **par** (`psn % 2 == 0`), el transmisor aplica primero la función `f1` (XOR de inversión de bytes) y luego `f2` (suma aritmética).
- Si el PSN es **impar**, invierte la secuencia (`f2` seguida de `f1`).
- El receptor lee el PSN y ejecuta las operaciones matemáticas inversas (como la resta y la reversión del XOR) en orden contrario para revelar el texto original.

### 4. Lógica de Control del Receptor (`callback`)

Actúa como una interrupción que escucha el broker MQTT y decide cómo reaccionar según el tipo de mensaje:

- **FCM / KUM**: Extrae los nuevos números primos ($P, Q$) y la semilla ($S$) del payload usando `sscanf`, ejecutando inmediatamente `generarTabla()` para sincronizar o actualizar las llaves de seguridad en caliente.
- **RM**: Extrae el PSN, ejecuta `descifrarPolimorfico()` y muestra el mensaje recuperado en el Monitor Serie.
- **LCM**: Ejecuta un bucle de seguridad que sobrescribe todo el arreglo de llaves con ceros (`tablaLlaves[i] = 0;`), destruyendo la información de la memoria RAM del ESP32.

### 5. Automatización del Transmisor (`loop`)

Gestiona el flujo temporal de la simulación utilizando una variable `contadorMensajes` para enviar de forma ordenada y automatizada el contacto inicial, los mensajes cifrados regulares, la actualización de claves y el cierre seguro de la conexión.

---

## Instrucciones de Ejecución

1. Abrir las simulaciones de Wokwi (Nodo Transmisor y Nodo Receptor) en dos pestañas separadas del navegador.
2. Iniciar primero la simulación en el **Nodo Receptor** para activar la escucha en el broker MQTT (`test.mosquitto.org`).
3. Iniciar la simulación en el **Nodo Transmisor** para disparar la secuencia automatizada.
4. Observar el Monitor Serie del receptor para verificar la generación idéntica de tablas, el descifrado polimórfico de los mensajes `RM`, la actualización `KUM` y el borrado seguro de memoria `LCM`.
