#include <Adafruit_NeoTrellis.h>
#include <Encoder.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//------------------------------------------- VARIABLES GLOBALES Y OBJETOS --------------------------------------------

//--------------------- AUDIO ----------------------------

AudioPlaySdWav playSdWav[16]; // objetos de audio para almacenar samples 
AudioOutputI2S i2s1; // habilitar comunicación i2s para audio 
AudioMixer4 mixer1, mixer2, mixer3, mixer4, sampleMixer, effectMixer1, effectMixer2, effectMixer3, outputMixer; // objetos para la cadena de audio 

//Utilizado para la salida USB
AudioOutputUSB usbaudio; 
AudioOutputAnalog dac;
AudioConnection *patchCords[36]; // conexiones para la cadena de audio 
AudioControlSGTL5000 sgtl5000_1; 

// Audio FX
AudioFilterStateVariable hpFilter; //objeto de filtro

AudioEffectGranular      granular1; // objeto de efecto

// definición de pin de acceso a tarjeta SD con samples en teensy AB
#define SDCARD_CS_PIN 10
#define GRANULAR_MEMORY_SIZE 12800
int16_t granularMemory[GRANULAR_MEMORY_SIZE];

// Samples de Audio
// de utilizar otros samples, el formato es en MAYUSCULAS.WAV
const char* sampleFiles[16] = {"BD.WAV", "CB.WAV", "CH.WAV", "CL.WAV", "CP.WAV", "CY.WAV", "HC.WAV", "HT.WAV", "LC.WAV", "LT.WAV", "MA.WAV", "MC.WAV", "MT.WAV", "OH.WAV", "RS.WAV", "SD.WAV"};

/// variables para filtros
float hpFilterFreq = 100.0;  // Start at 100 Hz
int hpFilterStep = 0; // filtro pasa altas HPF inicializado en 0
const int maxSteps = 100;  // numero maximo de steps del filtro 
float hpFilterDryWet; // dry/wet para HPF

int granularFxStep = 0; // step inicial 0 para efecto granular
float granularFxWetDry = granularFxStep / 10.0; // wet/dry logic de efecto granular

// volumen inicial 
float masterVolume = 0.8;
// steps y playback 

unsigned long lastPlaybackTime = 0; // almacenamiento de la ultima reproducción de un sonido 


//--------------------- TRELLIS ----------------------------
Adafruit_NeoTrellis trellis(0x2F);
//duración de blink al seleccionar samples
const uint32_t blinkDuration = 500; // duracion del parpadeo de LEDs al seleccionar un sample en ms
uint32_t colors[16]; // almacena los colores asignados a cada pad del trellis
int sequencerSteps[16] = {0}; //array que guarda los pasos del secuenciador indicando qué sample se reproduce en cada paso
//otras variables
int repeatPadNum = -1; // indica el numero de pad que se repite 
int selectedSample = -1; // indica el número de sample seleccionado
unsigned long lastStepTime = 0; // alacenamiento de ultimo step del secuenciador


//--------------------- ENCODERS ----------------------------
#define EFFECTS_BUTTON_PIN 28 // pin asignado para el botón de control de selección  de efectos 
Encoder bpmEncoder(31, 32); // control de BPM
Encoder effectsEncoder(29, 30); // control de efectos 
int lastbpmEncoderPosition = 0; //almacena ultima posición para detectar cambios
int lastEffectsEncoderPosition = 0; //almacena ultima posición para detectar cambios
int BPM = 120; // bpm inicial
bool lastEffectsButtonState = HIGH; //
unsigned long MS_PER_BEAT = 60000 / BPM; // conversión de bpm a ms
//estados iniciales de funciones
bool adjustingFx = false; // indica si el sistema está actualmente ajustando el volumen o efecto o filtro 
int activeFx = -1; // mantiene el índice del volumen efecto o filtro activo para saber qué se está modificando

//--------------------- PANTALLA ----------------------------
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//--------------------- BOTONES ----------------------------
#define PROGRAM_BUTTON_PIN 41 //pin para boton de program/freeplay
#define REPEAT_BUTTON_PIN 40 // pin para boton de repeatmode
#define START_STOP_BUTTON_PIN 39 // boton de play/pausa 
bool lastProgramButtonState = HIGH; 
bool lastStartStopButtonState = HIGH;

//banderas para saber qué función está activa 
bool repeatMode = false;
bool programMode = false;
bool isPlaying = true;

