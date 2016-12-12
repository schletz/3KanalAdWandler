/* 
****************************************************************************************************
EINSTELLEN DES OSCCAL REGISTERS

Autor: Michael Schletz, 21. November 2016
Desc:  Timer 0 invertiert alle 50 Zyklen den Pin PB0. Daraus ergibt sich ein Faktor von 100 
       Takte pro Herz. 
       Kalibrierung: Oszilloskop in den Countmodus (Function Button) schalten und die Frequenz von
       PB0 ablesen. Die angezeigte Freqzenz in kHz : 10 ergibt die Taktfrequenz in MHz. Für höhere 
       Freqzenzen kann der PLL Takt verwendet werden (16 MHz). Am Oszi kann auch die Trendfunktion 
       verwendet werden, da die Freqzenz schwankt 
       (Messbeispiel: MIN 14.63, AVG 14.76, MAX 14.88 MHz, also +/- 0.8 %).
       
       Das EEPROM kann maximal mit 8.8 MHz arbeiten, deshalb sollte der Takt nicht allzu hoch
       getrieben werden (möglich sind 4 - 16 MHz beim RC Oszillator)

       Eine Einheit entspricht rd. 0.05 MHz beim 8 MHz RC Oszillator oder 0.1 MHz beim PLL Clock.

Pinout:
                +------+
          RESET |1    8| VCC
     NC --> PB3 |2    7| PB2 --> NC
     NC --> PB4 |3    6| PB1 <-- NC
            GND |4    5| PB0 --> Oszi
                +------+

****************************************************************************************************
*/

#include <avr/io.h>


static inline void calibrateOscillator(void)
{
  OSCCAL = 70;

	DDRB = (1 << PB0);     // OC0A Pin (PB0) als Outputpin

	OCR0A = 50-1;                  // Zählen bis 49. Das sind 50 Zyklen. Da 1 Hz 2 Zyklen braucht,
                                 // können wir am Oszi 1/100 des CPU Taktes als Frequenz ablesen.
	TCCR0A = (0b10 << WGM00) |     // Timer Reset beim Compare Match.
           (0b01 << COM0A0);     // OC0A Pin beim Compare Match umschalten
  TCCR0B = (0b001 << CS00);      // Prescale 1

  while(1);
}

