/* 
*************************************************************************************************
CRASHWAGERL: 3 Kanal 10bit A/D Wandler mit Samplingintervall von 1 ms und 460 800 bit/s UART.

Autor: Michael Schletz, 21. November 2016
Desc:  Liest jede Millisekunde den Analogwert der Pins PC2, PB3 und PB4 und sendet ihn mit einem 
       ms Timecode über den UART Pin. Am Anfang wird der Wert der internen 1.1 V Referenzspannung
       in Bezug zur Betriebsspannung gemessen. So kann der absolute Spannungswert berechnet werden.
       
       Wichtig für das Programmieren von neuen Chips: Beim Attiny muss, um einen 16 MHz Takt zu 
       erhalten, die Fuse CLCK DIV8 deaktiviert werden. Danach muss der Wert von OSCILLATOR_CAL 
       so gesetzt werden, dass die CPU mit einem Takt von 7.3728 MH arbeitet. So kann mit 
       460 800 bit/s übertragen werden.

       Die UART Nachricht ist ein ASCII String, der mit \r\n beendet wird. Die Werte für den 
       Timecode und den ADC Messwert werden BASE64 codiert. Dabei werden die ersten 4 Stellen
       für den Timecode, danach 2 Stellen für den Wert der 1.1V Spannung in Bezug auf Vcc und danach 
       jeweils 2 Stellen pro Kanal für den Messwert verwendet. Eine Nachricht hat daher
       immer 12 Zeichen (ohne CR LF) und besteht aus den Zeichen A-Z, a-z, 0-9, + und /
                                ABDE | ab | AB | ab | AB
                                Time | Ref| Ch1| Ch2| Ch3

       Die Decodierung kann so erfolgen:
         TIMECODE = 0
         SOLANGE BIS 4 STELLEN GELESEN
            NÄCHSTES ZEICHEN LESEN
            TIMECODE = 64 * TIMECODE + DEZIMALWERT DES ZEICHENS
       Eine C Musterimplementierung ist in der Funktion base64Decode in der Headerdatei.

       Die Dezimalwerte der Zeichen können auf [https://de.wikipedia.org/wiki/Base64] unter
       Base64-Zeichensatz nachgelesen werden. Folgende Beispiele zeichen die decodierten Werte:


Pinout:
                            +------+
                      RESET |1    8| VCC
Analog in Channel 2 --> PB3 |2    7| PB2 <-- Analog in Channel 1
Analog in Channel 3 --> PB4 |3    6| PB1 --> UART TX
                        GND |4    5| PB0 --> Ms Takt (Toggle bei Compare Match)
                            +------+

*************************************************************************************************
*/
#include "Crashwagerl.h"

/**
* (Leere) ISR für den ADC Complete Interrupt. Muss vorhanden sein, da vor der AD Konvertierung
* der Sleep Mode für die ADC Noise Reduktion aufgerufen wird. Nicht löschen, sonst wird beim
* ersten Aufrufen des Interrupts das Programm beendet, da eine ungültige Sprungadresse in der ISR
* Tabelle ist!
* @param ADC_vect: Interruptvektor aus interrupt.h
* @return void
*/
ISR(ADC_vect)
{

}

int main(void)
{
  char message[15] = "            \r\n";
  uint8_t count;
  uint16_t adcValues[4];
  uint32_t msCounter = 0;

  // Internen Oszillator konfigurieren, damit die Zielfrequenz erreicht wird.
  OSCCAL = OSCILLATOR_CAL;         

  DDRB = (1 << MILLISEC_PIN);   // MILLISEC_PIN als Output definieren.
  
  initAdc();   // AD Einstellungen initialisieren.
  initUart();  // Timer und PIN für das USI Interface initialisieren.
  sei();       // Interrupts aktivieren.

  // Den Wert der internen 1.1 V Referenzspannung lesen. Damit kann auf die Referenzspannung für die
  // AD Messung (Vcc) geschlossen werden. Nach dem Wechsel zur internen Referenzspannung braucht der
  // ADC allerdings lt. Datenblatt 1ms. Deshalb kann nur am Anfang der Wert gemessen werden.
  DELAY_US(5000);                           // 5 ms warten bis sich die Betriebsspannung 
                                            // stabilisiert hat.
  adcValues[0] = readAdcValue(ADC_1V1REF);
  for(count=63; count; count--)
  {
    adcValues[0] += readAdcValue(ADC_SAME_CHANNEL);
  }
  adcValues[0] /= 64;

  while (1) 
  {
    PINB |= (1 << MILLISEC_PIN);         // Toggle Pin, also den ms Takt ausgeben.

    adcValues[1] = readAdcValue(ADC_PB2);
    adcValues[2] = readAdcValue(ADC_PB3);
    adcValues[3] = readAdcValue(ADC_PB4);

    // Die unteren 24 Bit als MS Wert als Base64 ASCII senden. Das sind Base64 Codiert 4 Zeichen.
    // Der Timer läuft also nach 4h 40m über.
    uint32ToBase64(msCounter, message, 4);
    // Die unteren 12 Bit des ADC Wertes als Base64 senden. Das sind 2 Zeichen.
    uint16ToBase64(adcValues[0], message+4, 2);   // Wert der internen 1.1V Referenzspannung
    uint16ToBase64(adcValues[1], message+6, 2);   // Wert von PB2
    uint16ToBase64(adcValues[2], message+8, 2);   // Wert von PB3
    uint16ToBase64(adcValues[3], message+10, 2);  // Wert von PB4
    uartSendMessage(message);

    msCounter++;

    // Hier wird gewartet, bis 1 ms komplett ist. Da die CPU während der Messung in den Sleepmode
    // geht, und das Programm sowieso alles synchron abarbeitet, ist diese Lösung genauer als ein
    // Timer. Der Compare Match des Timers kann nur von 0-255 eingestellt werden und ist daher nicht
    // so genau.
    // Jeder Schleifendurchlauf muss allerdings die gleiche Anzahl von Zyklen benötigen. Das ist bei
    // if Verzweigungen u. U. kritisch. Außerdem muss bei einer Programmänderung der Wert für
    // WAIT_US_PER_LOOP neu eingemessen werden.
    DELAY_US(WAIT_US_PER_LOOP);

  }  // while (1) 
}

