#include <Arduino.h>
#include <SPI.h>
#include "MFRC522.h"
#include "EEPROMex.h"
#include "RtcDS1302.h"

#define KEY1 "96:B2:F5:5F"
#define KEY2 "1A:DB:B8:89"

#define SS_PIN 10 //PINO SDA
#define RST_PIN 9 //PINO DE RESET
#define LED_PIN 8

#define RTC_MEM_ADDR 1

ThreeWire rtcwire(7, A6, A2);
RtcDS1302<ThreeWire> rtc(rtcwire);
MFRC522 rfid(SS_PIN, RST_PIN); //PASSAGEM DE PARÂMETROS REFERENTE AOS PINOS

short booleansAddr = 0;

uint8_t currentCounter = 0;
unsigned long counterDelay = 0;

unsigned short lockedBit = 1;
bool locked = false;
unsigned int timeToLock = 52;

unsigned short timeoutBit = 0;
bool timeout;
unsigned int defaultTimeout = 30;

bool readSuccessfully = false;

void setCounterVal(uint8_t counter) {
    rtc.SetMemory(RTC_MEM_ADDR, counter);
}

void readCounterVal() {
    currentCounter = rtc.GetMemory(RTC_MEM_ADDR);
}

bool isCounting() {
    return currentCounter > 0;
}

void setLocked(bool val) {
    if (locked != val) {
        EEPROM.updateBit(booleansAddr, lockedBit, val);
    }
    locked = val;
}

bool readLocked() {
    return EEPROM.readBit(booleansAddr, lockedBit);
}

void setHasTimeout(bool hasTimeout) {
    if (timeout != hasTimeout) {
        EEPROM.updateBit(booleansAddr, timeoutBit, !hasTimeout);
    }
    timeout = hasTimeout;
}

bool readTimeout() {
    return !EEPROM.readBit(booleansAddr, timeoutBit);
}

void toggle(bool open) {
    if (open) {
        analogWrite(3, 255);
        digitalWrite(LED_PIN, HIGH);
    } else {
        analogWrite(3, 0);
        digitalWrite(LED_PIN, LOW);
    }
}

void startRtc() {
    rtc.Begin();
    RtcDateTime compiled = RtcDateTime("Jan 01 2020", "12:00:00");

    if (rtc.GetIsWriteProtected())
    {
        Serial.println("RTC was write protected, enabling writing now");
        rtc.SetIsWriteProtected(false);
    }

    if (!rtc.IsDateTimeValid())
    {
        Serial.println("RTC lost confidence in the DateTime!");
        rtc.SetDateTime(compiled);
        setCounterVal(0);
    }

    if (!rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        rtc.SetIsRunning(true);
    }

    RtcDateTime now = rtc.GetDateTime();
    if (now < compiled)
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        rtc.SetDateTime(compiled);
    }

    readCounterVal();
}

void setup() {
    pinMode(LED_PIN, OUTPUT);

    startRtc();

    SPI.begin(); //INICIALIZA O BARRAMENTO SPI
    rfid.PCD_Init(); //INICIALIZA MFRC522
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);

    timeout = readTimeout();
    locked = readLocked();

    toggle(!locked);

    counterDelay = millis();
    if (!locked && !isCounting()) {
        setCounterVal(1);
        currentCounter = 1;
    }
}

void loop() {
    if (!readSuccessfully && currentCounter > timeToLock) {
        toggle(false);
        setLocked(true);
    }

    if (!readSuccessfully && timeout) {
        delay(defaultTimeout * 1000);
        setHasTimeout(false);
    }

    if (!readSuccessfully && millis() - counterDelay > 1000) {
        currentCounter++;
        setCounterVal(currentCounter);
        counterDelay = millis();
    }

    if (!readSuccessfully && (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())) //VERIFICA SE O CARTÃO PRESENTE NO LEITOR É DIFERENTE DO ÚLTIMO CARTÃO LIDO. CASO NÃO SEJA, FAZ
        return; //RETORNA PARA LER NOVAMENTE

    /***INICIO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/
    String strID = "";
    for (byte i = 0; i < 4; i++) {
        strID +=
                (rfid.uid.uidByte[i] < 0x10 ? "0" : "") +
                String(rfid.uid.uidByte[i], HEX) +
                (i!=3 ? ":" : "");
    }
    strID.toUpperCase();
    /***FIM DO BLOCO DE CÓDIGO RESPONSÁVEL POR GERAR A TAG RFID LIDA***/

    if (strID == F(KEY1) || strID == F(KEY2)) {
        toggle(true);
        readSuccessfully = true;
        setLocked(false);
    } else {
        toggle(false);
        setHasTimeout(true);
    }

    setCounterVal(0);
    currentCounter = 0;

    rfid.PICC_HaltA(); //PARADA DA LEITURA DO CARTÃO
    rfid.PCD_StopCrypto1(); //PARADA DA CRIPTOGRAFIA NO PCD
}