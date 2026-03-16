#include <avr/wdt.h>


byte relayState = 0;
byte newState = 0;
byte slaveAddr = 0;
byte RS_Pin = 10;
const unsigned int lostConnectionTime = 5000;
unsigned long resetWDtimer;
byte watchDog = 0;
byte receiveMode = B11111011;


void setup()
{

    wdt_disable();                                                   // Отключение watchDog
    wdt_enable(WDTO_2S);                                             // Запуск watchDog таймера

    if (analogRead(A5) > 500)
        slaveAddr = slaveAddr | B00000001;

    if (analogRead(A4) > 500)
        slaveAddr = slaveAddr | B00000010;

    if (analogRead(A3) > 500)
        slaveAddr = slaveAddr | B00000100;

    if (analogRead(A2) > 500)
        slaveAddr = slaveAddr | B00001000;

    if (analogRead(A1) > 500)
        slaveAddr = slaveAddr | B00010000;


    if (slaveAddr == 0)
        slaveAddr = 1;


    Serial.begin(9600, SERIAL_8N1 );

    for (byte i = 2; i < 10; i++)
        {
            pinMode(i, OUTPUT);
            digitalWrite(i, LOW);
        }



    // put your setup code here, to run once:
    //Serial.begin(9600);
    pinMode(RS_Pin, OUTPUT);
    digitalWrite(RS_Pin, LOW);

    pinMode(A0, OUTPUT);
    digitalWrite(A0, LOW);
    resetWDtimer = millis();

    UCSR0B |= ( 1 << TXCIE0 );     // UART 1 Interrupt Enable

    wdt_reset();   // Сброс WatchDog
    //digitalWrite(A0, HIGH);
    //delay(3000);

}

void loop()
{
    // put your main code here, to run repeatedly:


    if (watchDog == 1)
        {
            resetWDtimer = millis();
            watchDog = 0;
        }

    if (millis() - lostConnectionTime > resetWDtimer )
        {
            digitalWrite(A0, LOW);
            newState = B00000000;
        }

    if (relayState != newState)
        {
            relayState = newState;
            updateRelays();
        }


    newState = checkMessage();


    wdt_reset();   // Сброс WatchDog
}





void updateRelays()
{
    byte i = 9;
    for (byte mask = 00000001; mask > 0; mask <<= 1)      // itarate through bit mask
        {
            if (relayState & mask)         // if bitwise AND resolves to true
                digitalWrite(i, HIGH);             // Block Relay 1 range 1..8 => i
            else
                digitalWrite(i, LOW);
            i--;
        }

}







unsigned long Nowdt = 0;
unsigned int lastBytesReceived;


byte checkMessage()
{
    int length = Serial.available();
    unsigned long now = millis();
    byte message[] = {0, 0, 0, 0, 0};
    byte i = 0;

    if (length == 0)
        {
            lastBytesReceived = 0;
            return (relayState);
        }

    if (lastBytesReceived != length)
        {

            lastBytesReceived = length;
            Nowdt = now;
            return (relayState);
        }
    if (now - 50 < Nowdt)
        return (relayState);





    while (Serial.available() && i < 4)					
        {
            message[i] = Serial.read();
            i++;
        }

    while (Serial.available())
        {
            Serial.read();
        }


    if (message[0] == slaveAddr && message[2] == 7)
        {
            watchDog = 1;

            digitalWrite(A0, HIGH);
            digitalWrite(RS_Pin, HIGH);
            Serial.write(0x09);


            return (message[1]);
        }
    else
        {
            return (relayState);
        }
}



ISR(USART_TX_vect)
{
    PORTB = PORTB & receiveMode;
}


