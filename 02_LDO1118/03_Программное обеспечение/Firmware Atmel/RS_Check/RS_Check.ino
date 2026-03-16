



byte relayState = 0;
byte newState = 0;
byte slaveAddr = 0;
byte RS_Pin = 10;
unsigned int lostConnectionTime = 3000;
unsigned long resetWDtimer;
byte watchDog = 0;
byte receiveMode = B11111011;


void setup() {

  slaveAddr = 1;
  Serial.begin(9600);

  pinMode(RS_Pin, OUTPUT);
  digitalWrite(RS_Pin, LOW);

  UCSR0B |= ( 1 << TXCIE0 );     // UART 1 Interrupt Enable

}

void loop() {
  // put your main code here, to run repeatedly:

  newState = checkMessage();



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
  byte message[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  byte i = 0;

  if (length == 0) {
    lastBytesReceived = 0;
    return (relayState);
  }

  if (lastBytesReceived != length)
  {
    lastBytesReceived = length;
    Nowdt = now + 5;
    return (relayState);
  }
  if (now < Nowdt)
    return (relayState);



  while (Serial.available() && i < 9)
  {
    message[i] = Serial.read();
    i++;
  }

  while (Serial.available())
    Serial.read();



  digitalWrite(RS_Pin, HIGH);
  
  for (byte j = 0; j <= i; j++)
    Serial.write(message[j]);


}



ISR(USART_TX_vect)  {
  PORTB = PORTB & receiveMode;
}


