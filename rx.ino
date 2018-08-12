#include <SPI.h>
#include "RF24.h"
#include <LiquidCrystal.h>

#define swPin A5

char dataReceived[130];
const uint64_t pipe = 0x123456789ABC;
const int rs = 6, en = 9, d4 = 5, d5 = 4, d6 = 3, d7 = 2;

RF24 radio(7, 8);
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
int page = 0;

void setup() {
	Serial.begin(115200);
	pinMode(swPin, INPUT);

	lcd.begin(16, 2);

	lcd.setCursor(0, 0);
	lcd.write("RX REMOTE");
	lcd.setCursor(0, 1);
	lcd.write("BURHAN AKYUZ");
	delay(2000);

	radio.begin();
	radio.setPALevel(RF24_PA_LOW);
	radio.setChannel(110);
	radio.openReadingPipe(1, pipe);
	radio.startListening();
}

void loop()
{
	if (digitalRead(swPin) == HIGH) { if (page == 0) { page = 1; } else { page = 0; } lcd.clear();  }

	if (radio.available())
	{
		while (radio.available()) { radio.read(&dataReceived, sizeof(dataReceived)); }
		radio.stopListening();

		if (String(dataReceived).indexOf('line') > 0) { Serial.println(); }
		else
		{
			Serial.print(" ");
			Serial.print(dataReceived);
			Serial.print(" ");

			if (page == 0) 
			{
				if (String(dataReceived).indexOf('Ligh') > 0)
				{
					String ldr_val = "";
					for (int i = 0; i < sizeof(dataReceived); i++)
					{
						if (isDigit(dataReceived[i])) { ldr_val += dataReceived[i]; }
					}

					lcd.setCursor(0, 0);
					lcd.print("      ");

					lcd.setCursor(0, 0);
					lcd.print(ldr_val);
					lcd.print("%");
				}

				if (dataReceived[0] == '=')
				{
					String time = "";
					for (int i = 1; i < 32; i++) { time += dataReceived[i]; }
					lcd.setCursor(0, 1);
					lcd.print(time);
				}

				if (dataReceived[0] == 'x')
				{
					String ax = "";
					for (int i = 0; i < 32; i++) { if (dataReceived[i] == ';') { break; } ax += dataReceived[i]; }
					lcd.setCursor(6, 0);
					lcd.print(ax);
				}
			}
			else
			{
				if (dataReceived[0] == 'y')
				{
					String ax = "";
					for (int i = 0; i < 32; i++) { if (dataReceived[i] == ';') { break; } ax += dataReceived[i]; }
					lcd.setCursor(0, 0);
					lcd.print(ax);
				}

				if (dataReceived[0] == 'z')
				{
					String ax = "";
					for (int i = 0; i < 32; i++) { if (dataReceived[i] == ';') { break; } ax += dataReceived[i]; }
					lcd.setCursor(9, 0);
					lcd.print(ax);
				}

				if (dataReceived[0] == 'I')
				{
					String ax = "";
					for (int i = 3; i < 32; i++) { if (dataReceived[i] == ';') { break; } ax += dataReceived[i]; }
					if (dataReceived[1] == '1') { lcd.setCursor(0,1); }
					else { lcd.setCursor(9,1); }
					lcd.print(ax);
				}
			}

		}

		radio.startListening();
	}
}