//--------------------- MENU ----------------------------
unsigned long lastMenuItemChange = 0; //guarda tiempo desde el ultimo cambio en el item del menu 
const unsigned long menuItemDisplayDuration = 2000; // 2s que se despliega el item del menu en la pantalla 
const char* fxMenuItems[] = {"Volume", "High-Pass", "GranularFx"}; //array de items en el menu
int numItems = sizeof(fxMenuItems) / sizeof(fxMenuItems[0]); //calcula el número de items en el array
int currentMenuItem = 0; //indica que se ha seleccionado en el menu

//------------------------------------------------------- FUNCIONES ----------------------------------------------------------------------


//------------------------------------------- SETUP DE HARDWARE --------------------------------------------
// SetUp inicializa el hardware y las conexiones de audio
void setup() {
    Serial.begin(9600);
    initializeHardware();
    initializeAudioConnections();
}


//inicialización de hardware: MicroSD de Audio Board, Pantalla y NeoTrellis
void initializeHardware() {
    AudioMemory(50);
    sgtl5000_1.enable();
    sgtl5000_1.volume(0.7);

    //screen setup
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // DIRECCION I2C 0x3C PARA SSD1306
  Serial.println(F("SSD1306 allocation failed"));
  for(;;);
  }
  
  display.display();
  delay(2000); 

  // limpieza de buffer
  display.clearDisplay();
  //inicialización neotrellis 
  trellis.begin();
    assignColors();
    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        trellis.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
        trellis.registerCallback(i, keyEventCallback);
    }
    startupAnimation();
    
    // inicialización de pines para botones con resistencia pullup
    pinMode(PROGRAM_BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(EFFECTS_BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(REPEAT_BUTTON_PIN, INPUT_PULLDOWN);
    pinMode(START_STOP_BUTTON_PIN, INPUT_PULLDOWN);  
}

// Inicialización de conexiones de Audio 
void initializeAudioConnections() {

  granular1.begin(granularMemory, GRANULAR_MEMORY_SIZE); 

    if (!SD.begin(SDCARD_CS_PIN)) {
        Serial.println("Unable to access the SD card");
        while (1) delay(500);
    }


  for (int i = 0; i < 4; i++) { 
        mixer1.gain(i, 0.75); // gain inicial de mixers
        mixer2.gain(i, 0.75); // 
        mixer3.gain(i, 0.75); // 
        mixer4.gain(i, 0.75); // 
    }

    // Set gains para los mixers de samples
    sampleMixer.gain(0, 1.0);
    sampleMixer.gain(1, 1.0);
    sampleMixer.gain(2, 1.0);
    sampleMixer.gain(3, 1.0);

    // Ejemplo para setear los gains del mixer de efectos 
    effectMixer1.gain(0, hpFilterDryWet);
    effectMixer1.gain(1, 1.0 - hpFilterDryWet);

    effectMixer2.gain(0, granularFxWetDry);
    effectMixer2.gain(1, 1.0 - granularFxWetDry);

    // Ajustar el gain del output inicial 
    outputMixer.gain(0,0.5);



    //patcheo de conexiones de audio a los mixers iniciales, uno para cada fila del secuenciador (4 samples)
    for (int i = 0; i < 4; i++) {
        patchCords[i] = new AudioConnection(playSdWav[i], 0, mixer1, i);
    }
    for (int i = 4; i < 8; i++) {
        patchCords[i] = new AudioConnection(playSdWav[i], 0, mixer2, i - 4);
    }
    for (int i = 8; i < 12; i++) {
        patchCords[i] = new AudioConnection(playSdWav[i], 0, mixer3, i - 8);
    }
    for (int i = 12; i < 16; i++) {
        patchCords[i] = new AudioConnection(playSdWav[i], 0, mixer4, i - 12);
    }
    //patcheo de todos los samples en sus mixers para tenerlos en una sola salida 
    patchCords[16] = new AudioConnection(mixer1, 0, sampleMixer, 0);
    patchCords[17] = new AudioConnection(mixer2, 0, sampleMixer, 1);
    patchCords[18] = new AudioConnection(mixer3, 0, sampleMixer, 2);
    patchCords[19] = new AudioConnection(mixer4, 0, sampleMixer, 3);

    //PATCHEO GRANULAR
    patchCords[20] = new AudioConnection(sampleMixer, granular1); 
    patchCords[21] = new AudioConnection(sampleMixer, 0, effectMixer1, 1);
    patchCords[22] = new AudioConnection(granular1, 0, effectMixer1, 0);
    
    //PATCHEO HPF
    patchCords[23] = new AudioConnection(effectMixer1, 0, hpFilter, 0);
    patchCords[24] = new AudioConnection(hpFilter, 2, effectMixer2, 0);

    //HPF Y GRANULAR A MIXER 2
    patchCords[25] = new AudioConnection(effectMixer1, 0, effectMixer2, 1);

    // MIXER A OUTPUT MIXER
    patchCords[26] = new AudioConnection(effectMixer2, 0, outputMixer, 0);

    //OUTPUT MIXER A USBAUDIO POR DEFAULT
    patchCords[27] = new AudioConnection(outputMixer, 0, usbAudio, 0);
    patchCords[28] = new AudioConnection(outputMixer, 0, usbAudio, 1);
    patchCords[29] = new AudioConnection(outputMixer, 0, dac, 0);

    /* DESCOMENTAR SI SE DESEA UTILIZAR LA SALIDA DE AUDIFONOS 3.5MM
    // OUTPUT MIXER A I2S STEREO DEL AUDIO BOARD
    patchCords[31] = new AudioConnection(effectMixer1, 0, outputMixer, 0);
    patchCords[27] = new AudioConnection(outputMixer, 0, i2s1, 0);
    patchCords[28] = new AudioConnection(outputMixer, 0, i2s1, 1);
  */
}

//función de asignación de colores a cada pad
void assignColors() {
    colors[0] = trellis.pixels.Color(255, 0, 0); // Red
    colors[1] = trellis.pixels.Color(0, 255, 0); // Green
    colors[2] = trellis.pixels.Color(0, 0, 255); // Blue
    colors[3] = trellis.pixels.Color(255, 255, 0); // Yellow
    colors[4] = trellis.pixels.Color(0, 255, 255); // Cyan
    colors[5] = trellis.pixels.Color(255, 0, 255); // Magenta
    colors[6] = trellis.pixels.Color(255, 165, 0); // Orange
    colors[7] = trellis.pixels.Color(128, 0, 128); // Purple
    colors[8] = trellis.pixels.Color(255, 192, 203); // Pink
    colors[9] = trellis.pixels.Color(128, 128, 0); // Olive
    colors[10] = trellis.pixels.Color(0, 128, 0); // Dark Green
    colors[11] = trellis.pixels.Color(128, 0, 0); // Maroon
    colors[12] = trellis.pixels.Color(0, 128, 128); // Teal
    colors[13] = trellis.pixels.Color(0, 0, 128); // Navy
    colors[14] = trellis.pixels.Color(139, 69, 19); // Saddle Brown
    colors[15] = trellis.pixels.Color(75, 0, 130); // Indigo
}


//------------------------------------------- MANEJO DE INTERFAZ DE USUARIO --------------------------------------------
// función para cambiar entre program mode y freeplay mode
void toggleProgramMode() { 
    bool newProgramButtonState = digitalRead(PROGRAM_BUTTON_PIN);
    if (newProgramButtonState != lastProgramButtonState && newProgramButtonState == LOW) { //detecta cambio de estado
        programMode = !programMode;
        selectedSample = -1;  // RESETEA EL SAMPLE SELECCIONADO 
        Serial.println(programMode ? "Programming Mode" : "Free Play Mode"); //debug en serial 
    }
    lastProgramButtonState = newProgramButtonState;
}
// función para play / pause
void toggleStartStop() { 
    bool newStartStopButtonState = digitalRead(START_STOP_BUTTON_PIN);
    if (newStartStopButtonState != lastStartStopButtonState && newStartStopButtonState == LOW) { //detecta cambio de estado
        isPlaying = !isPlaying; 
        if (!isPlaying) {
            for (int i = 0; i < 16; ++i) {
                playSdWav[i].stop();
            }
        }
        delay(300); // Debounce delay
        Serial.println(isPlaying ? "Sequencer Resumed" : "Sequencer Paused");
    }
    lastStartStopButtonState = newStartStopButtonState;
}

//función para ajustar BPM 
void handleBpmAdjustment() { 
    int currentbpmEncoderPosition = bpmEncoder.read();
    if (currentbpmEncoderPosition != lastbpmEncoderPosition) {
        int encoderChange = currentbpmEncoderPosition - lastbpmEncoderPosition;
        BPM += encoderChange; // detecta cambios en el encoder para determinar el nuevo BPM
        BPM = constrain(BPM, 30, 300); // incremento de 30 debido al encoder KY040
        MS_PER_BEAT = 60000 / BPM;
        lastbpmEncoderPosition = currentbpmEncoderPosition;
        Serial.print("BPM: ");
        Serial.println(BPM);
    }
}

// función para selección de volumen, filtro o efecto
void toggleFxAdjustmentMode() { 
    bool currentEffectsButtonState = digitalRead(EFFECTS_BUTTON_PIN);
    if (currentEffectsButtonState == LOW && lastEffectsButtonState == HIGH) {  //lee el estado anterior para determinar si cambia
        adjustingFx = !adjustingFx;
        if (adjustingFx) {
            activeFx = currentMenuItem;
            Serial.print("Adjusting: ");
            Serial.println(fxMenuItems[currentMenuItem]);
        } else {
            activeFx = -1;
            Serial.println("Adjustment mode exited");
        }
    }
    lastEffectsButtonState = currentEffectsButtonState;
}

//manejo del encoder de efectos y su menu 
void handleEffectsEncoder() {
    static int effectsDetentCounter = 0;
    int currentEffectsEncoderPosition = effectsEncoder.read();

    if (currentEffectsEncoderPosition != lastEffectsEncoderPosition) {
        int encoderChange = currentEffectsEncoderPosition - lastEffectsEncoderPosition;
        effectsDetentCounter += abs(encoderChange);  // Contador del incremento por el cambio absoluto por encoder KY040 
        if (effectsDetentCounter >= 4) {  // Usando 4 como un threshold del dent para menos sensibilidad del encoder para navegación en el menu
            effectsDetentCounter = 0; 
            int menuChange = (encoderChange > 0) ? 1 : -1; 
            currentMenuItem += menuChange;
            currentMenuItem = (currentMenuItem + numItems) % numItems; // darle vuelta al menu de selección cuando se llega al ultimo elemento

            if (!adjustingFx) {
                displayMenu();  // llama funcion de display
                lastMenuItemChange = millis(); // cambia el timestamp al cambiar la seleccion en el menu
            } else if (adjustingFx && activeFx == currentMenuItem) {
                lastMenuItemChange = millis();
                adjustFxParameter(menuChange);
            }
        }

        lastEffectsEncoderPosition = currentEffectsEncoderPosition; // actualiza la ultima posición para la próxima llamada
    }
}

//detección de pad y activación de color
void togglePadState(int padNum) {
    if (sequencerSteps[padNum] == 0) {
        sequencerSteps[padNum] = selectedSample + 1;
        trellis.pixels.setPixelColor(padNum, colors[selectedSample]);
    } else {
        sequencerSteps[padNum] = 0;
        trellis.pixels.setPixelColor(padNum, 0);
    }
    trellis.pixels.show();
}

//para la constante lectura de entradas en el trellis
void pollTrellis() { 
    trellis.read();
}

// manejo de trellis Y modos programing/freeplay y play/pause
TrellisCallback keyEventCallback(keyEvent evt) {
    int padNum = evt.bit.NUM;  // identifica el numero de pad 

    if (evt.bit.EDGE == SEESAW_KEYPAD_EDGE_RISING && padNum >= 0 && padNum < 16) { // identifica eventos del pad
        Serial.println("Edge Rising Detected");

        if (!programMode) { // Modo Freeplay
            if (digitalRead(REPEAT_BUTTON_PIN) == HIGH) {
                // si se presiona el botón de repetición, repetir sample
                repeatMode = true; 
                Serial.println("Repeat Mode Activated"); //debuggeo en serial 
                repeatPadNum = padNum;  // marca el pad para repetición
            } else if (digitalRead(REPEAT_BUTTON_PIN) == LOW){
                // si no se presiona el boton repeat, solo se reproduce una vez 
                repeatMode = false; 
                playSdWav[padNum].stop();
                playSdWav[padNum].play(sampleFiles[padNum]);
                Serial.println("Playing Sample Once");
                //activación de color momentánea
                trellis.pixels.setPixelColor(padNum, colors[padNum]);
                trellis.pixels.show();
                delay(50); //delay para corta visibilidad del color
                trellis.pixels.setPixelColor(padNum, 0);
                trellis.pixels.show();
                repeatPadNum = -1;  // asegurar que no se active modo repetición         
            }
        } else {
            // modo de programación
            if (selectedSample == -1) {
                repeatMode = false; 
                selectedSample = padNum;
                // retroalimentación visual para sample seleccionado 
                trellis.pixels.setPixelColor(selectedSample, colors[selectedSample]);
                trellis.pixels.show();
                delay(blinkDuration);
                trellis.pixels.setPixelColor(selectedSample, 0);
                trellis.pixels.show();
            } else {
                // asignación del sample al paso seleccionado
              togglePadState(padNum);
              printArray();  // debuggeo del array del secuenciador
            }
        }
    }
    return 0;
}

//------------------------------------------- PROCESAMIENTO Y CONTROL DE AUDIO --------------------------------------------

//actualización del secuenciador 
void updateSequencer() { 
    unsigned long currentMillis = millis();
    if (isPlaying && currentMillis - lastStepTime >= MS_PER_BEAT) {
        lastStepTime = currentMillis;
        static int currentStep = 0;
        currentStep = (currentStep + 1) % 16;

        int fileIndex = sequencerSteps[currentStep] - 1;
        if (fileIndex >= 0 && fileIndex < 16) {
            playSdWav[currentStep].stop();
            playSdWav[currentStep].play(sampleFiles[fileIndex]);
        }

        // LEDS para indicar el paso del secuenciador en tiempo real
        for (int i = 0; i < 16; i++) {
            if (i == currentStep) {
                trellis.pixels.setPixelColor(i, trellis.pixels.Color(255, 255, 255));
            } else if (sequencerSteps[i] > 0) {
                uint32_t color = colors[sequencerSteps[i] - 1];
                trellis.pixels.setPixelColor(i, color);
            } else {
                trellis.pixels.setPixelColor(i, 0);
            }
        }
        trellis.pixels.show();
    }
}

// manejo del modo de repetición 
void handleRepeat() { 
    unsigned long currentMillis = millis();
    if (repeatPadNum >= 0 && digitalRead(REPEAT_BUTTON_PIN) == HIGH) {
        if (currentMillis - lastPlaybackTime >= MS_PER_BEAT) {
            playSdWav[repeatPadNum].stop();
            playSdWav[repeatPadNum].play(sampleFiles[repeatPadNum]);
            Serial.println("Repeating Sample");
            lastPlaybackTime = currentMillis;
        }
    } else if (repeatPadNum >= 0 && digitalRead(REPEAT_BUTTON_PIN) == LOW) {
        playSdWav[repeatPadNum].stop();
        Serial.println("Stopping Repeat");
        repeatPadNum = -1;
    }
}

//ajuste de volumen
void adjustVolume(int encoderChange) {
    // ajusta el volumen por incrementos 
    float volumeIncrement = 0.1; // valor de incremento de 0.1 para 10 pasos
    
    // ajuste del volumen master
    masterVolume += encoderChange * volumeIncrement;
    
    // 0 mute 1 volumen máximo
    masterVolume = constrain(masterVolume, 0, 1);
    
    // actualiza el gain 
    outputMixer.gain(0, masterVolume);

    Serial.print("Master Volume: ");
    Serial.println(masterVolume);
}

//ajuste HPF
void adjustHighPassFreq(int encoderChange) {
    
    hpFilterStep += encoderChange;
    hpFilterStep = constrain(hpFilterStep, 0, 10);  // 10 pasos 

    // calcular la frecuencia con el cambio en el encoder
    float minFreq = 100.0;
    float freqIncrement = 490.0;
    float hpFilterFreq = minFreq + (hpFilterStep * freqIncrement);

    // asignar la frecuencia
    hpFilter.frequency(hpFilterFreq);

    // control dry/wet del filtro 
    if (hpFilterStep > 1) {
        hpFilterDryWet = 100; // 
    } else {
        hpFilterDryWet = 0; // 
    }

    // aplicación del dry/wet en el mixer 
    effectMixer2.gain(0, hpFilterDryWet / 100.0);
    effectMixer2.gain(1, 1.0 - hpFilterDryWet / 100.0);

    Serial.print("Step: ");
    Serial.print(hpFilterStep);
    Serial.print(" - HP Filter Frequency: ");
    Serial.print(hpFilterFreq);
    Serial.println(" Hz");
    Serial.print("Dry/Wet Mix: ");
    Serial.print(hpFilterDryWet);
    Serial.println("%");
}

//ajuste efecto Granular
void adjustGranularFx(int encoderChange) {
    // escala el encoderchange directamente al mix wet/dry 
    granular1.beginPitchShift(55); // pitch shift establecido a 55 
    granularFxWetDry += encoderChange*5;  // cada paso cambia el mix por 1%
    granularFxWetDry = constrain(granularFxWetDry, 0, 100); // manejo de 0 a 100 

    effectMixer1.gain(0, granularFxWetDry / 100.0);
    effectMixer1.gain(1, 1.0 - granularFxWetDry / 100.0);

    Serial.print("Granular FX Dry/Wet: ");
    Serial.print(granularFxWetDry);
    Serial.println("% Wet");
}

// controla el manejo de ajustes del menu y activa la función de cada uno 
void adjustFxParameter(int encoderChange) { 
    switch (activeFx) {
        case 0:
            adjustVolume(encoderChange);
            break;
        case 1:
            adjustHighPassFreq(encoderChange);
            break;
        case 2:
            adjustGranularFx(encoderChange);
            break;
        default:
            Serial.println("No active FX to adjust");
            break;
                }
}


//------------------------------------------- FUNCIONES DE AYUDA Y VISUALIZACIÓN --------------------------------------------

void startupAnimation() {
    for (uint16_t i = 0; i < trellis.pixels.numPixels(); i++) {
        trellis.pixels.setPixelColor(i, Wheel(map(i, 0, trellis.pixels.numPixels(), 0, 255)));
        trellis.pixels.show();
        delay(50);
    }
    for (uint16_t i = 0; i < trellis.pixels.numPixels(); i++) {
        trellis.pixels.setPixelColor(i, 0x000000);
        trellis.pixels.show();
        delay(50);
    }
}
// Colores de animación inicial
uint32_t Wheel(byte WheelPos) {
    if (WheelPos < 85) {
        return trellis.pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if (WheelPos < 170) {
        WheelPos -= 85;
        return trellis.pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
        WheelPos -= 170;
        return trellis.pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
}

// debugueo del secuenciador y los samples asignados a los pads
void printArray() {
    Serial.print("Sequencer Steps: ");
    for (int i = 0; i < 16; i++) {
        Serial.print(sequencerSteps[i]);
        Serial.print(" ");
    }
    Serial.println();
}

// despliega el menu para selección de ajuste: volumen, efecto o filtro
void displayMenu() {
  Serial.println("FX Menu:");
  for (int i = 0; i < static_cast<int>(sizeof(fxMenuItems) / sizeof(fxMenuItems[0])); i++) {
    if (i == currentMenuItem) {
      Serial.print("> "); // Indicate the current selection
    } else {
      Serial.print("  ");
    }
    Serial.println(fxMenuItems[i]);
  }
}

// visualización y actualización de la pantalla según los estados
void updateDisplay(int BPM, float volume, bool isPlaying, bool programMode, bool repeatMode, bool adjustingFx) {
  // limpia el buffer
  display.clearDisplay();


  //-----------------------bpm-----------------------------
  int bpmX = 85; // Adjust X coordinate as needed
  int bpmY = 3; // Adjust Y coordinate as needed
  display.setCursor(bpmX, bpmY);
  display.print("BPM:");
  display.print(BPM);
  
  display.setTextSize(1);
  display.setTextColor(WHITE);

  //-----------------------play/pause-----------------------------
  int pauseIconX = 5; // X coordinate for the pause icon
  int pauseIconY = 3; // Y coordinate for the pause icon
  int pauseIconWidth = 2; // Width of each pause bar
  int pauseIconHeight = 5; // Height of each pause bar
  int pauseIconSpacing = 2; // Space between the pause bars

  if (isPlaying) {
    // Draw pause symbol (two vertical bars)
    display.fillRect(pauseIconX, pauseIconY, pauseIconWidth, pauseIconHeight, WHITE);
    display.fillRect(pauseIconX + pauseIconWidth + pauseIconSpacing, pauseIconY, pauseIconWidth, pauseIconHeight, WHITE);
  } else {
    // Draw play symbol (rightward triangle)
    display.fillTriangle(
      pauseIconX, pauseIconY,
      pauseIconX, pauseIconY + pauseIconHeight,
      pauseIconX + pauseIconHeight, pauseIconY + (pauseIconHeight / 2), 
      WHITE
    );
  }
  //-----------------------program/freeplay mode-----------------------------
  if (programMode) {
    int programIconX = 45; // X coordinate for the "P"
    int programIconY = 3; // Y coordinate for the "P"
    
    display.setCursor(programIconX, programIconY);
    display.print("P");
  } else if (!programMode) {
    int programIconX = 40; // X coordinate for the "P"
    int programIconY = 3; 
    
    display.setCursor(programIconX, programIconY);
    display.print("F");

  }
  //-----------------------repeat mode-----------------------------
  if (repeatMode) {
    int repeatIconX = 60; // X coordinate for the "P"
    int repeatIconY = 3; // Y coordinate for the "P"
    
    display.setCursor(repeatIconX, repeatIconY);
    display.print("R");
  } 

  //-------------------------------menu-----------------------------
  if (!adjustingFx && millis() - lastMenuItemChange < menuItemDisplayDuration) {
    // Clear the middle section where the effect name appears
    display.fillRect(0, (display.height() / 2) - 8, display.width(), 16, BLACK);

    // Display the effect name in the middle of the screen
    int centerPosX = (display.width() - 6 * strlen(fxMenuItems[currentMenuItem])) / 2;
    int centerPosY = display.height() / 2;
    display.setCursor(centerPosX, centerPosY);
    display.print(fxMenuItems[currentMenuItem]);
  }
  // mostrar el item del menu por 2 segundos luego de un cambio en el encoder y luego desaparecer 
  if (adjustingFx && millis() - lastMenuItemChange < 2000) {
        bool redrawDial = true;
        double currentValue = 0.0;
        String label = "";

        switch (activeFx) {
            case 0:  // Volume
                currentValue = masterVolume * 100;
                label = "Volume";
                break;
            case 1:  // High-Pass
                currentValue = hpFilterStep*10;
                label = "HPF";
                break;
            case 2:  // GranularFx
                currentValue = granularFxWetDry;
                label = "Granular";
                break;
        }

        // visualizar en el centro del display
        int centerX = 64;
        int centerY = 32;
        int radius = 28;

        // dibujar dial
        DrawDial(display, currentValue, centerX, centerY, radius, 0, 100, label, redrawDial);
    }

    // actualizar display
    display.display();
}

// dibujo del dial para mostrar en pantalla al modificar volumen, efecto o filtro 
void DrawDial(Adafruit_SSD1306 &display, double curval, int cx, int cy, int r, double loval, double hival, String label, bool &Redraw) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(cx - r, cy - r - 8);
    display.println(label);

    // Adjusted number of segments for clarity
    int numSegments = 12;
    double angleStep = 270.0 / numSegments; // Total sweep angle of 270 degrees
    double needleAngle = map(curval, loval, hival, 0, 270); // Map the value to an angle

    // Starting angle offset by an additional -90 degrees to rotate start position
    double startAngle = -135 - 90;

    // Redraw control
    if (Redraw) {
        display.fillRect(0, 0, display.width(), cy + r + 2, SSD1306_BLACK); // Clear only the necessary area
        Redraw = false;
    }

    // Draw the scale marks
    for (int i = 0; i <= numSegments; i++) {
        double angle = i * angleStep + startAngle;
        double radAngle = angle * DEG_TO_RAD;
        int x0 = cx + (r * cos(radAngle));
        int y0 = cy + (r * sin(radAngle));
        int x1 = cx + ((r - 4) * cos(radAngle));
        int y1 = cy + ((r - 4) * sin(radAngle));
        display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
    }

    // Draw the needle
    double needleRad = (needleAngle + startAngle) * DEG_TO_RAD;
    int needleX = cx + ((r - 5) * cos(needleRad));
    int needleY = cy + ((r - 5) * sin(needleRad));
    display.drawLine(cx, cy, needleX, needleY, SSD1306_WHITE); // Draw the needle

    // Draw the dial center
    display.fillCircle(cx, cy, 2, SSD1306_WHITE);

    // Update the display
    display.display();
}


//------------------------------------------------------- LOOP --------------------------------------------
void loop() {
    handleBpmAdjustment();
    toggleProgramMode();
    toggleStartStop();
    updateSequencer();
    handleRepeat();
    pollTrellis();
    toggleFxAdjustmentMode();
    handleEffectsEncoder();
    updateDisplay(BPM, masterVolume, isPlaying, programMode, repeatMode, adjustingFx);
}
