# OR-1: OPEN SOURCE EDUCATIONAL KIT 2024

Este proyecto es una máquina de ritmos construida utilizando un microcontrolador Teensy 4.1 y el Audio Shield Rev D. Reproduce muestras de audio desde una tarjeta SD y envía el audio a través de USB y un DAC analógico. El Teensy está configurado para actuar como un dispositivo de audio USB, permitiendo una salida de audio de alta calidad, adecuada para grabación, transmisión en vivo o procesamiento con software de audio digital.

## Características

- **Reproducción de Audio**: Reproduce archivos WAV almacenados en una tarjeta SD.
- **Salida de Audio USB**: Envía audio directamente a una computadora vía USB.
- **Salida de Audio Analógica**: Salida opcional a un DAC analógico.
- **Interfaz de Usuario**: Controla la máquina de ritmos utilizando botones pulsadores y encoders.
- **Pantalla OLED**: Muestra el estado actual y los ajustes.
- **Carcasa Impresa en 3D**: Carcasa personalizada y accesorios para botones pulsadores.
- **Diseño de PCB**: Esquemático y archivos para la fabricación del PCB.

## Requisitos de Hardware

- Teensy 4.1 (o compatible)
- Audio Shield REV D (opcional para salida analógica)
- Tarjeta MicroSD 
- 3 Botones pulsadores
- 2 Encoders KY-040
- Adafruit NeoTrellis 4x4
- Pantalla OLED Adafruit SSD1306

## Configuración de Pines

- **Tarjeta SD**:
  - CS: Pin 10
  - MOSI: Pin 7
  - SCK: Pin 14
- **Botones Pulsadores**:
  - Botón de Programación: Pin 41
  - Botón de Repetición: Pin 40
  - Botón de Inicio/Parada: Pin 39
  - Botón de Efectos: Pin 28
- **Encoders**:
  - Encoder Principal: Pines 31, 32
  - Encoder de Efectos: Pines 29, 30
- **Dispositivos I2C**:
  - NeoTrellis: Dirección 0x2F
  - Pantalla OLED: Dirección I2C por defecto (0x3C)

## Requisitos de Software

- Arduino IDE con Teensyduino
- Bibliotecas Adafruit GFX y SSD1306
- Bibliotecas Audio, Wire, SPI, SD y SerialFlash

## Configuración

1. **Instalar Arduino IDE y Teensyduino**:
   - Descarga e instala el [Arduino IDE](https://www.arduino.cc/en/software).
   - Instala [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html) para agregar soporte para Teensy al Arduino IDE.

2. **Instalar Bibliotecas Requeridas**:
   - Abre el Arduino IDE.
   - Ve a **Sketch > Include Library > Manage Libraries**.
   - Instala las bibliotecas Adafruit GFX y Adafruit SSD1306.
   - Asegúrate de que las bibliotecas Audio, Wire, SPI, SD y SerialFlash estén instaladas.

3. **Configurar el Teensy**:
   - Ve a **Tools > Board** y selecciona tu modelo de Teensy.
   - Ve a **Tools > USB Type** y selecciona **Audio**.

4. **Conectar el Hardware**:
   - Conecta los botones pulsadores, encoders, NeoTrellis y la pantalla OLED según la configuración de pines.
   - Inserta la tarjeta SD con tus archivos WAV.

5. **Subir el Código**:
   - Abre el archivo `rhythm_machine.ino` en el Arduino IDE.
   - Haz clic en **Upload** para programar el Teensy.

## Uso

- **Reproducción de Audio**:
  - La máquina de ritmos reproducirá automáticamente los archivos de audio listados en el código (`SD.WAV`, `BD.WAV`, `CY.WAV`, etc.).
  - Usa los botones pulsadores para controlar la reproducción y cambiar modos.
  - Los encoders te permiten ajustar el BPM y los parámetros de los efectos.

- **Audio USB**:
  - Conecta el Teensy a tu computadora vía USB.
  - El Teensy debería aparecer como un dispositivo de audio en tu computadora.
  - Puedes usar software de grabación para capturar la salida de audio del Teensy.

- **Salida de Audio por I2S**:
  - Para utilizar la salida de audio por I2S en el jack de audífonos, descomenta las líneas correspondientes en el código.

## Archivos Incluidos

- **Modelo 3D**: Archivos STL para imprimir la carcasa y los accesorios para los botones pulsadores.
- **Diseño de PCB**: Esquemático y archivos Gerber para la fabricación del PCB.

## Resolución de Problemas

- **Sin Sonido**:
  - Asegúrate de que el Teensy esté configurado para Audio USB en el Arduino IDE.
  - Verifica las conexiones a la tarjeta SD y asegúrate de que los archivos WAV estén en el formato correcto.
  - Asegúrate de que la asignación de `AudioMemory()` sea suficiente.

- **Audio Distorsionado**:
  - Esto puede ocurrir en macOS. Consulta el [foro de Teensy](https://forum.pjrc.com/threads/34855-Distorted-audio-when-using-USB-input-on-Teensy-3-1?p=110392&viewfull=1#post110392) para posibles soluciones.

## Licencia

Este proyecto está licenciado bajo la Licencia MIT. Consulta el archivo [LICENSE](LICENSE) para más detalles.

## Contribuciones

¡Las contribuciones son bienvenidas! Por favor, haz un fork de este repositorio y envía pull requests.

## Agradecimientos

- Agradecimientos especiales a [PJRC](https://www.pjrc.com/) por la plataforma Teensy.
- Bibliotecas de [Adafruit](https://www.adafruit.com/) y la comunidad de código abierto.

