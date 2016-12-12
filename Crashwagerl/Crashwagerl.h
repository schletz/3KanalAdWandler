/*
*************************************************************************************************
CRASHWAGERL.H

Autor: Michael Schletz, 22. November 2016
Desc:  Headerdatei mit den Definitionen der Pins und inline Hilfsfunktionen für das Lesen des AD
       Wertes und die base64 Codierung.
*************************************************************************************************
*/

#ifndef CRASHWAGERL_H_
#define CRASHWAGERL_H_

/* KONFIGURATION ******************************************************************************** */
#define OSCILLATOR_CAL    88   // Wert für die Oszillatorkalibrierung für 7.3728 MHz
#define MILLISEC_PIN      PB0  // Pin, wo der ms Takt ausgegeben wird.
#define UART_TIMER_CYCLES 16   // 460 800 Baud bei 7.3728 MHz Takt (Prescale 1)
#define WAIT_US_PER_LOOP  123  // Anzahl der Mikrosekunden, die nach der Übertragung und vor der
                               // nächsten Messung gewartet wird. Muss mit dem Oszi und dem 
                               // MILLISEC_PIN ermittelt werden.

#define F_CPU 7372800ul        // Fuse CKDIV8 deaktiviert werden!
/* ********************************************************************************************** */

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "UartLibrary.h"

