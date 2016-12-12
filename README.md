# Crashwagerl
<pre>
3 Kanal 10bit A/D Wandler mit Samplingintervall von 1 ms und 460 800 bit/s UART.
Liest jede Millisekunde den Analogwert der Pins PC2, PB3 und PB4 und sendet ihn mit einem 
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
</pre>
