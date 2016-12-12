/*
****************************************************************************************************
UARTLIBRARY.H

Autor: Michael Schletz, 21. November 2016
Desc:  Headerdatei mit den inline Funktionen zum Senden von Nachrichten über UART. Die 
       Implementierung erfolgt über das USI Register.
****************************************************************************************************
*/

#ifndef UARTLIBRARY_H_
#define UARTLIBRARY_H_

#include <avr/io.h>
#include <util/atomic.h>

#ifndef UART_TIMER_CYCLES
#error "UART_TIMER_CYCLES ist nicht definiert"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Dreht alle Bits eines Bytes in der Reihenfolge um, macht also aus 10001011 -> 11010001. Das ist
* für die Übertragung über das USI Schieberegister nötig.
* @param val: Bytewert
* @return Umgedrehtes Byte
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline uint8_t reverseByte (uint8_t x)
{
  x = ((x & 0x55) << 1) | ((x & 0xaa) >> 1);
  x = ((x & 0x33) << 2) | ((x & 0xcc) >> 2);
  __asm__ volatile ("swap %0" : "=r" (x) : "0" (x)); /* swap nibbles. */
  return x;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Initialisiert den Timer 1 mit dem in UART_TIMER_CYCLES angegebenen Compare Wert.
* Aktiviert den in UART_TX_PIN definierten Pin für Output und setzt ihn auf 1. Weiters werden
* die globalen Interrupts aktiviert.
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void initUart()
{
	DDRB |= (1 << PB1);    // TX Pin hat Output Direction
  PORTB |= (1 << PB1);   // Pin auf HIGH Legen (IDLE Pegel von UART)
  // Timer 0 wird für das USI Interface verwendet. Bei jedem Compare Match wird das Schieberegister
  // um 1 Bit nach links geschoben und das rausgefallene Bit wird auf den Pin gelegt.
  OCR0A = UART_TIMER_CYCLES-1;   // Zählen bis zur definierten Anzahl von Zyklen.
  TCCR0A = (0b10 << WGM00);      // Timer Reset beim Compare Match.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
* Kopiert jedes Bytes des MessagePointers (exkl. \0) in den TxBuffer, damit die ISR das Byte
* senden kann. Diese Funktion arbeitet synchron, d. h. sie blockiert den Programmablauf bis
* alle Bytes gesendet wurden.
* @param messagePtr: Pointer auf die zu sendende Nachricht, muss mit \0 terminiert sein.
* @param uartTxBufferPtr: Pointer auf den Transmitpuffer, der vom Timer abgearbeitet wird.
* @return void
*/
////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void uartSendMessage(const char * messagePtr)
{
  uint8_t reversedByte;

  while ((reversedByte = reverseByte(*messagePtr++)))
  {
    // Damit das erste Bit auch die volle Länge hat, darf der Timerstand nicht undefiniert sein.
    // Der Timer muss allerdings mit 1 initialisiert werden, da das Aktivieren des Timers 1 
    // Instruktion nach dem Aktivieren des USI erfolgt.
    TCNT0 = 1;
    // Das Schieberegister mit folgenden Daten aufbereiten:
    // 0  D0 D1 D2 D3 D4 D5 D6
    // Wir mit dem Startbit (0) und es folgen bei der ersten Übertragung 4 Datenbits.
    USIDR = (reversedByte >> 1);
    // Der Zähler im USISR soll bei 11 beginnen. Er läuft bei 15 über und setzt das USIOIF Flag.
    // Somit werden hier 5 Bits rausgeschrieben.
    USISR = (1 << USIOIF) | 11;
    // USI Clock Source ist der Timer 0, Modus ist 3 Wire Mode.
    USICR = (0b01 << USIWM0) | (0b01 << USICS0);
    // Timer starten, das USI schiebt nun bei jedem Compare Match 1 Bit raus.
    TCCR0B = (0b001 << CS00);        // Prescale 1
    // Warten, bis der USI Counter das Overflow Flag setzt.
    while (!(USISR & (1 << USIOIF)));

    // Nun übertragen wir den 2. Teil des 10 Bit langen UART Frames. Dabei muss sich der Inhalt
    // des USIDR um mindestens 1 Bit überlappen, da Änderungen sofort geschrieben werden.
    // Wir lassen den Timer ab 10 laufen, damit ein 2. Stoppbit übertragen wird. Dies verbessert
    // D4 D5 D6 D7 1 1 1 1
    USIDR = (reversedByte << 4) | 0b1111;
    // USI Overflow Flag löschen
    USISR = (1 << USIOIF) | 10;
    while (!(USISR & (1 << USIOIF)));
    // Timer und USI deaktivieren.
    TCCR0B = 0;
    USICR = 0;
  }
}

#endif /* UARTLIBRARY_H_ */