typedef enum {STATE_IDLE, STATE_MS_COMPLETE} STATES;
typedef enum {ADC_PB5, ADC_PB2, ADC_PB4, ADC_PB3, ADC_NA1, ADC_NA2, ADC_NA3, ADC_NA4, 
              ADC_NA5, ADC_NA6, ADC_NA7, ADC_NA8, ADC_1V1REF, ADC_SAME_CHANNEL } ADC_CHANNELS;

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Wartet präzise die angegebene Anzahl von Mikrosekunden. Der maximale Wert beträgt bei 16 MHz
* 17 179, damit die Berechnung nicht überläuft. Der minimale Wert darf nicht unter 16 Zyklen 
* entsprechen, also 16 us bei 1 MHz oder 1 us bei 16 MHz.
* Erklärung der Berechnung: / 4 weil pro Schleife 4 Zyklen verwendet werden.
*                           - 3 weil der Overhead 12 Zyklen, also 3 Durchläufen entspricht
*                           + 500000 damit nicht abgeschnitten, sondern gerundet wird
* Das ergibt (F_CPU / 4 * val + 500000) / 1000000 - 3
* Damit die Berechnung nicht überläuft, wurde Zähler und Nenner noch durch 16 gekürzt.
* @param val: Ein Wert von 16 / CPU Clock (in MHz) bis zu 1073 us, der gewartet werden soll.
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
#define DELAY_US(val) __asm__ volatile (                                                           \
  "push R24 \n\t"                         /* Register am Stack sichern */                          \
  "push R25 \n\t"                         /* (2 Cycles) */                                         \
  "ldi R24,%[counterLo] \n\t"                                                                      \
  "ldi R25,%[counterHi] \n\t"                                                                      \
  "1: sbiw R24,1" "\n\t"     /* Subtract Immediate from Word, 2 Cycles  */                         \
  "brne 1b \n\t"             /* Branch if Not Equal, 2 Cycles wenn true */                         \
  "nop \n\t"                 /* Um den fehlenden Sprung auszugleichen (brne hat nur 1 Zyklus */    \
                             /* für false, somit braucht jeder Durchlauf 4 Zyklen) */              \
  "pop R25 \n\t"                                                                                   \
  "pop R24 \n\t"                                                                                   \
  "nop \n\t"                 /* Damit 12 Zyklen (durch 4 teilbar) overload entstehen */            \
  "nop \n\t"                 /* (das entspricht 3 Durchlaeufen */                                  \
  :                                                                                                \
  : [counterHi] "M" (((F_CPU / 64ul * val + 31250ul) / 62500ul - 3ul) >> 8),                      \
    [counterLo] "M" (((F_CPU / 64ul * val + 31250ul) / 62500ul - 3ul) & 0xFF)                     \
)

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Initialisiert den AD Converter für eine single-ended Messung. Der Prescaler stellt einen Wert für
* den ADC Clock auf einen Wert von 50 - 200 kHz ein. Der AD Conversion Comlete Interrupt wird 
* aktiviert, damit im Sleep Modus SLEEP_MODE_ADC gemessen werden kann.
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void initAdc()
{
  ADCSRA = (1 << ADEN) |      // AD Converter aktivieren.
           (0b110 << ADPS0) | // Prescaler 64, ergibt 115.2 kHz für den AD Converter (s. Doku, S125)
           (1 << ADIE);       // AD Conversion Complete Interrupt aktivieren.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Liest vom AD Converter. Dabei wird die CPU in den ADC Noise Reduction Sleep Mode geschickt. 
* Der AD Complete Interrupt weckt sie wieder auf und es kann das Ergebnis aus dem ADC Register
* gelesen werden. Dieser Vorgang wird mehrmals (je nach dem Wert von NUMBER_OF_MEASUREMENTS)
* durchgeführt, um das Ergebnis zu mitteln.
* Eine Wandlung benötigt 13 Zyklen, wobei 1 Zyklus = CPU-Takt / Prescaler, also rd. 115 kHz ist.
* @return Gelesener AD Wert (0 - 1023)
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline uint16_t readAdcValue(ADC_CHANNELS channel)
{
  if (channel != ADC_SAME_CHANNEL)
  {
    ADMUX = (0b000 << REFS0) | (channel << MUX0); // Vom angegebenen Eingang mit Vcc als 
                                                  // Referenzspannung lesen.
  }
  if (channel == ADC_1V1REF)
  {
    DELAY_US(5000);               // 5 ms warten (1ms Mindeszeit lt. Datenblatt).
  }
  set_sleep_mode(SLEEP_MODE_ADC); // ADC Noise Reduction Sleep Mode einstellen.
  sleep_mode();                   // Sleepmode starten. 
  while (ADCSRA & (1 << ADSC));   // Warten, solange der AD Wandler noch arbeitet. Das 
                                  // sollte jedoch nicht auftreten. Es kommt nur vor, 
                                  // wenn ein anderer Interrupt die CPU vor dem Ende der 
                                  // Konvertierung aufweckt.
  return ADC;                    // Ergebnis auslesen und zurückliefern.
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Codiert eine übergebene 32bit Variable als ASCII base64 String. Dies Codierung ist allerdings
* keine standardgemäße base64 Codierung, da hier nicht auf 3 Bytes aufgefüllt wird. Es wird im
* Prinzip ins 64er System konvertiert und die Zeichentabelle der base64 Codierung verwendet.
* @param val: Der zu codierende binäre Wert.
* @param bufferPtr: Pointer auf den Puffer, der den String speichern soll.
* @param len: Länge des base64 codierten Strings. Ist die Länge kleiner als ein 32bit Wert haben
*             kann (6 Zeichen), werden die niederwertigsten Bits genommen. Es können pro Zeichen
*             6 Bits codiert werden (2^6 = 64)
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void uint32ToBase64(uint32_t val, char *bufferPtr, uint8_t len)
{
  uint8_t byteToEncode;

  bufferPtr += len;
  while (len--)
  {
    --bufferPtr;
    byteToEncode = val & 0b111111;
    val >>= 6;
    if (byteToEncode < 26)       *bufferPtr = 'A'+byteToEncode;
    else if (byteToEncode < 52)  *bufferPtr = 'a'+(byteToEncode-26);
    else if (byteToEncode < 62)  *bufferPtr = '0'+(byteToEncode-52);
    else if (byteToEncode == 62) *bufferPtr = '+';
    else if (byteToEncode == 63) *bufferPtr = '/';
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Codiert eine übergebene 16bit Variable als ASCII base64 String. Dies Codierung ist allerdings
* keine standardgemäße base64 Codierung, da hier nicht auf 3 Bytes aufgefüllt wird. Es wird im
* Prinzip ins 64er System konvertiert und die Zeichentabelle der base64 Codierung verwendet.
* @param val: Der zu codierende binäre Wert.
* @param bufferPtr: Pointer auf den Puffer, der den String speichern soll.
* @param len: Länge des base64 codierten Strings. Ist die Länge kleiner als ein 16bit Wert haben
*             kann (3 Zeichen), werden die niederwertigsten Bits genommen. Es können pro Zeichen
*             6 Bits codiert werden (2^6 = 64)
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void uint16ToBase64(uint16_t val, char *buffer, uint8_t len)
{
  uint8_t byteToEncode;

  buffer += len;
  while (len--)
  {
    --buffer;
    byteToEncode = val & 0b111111;
    val >>= 6;
    if (byteToEncode < 26)       *buffer = 'A'+byteToEncode;
    else if (byteToEncode < 52)  *buffer = 'a'+(byteToEncode-26);
    else if (byteToEncode < 62)  *buffer = '0'+(byteToEncode-52);
    else if (byteToEncode == 62) *buffer = '+';
    else if (byteToEncode == 63) *buffer = '/';
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Decodiert einen übergebenen base64 String in eine 32bit Variable. Dieser
* String darf maximal 5 Stellen lang sein, da sonst der 32bit Wert nicht
* ausreicht und überläuft.
* Wird im Programm nicht verwendet, ist für die Programmierung eines Empfängers allerdings
* hilfreich, da die Codierung keine "normale" base64 Codierung ist. Dafür müsste auf 3 Bytes
* stets aufgefüllt werden, was hier nicht der Fall ist.
* @param bufferPtr: Pointer auf den Puffer, der den String beinhaltet.
* @param len: Anzahl der Zeichen, die aus dem buffer gelesen werden sollen.
* @return Decodierter 32bit unsigned Wert oder -1, wenn ungültige Zeichen enthalten sind.
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline uint32_t base64Decode(const char * bufferPtr, uint8_t len)
{
  uint32_t val = 0;
  while (len--)
  {
    if      (*bufferPtr >= 'A' && *bufferPtr <= 'Z') val = val*64 + (*bufferPtr - 'A');
    else if (*bufferPtr >= 'a' && *bufferPtr <= 'z') val = val*64 + (*bufferPtr - 'a' + 26);
    else if (*bufferPtr >= '0' && *bufferPtr <= '9') val = val*64 + (*bufferPtr - '0' + 52); 
    else if (*bufferPtr == '+')                      val = val*64 + 62;
    else if (*bufferPtr == '/')                      val = val*64 + 63;
    else return -1;
    bufferPtr++;
  }
  return val;
}

#endif /* CRASHWAGERL_H_ */