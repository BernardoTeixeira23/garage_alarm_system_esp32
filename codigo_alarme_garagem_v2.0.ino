/*Código para o Projeto: Sistema de Segurança de Garagem com Arduino V2.0
  - Material necessário:
    -> LilyGo TTGO SIM800L (ESP32)
    -> 2 Breadboards
    -> Teclado Maticial 4x3
    -> Display LCD 20x4 I2C
    -> Sensor de Abertura de Porta
    -> Resistência de 1K (para estabilidade do sensor de abertura de porta)
    -> Buzzer Passivo
    -> Fios de Ligação Macho-Macho
    -> Fios de Ligação Macho-Fêmea
  Todo o material pode ser adquirido a partir do artigo do nosso blog deste projeto!
  *****************************************************ATENÇÃO******************************************************
  O CÓDIGO INICIAL É 000000. PARA ALTERAR, BASTA MANTER PREMIDO A TECLA '*' APÓS REINICIAR A PLACA MICROCONTROLADORA
  *****************************************************ATENÇÃO******************************************************
  - Para navegar na interface, basta utilizar o teclado matricial do projeto:
    -> Tecla '*': Menu de arme/desarme do alarme
    -> Tecla '#': Menu de configuração de tempo de abertura da porta da garagem
  - Nos menus, para confirmar as escolhas, pressione a tecla '#'
  - Conectar um dispositivo de saída ao pino 15 (Buzzer) para resposta sonora à atuação do alarme
  - Inserir um cartão SIM válido com o código PIN DESATIVADO
  - Preencher a linha 17 com o número de telemóvel a qual pretende que a mensagem seja enviada
  - Preencher a linha 18 com a mensagem que deseja ser enviada quando o alarme dispara
  www.electrofun.pt/blog/alarme-de-garagem-com-envio-de-mensagem
  Electrofun@2022 ---> www.electrofun.pt
*/

#define SMS_TARGET  "+3519********"
#define MSN_TO_SEND "GARAGEM FOI ABERTA!"

#define pinoBuzzer 15
#define pinoSensorPorta 34
#define SIM800L_AXP192_VERSION_20200327
#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_RX_BUFFER 1024
#define SerialMon Serial
#define SerialAT  Serial1

//Inclusão de Bibliotecas
#include <LiquidCrystal_I2C.h> //Biblioteca do Display I2C
#include <Keypad.h> //Biblioteca do Teclado Matricial (4x4)
#include <EEPROM.h> //Biblioteca da memória interna do nosso ESP32
#include <TinyGsmClient.h>
#include "utilities.h"

//Declaração de variáveis
const byte linhasTeclado = 4;
const byte colunasTeclado = 3;
byte pinosLinhasTeclado[linhasTeclado] = {0, 2, 14, 18};  //Conexões do teclado matricial
byte pinosColunasTeclado[colunasTeclado] = {19, 13, 12};  //Conexões do teclado matricial
byte tamanhoCodigo = 6;
int contagemDec, stopVar;
int debouce = 200; //Esta variável é utilizada apenas para conhecimento do utilizador do motivo da função delay() no pedaço de código
int tempoAberturaPorta = 10;
long tempo;
bool estadoSensorPorta, portaAberta;
bool alarmeAtivado = false;
bool alarmeAcionado = false;
char teclaPressionada;
char codigoCorreto[6];
char codigoVerificar[6];
char hexaKeys[linhasTeclado][colunasTeclado] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

//Caracteres Necessários em BitMap
byte setaDireita[] = {
  B10000,
  B11000,
  B11100,
  B11111,
  B11111,
  B11110,
  B11000,
  B10000
};

byte setaEsquerda[] = {
  B00001,
  B00011,
  B00111,
  B11111,
  B11111,
  B01111,
  B00011,
  B00001
};

LiquidCrystal_I2C lcd(0x27, 20, 4);
Keypad teclado = Keypad( makeKeymap(hexaKeys), pinosLinhasTeclado, pinosColunasTeclado, linhasTeclado, colunasTeclado);
TinyGsm modem(SerialAT);

TaskHandle_t Task1;

void setup() {
  SerialMon.begin(115200);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  pinMode(pinoSensorPorta, INPUT_PULLUP);
  pinMode(pinoBuzzer, OUTPUT);
  setupModem();
  //Selecionar a correta função para inicializar o LCD (varia de acordo com a versão da livraria LiquidCrystal_I2C)
  //lcd.begin();
  lcd.init();
  lcd.backlight();
  EEPROM.begin(tamanhoCodigo);
  while (!EEPROM.begin(tamanhoCodigo)) {
    lcd.print("EEPROM FAILED!");
    delay(5000);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema de Seguranca");
  lcd.setCursor(4, 1);
  lcd.print("com Arduino");
  lcd.setCursor(1, 3);
  lcd.print("By Electrofun@2022");
  delay(3000);
  lcd.clear();
  lcd.setCursor(7, 0);
  lcd.print("Versao");
  lcd.setCursor(8, 1);
  lcd.print("2.0");
  lcd.setCursor(1, 3);
  lcd.print("By Electrofun@2022");
  delay(5000);
  if (teclado.getKey() == '*') {
    alterarPalavraChave();
  }
  lcd.clear();
  xTaskCreatePinnedToCore(funcaoAlarmeAcionado, "Task1", 10000, NULL, 1, &Task1, 0);
  delay(500);
}

void funcaoAlarmeAcionado( void * pvParameters ) {
  for (;;) {
    if (alarmeAcionado == 1) {
      if (stopVar == 0) {
        tempo = millis();
        stopVar++;
      }
      if (millis() - tempo > (tempoAberturaPorta * 1000)) {
        if (stopVar == 1) {
          modem.init();
          modem.sendSMS(SMS_TARGET, String(MSN_TO_SEND));
          modem.maintain();
          stopVar++;
        }
        for (int i = 0; i < 10; i++)
        {
          digitalWrite(pinoBuzzer, HIGH);
          delay(1);
          digitalWrite(pinoBuzzer, LOW);
          delay(1);
        }
        for (int i = 0; i < 20; i++)
        {
          digitalWrite(pinoBuzzer, HIGH);
          delay(2);
          digitalWrite(pinoBuzzer, LOW);
          delay(2);
        }
      } else {
        digitalWrite(pinoBuzzer, LOW);
      }
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void loop() {
  lcd.setCursor(7, 0);
  lcd.print("STATUS");
  lcd.setCursor(0, 2);
  lcd.print("Porta Garagem");
  estadoSensorPorta = digitalRead(pinoSensorPorta);
  if (estadoSensorPorta == 0) {
    portaAberta = 1;
    lcd.setCursor(0, 3);
    lcd.print("Aberta ");
  } else if (estadoSensorPorta == 1) {
    portaAberta = 0;
    lcd.setCursor(0, 3);
    lcd.print("Fechada");
  }
  if (alarmeAtivado == 1 && portaAberta == 1) {
    alarmeAcionado = 1;
  }
  if (alarmeAtivado == 0) {
    alarmeAcionado = 0;
  }
  teclaPressionada = teclado.getKey();
  if (teclaPressionada == '*') {
    delay(debouce);
    menu1();
  } else if (teclaPressionada == '#') {
    delay(debouce);
    menu2();
  }
}

void menu1() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("1 - Alarme");
  if (alarmeAtivado == 1) {
    lcd.setCursor(7, 2);
    lcd.print("Armado");
  } else if (alarmeAtivado == 0) {
    lcd.setCursor(5, 2);
    lcd.print("Desarmado");
  }
  lcd.createChar(1, setaEsquerda);
  lcd.setCursor(0, 3);
  lcd.write(1);
  lcd.createChar(2, setaDireita);
  lcd.setCursor(19, 3);
  lcd.write(2);
  teclaPressionada = teclado.waitForKey();
  lcd.clear();
  if (teclaPressionada == '#') {
    lcd.setCursor(0, 0);
    lcd.print("Codigo Seguranca:");
    lcd.setCursor(0, 3);
    for (int i = 0; i <  tamanhoCodigo; i++) {
      codigoVerificar[i] = teclado.waitForKey();
      lcd.print(codigoVerificar[i]);
    }
    lcd.clear();
    if (codigoVerificar[0] == EEPROM.read(0) &&
        codigoVerificar[1] == EEPROM.read(1) &&
        codigoVerificar[2] == EEPROM.read(2) &&
        codigoVerificar[3] == EEPROM.read(3) &&
        codigoVerificar[4] == EEPROM.read(4) &&
        codigoVerificar[5] == EEPROM.read(5)) {
      alarmeAtivado = !alarmeAtivado;
      if (alarmeAtivado == true) {
        stopVar = 0;
        contagemDec = tempoAberturaPorta;
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Alarme armado em:");
        do {
          if (contagemDec < 10) {
            lcd.setCursor(9, 2);
            lcd.print("0");
            lcd.setCursor(10, 2);
            lcd.print(contagemDec);
          } else {
            lcd.setCursor(9, 2);
            lcd.print(contagemDec);
          }
          contagemDec--;
          delay(1000);
        } while (contagemDec != 0);
      }
      lcd.clear();
      return;
    }
  } else if (teclaPressionada == '*') {
    delay(debouce);
    return;
  }
}

void menu2() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("2 - Tempo");
  lcd.setCursor(3, 1);
  lcd.print("Abertura Porta");
  lcd.createChar(1, setaEsquerda);
  lcd.setCursor(0, 3);
  lcd.write(1);
  lcd.createChar(2, setaDireita);
  lcd.setCursor(19, 3);
  lcd.write(2);
  teclaPressionada = teclado.waitForKey();
  lcd.clear();
  if (teclaPressionada == '#') {
    delay(debouce);
    definirTempoAberturaPorta();
    return;
  } else if (teclaPressionada == '*') {
    delay(debouce);
    return;
  }
}

void definirTempoAberturaPorta() {
  lcd.clear();
  do {
    lcd.setCursor(0, 0);
    lcd.print("Defina o tempo de");
    lcd.setCursor(0, 1);
    lcd.print("abertura da porta:");
    if (tempoAberturaPorta < 10) {
      lcd.setCursor(9, 3);
      lcd.print("0");
      lcd.setCursor(10, 3);
      lcd.print(tempoAberturaPorta);
    } else {
      lcd.setCursor(9, 3);
      lcd.print(tempoAberturaPorta);
    }
    teclaPressionada = teclado.waitForKey();
    if (teclaPressionada == '6') {
      delay(debouce);
      tempoAberturaPorta++;
      if (tempoAberturaPorta > 100) {
        tempoAberturaPorta = 0;
      }
    } else if (teclaPressionada == '4') {
      delay(debouce);
      tempoAberturaPorta--;
      if (tempoAberturaPorta < 0) {
        tempoAberturaPorta = 99;
      }
    }
  } while (teclaPressionada != '#');
  delay(debouce);
  lcd.clear();
}

void alterarPalavraChave() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Modo Programador");
  lcd.setCursor(0, 1);
  lcd.print("Ativado!");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Codigo Seguranca:");
  lcd.setCursor(0, 3);
  for (int i = 0; i <  tamanhoCodigo; i++) {
    codigoVerificar[i] = teclado.getKey();
    lcd.print(codigoVerificar[i]);
  }
  lcd.clear();
  if (codigoVerificar[0] == EEPROM.read(0) &&
      codigoVerificar[1] == EEPROM.read(1) &&
      codigoVerificar[2] == EEPROM.read(2) &&
      codigoVerificar[3] == EEPROM.read(3) &&
      codigoVerificar[4] == EEPROM.read(4) &&
      codigoVerificar[5] == EEPROM.read(5)) {
    lcd.setCursor(0, 0);
    lcd.print("Insira novo codigo:");
    lcd.setCursor(0, 3);
    for (int i = 0; i < tamanhoCodigo; i++) {
      codigoCorreto[i] = teclado.getKey();
      EEPROM.write(i, codigoCorreto[i]);
      lcd.print(codigoCorreto[i]);
    }
    EEPROM.commit();
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Codigo Errado");
    lcd.setCursor(0, 2);
    lcd.print("Tente Novamente!");
    delay(5000);
  }
  lcd.clear();
}
