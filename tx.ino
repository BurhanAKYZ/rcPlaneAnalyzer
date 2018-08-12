#include <Time.h>
#include <TimeLib.h>
#include <DS1302RTC.h>
#include <Wire.h>
#include <TimerOne.h>
#include <SPI.h>
#include "ACS712.h"
#include "RF24.h"

#define LDR_pin A0
#define Curr_pin1 A3
#define Curr_pin2 A2
#define ledPin 3

#define MPU9250_ADDRESS            0x68
#define MAG_ADDRESS                0x0C

#define ACC_FULL_SCALE_2_G        0x00  
#define ACC_FULL_SCALE_4_G        0x08
#define ACC_FULL_SCALE_8_G        0x10
#define ACC_FULL_SCALE_16_G       0x18

bool isRTCOk = false;
boolean serialActivityExist = false;
boolean isCmdExist = false;
boolean stringComplete = false;
String cmdString = "";
int LDR_value = 0;
int current = 0;
int packetError = 0;
int transimttedPackets = 0;
const uint64_t pipe = 0x123456789ABC;
tmElements_t tm;

// Set pins:  CE, IO,CLK
DS1302RTC RTC(4, 5, 6);
ACS712 sensor1(ACS712_20A, Curr_pin1);
ACS712 sensor2(ACS712_20A, Curr_pin2);
RF24 radio(7, 8);

void setup()
{
	Serial.begin(115200);
	isRTCOk = true;

	if (RTC.haltRTC())
	{
		Serial.print("The DS1302 is stopped.  Please run the SetTime  example to initialize the time and begin running.");
		tm.Second = 0; tm.Hour = 0; tm.Minute = 0; tm.Wday = 2; tm.Day = 1; tm.Month = 7; tm.Year = 2;
		RTC.set(makeTime(tm));
		if (RTC.haltRTC())  isRTCOk = false;
	}
	
	if (!RTC.writeEN()) { Serial.println("The DS1302 is write protected. This normal."); }

	Wire.begin();

	I2CwriteByte(MPU9250_ADDRESS, 29, 0x06);
	I2CwriteByte(MPU9250_ADDRESS, 26, 0x06);

	I2CwriteByte(MPU9250_ADDRESS, 28, ACC_FULL_SCALE_4_G);
	I2CwriteByte(MPU9250_ADDRESS, 0x37, 0x02);
	I2CwriteByte(MAG_ADDRESS, 0x0A, 0x16);

	sensor1.calibrate();
	sensor2.calibrate();

	blink_led(3, 150);
	delay(1000);

	radio.begin();
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(110);
	radio.openWritingPipe(pipe);

	Timer1.initialize(500000);
	Timer1.attachInterrupt(blink);
}

void loop()
{
	LDR_value = analogRead(LDR_pin);
	LDR_value = fmap(LDR_value, 1023, 0, 0, 100);
	Serial.print("LDR = ");
	Serial.print(LDR_value);

	sendRFData(String("Light : ") + String(LDR_value) + String("%"));
	
	if (isRTCOk)
	{
		RTC.get();
		if (!RTC.read(tm))
		{
			Serial.print("  Time = "); print2digits(tm.Hour); Serial.print(':');
			print2digits(tm.Minute); Serial.print(':');
			print2digits(tm.Second); Serial.print(" ");
			Serial.print(tm.Day); Serial.print("/");
			Serial.print(tm.Month); Serial.print("/");
			Serial.print(year() + 48); Serial.print(" ");

			sendRFData(String("=") + String(tm.Hour) + String(":") + String(tm.Minute) + String(":") + String(tm.Second) + String(" ") + String(tm.Day) + String("/") + String(tm.Month) + String("/") + String(year() + 48));
		}
	}
	else
	{
		Serial.print(" No Clock Data !!ERROR!! ");
		sendRFData("  Time = NaN  ");
	}

	// Read accelerometer
	uint8_t Buf[14];
	I2Cread(MPU9250_ADDRESS, 0x3B, 14, Buf);

	int16_t ax = -(Buf[0] << 8 | Buf[1]);
	int16_t ay = -(Buf[2] << 8 | Buf[3]);
	int16_t az = Buf[4] << 8 | Buf[5];
	
	Serial.print(" ACC-X : "); Serial.print(ax, DEC);
	Serial.print(" ACC-Y : "); Serial.print(ay, DEC);
	Serial.print(" ACC-Z : "); Serial.print(az, DEC);

	sendRFData("x:" + String(ax) + ";");
	sendRFData("y:" + String(ay) + ";");
	sendRFData("z:" + String(az) + ";");

	float val1 = getCurrent(Curr_pin1);
	Serial.print(String("  I_1 = ") + val1 + " mA"); 
	sendRFData("I1:" + String(val1, 3) + ";");

	float val2 = getCurrent(Curr_pin2);
	Serial.print(String("  I_2 = ") + val2 + " mA");
	sendRFData("I2:" + String(val2, 3) + ";");

	Serial.println();
	sendRFData("line");

	if (stringComplete)
	{
		stringComplete = false;
		command(cmdString);
		cmdString = "";
		isCmdExist = false;
	}
	delay(150);
}

void print2digits(int number)
{
	if (number >= 0 && number < 10)
		Serial.write('0');
	Serial.print(number);
}

float getCurrent(int pin) 
{
	int raw1 = analogRead(pin);
	float val1 = fmap(raw1, 0, 1023, 0.0, 5.0);
	val1 = (val1 - 2.5) * 10;
	return val1;
}

void serialEvent()
{
	// > : start
	// < : stop
	char rx_byte = 0;
	while (Serial.available() > 0)
	{
		rx_byte = Serial.read();
		if (rx_byte == '<') { serialActivityExist = true; }

		if (rx_byte == '>' || isCmdExist)
		{
			isCmdExist = true;
			if (rx_byte != '<') cmdString += rx_byte;
		}

		if (rx_byte == '<' && isCmdExist) { stringComplete = true; }
	}
}

void command(String commandBuffer)
{
	char commandID = (char)commandBuffer[1];
	String commandInString = "";


	if (commandID == 'S')
	{
		switch ((char)commandBuffer[2])
		{
		case 'H': // Set Hour
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Hour = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		case 'M': // Set Minute
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Minute = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		case 'S': // Set Second
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Second = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		case 'Y': // Set year
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Year = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		case 'm': // Set month
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Month = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		case 'D': // Set day
			for (int i = 3; i <= commandBuffer.length() - 1; i++)
			{
				if (isDigit(commandBuffer[i])) { commandInString += (char)commandBuffer[i]; }
			}
			tm.Day = commandInString.toInt();
			RTC.set(makeTime(tm));
			break;
		default: Serial.println("Clock is set!"); break;
		}
	}

}

void blink()
{
	/*Serial.print("Transmitted Packets = ");
	Serial.print(transimttedPackets);
	Serial.print("  Packet Errors = ");
	Serial.println(packetError);*/

	if (digitalRead(ledPin) == HIGH)
	{
		digitalWrite(ledPin, LOW);
	}
	else
	{
		digitalWrite(ledPin, HIGH);
	}
}

void sendRFData(String data)
{
	char arr[32];
	data.toCharArray(arr, 32);
	bool result = radio.write(&arr, sizeof(arr));
	if (result) { transimttedPackets++; }
	else { packetError++; }
}

void blink_led(unsigned int count, unsigned int delay_ms)
{
	for (int i = 0; i < count; i++)
	{
		digitalWrite(ledPin, HIGH);
		delay(delay_ms);
		digitalWrite(ledPin, LOW);
		delay(delay_ms);
	}
}

String getJSON(String input[][2], uint8_t rows)
{
	String vals = "{";
	for (int i = 0; i < rows; i++)
	{
		String key = input[i][0];
		String val = input[i][1];

		vals += "\"" + key + "\":\"" + val + "\",";
	}
	vals.remove(vals.length() - 1, 1);
	vals += "}";
	return vals;
}

float fmap(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void I2Cread(uint8_t Address, uint8_t Register, uint8_t Nbytes, uint8_t* Data)
{
	// Set register address
	Wire.beginTransmission(Address);
	Wire.write(Register);
	Wire.endTransmission();

	// Read Nbytes
	Wire.requestFrom(Address, Nbytes);
	uint8_t index = 0;
	while (Wire.available())
		Data[index++] = Wire.read();
}

void I2CwriteByte(uint8_t Address, uint8_t Register, uint8_t Data)
{
	// Set register address
	Wire.beginTransmission(Address);
	Wire.write(Register);
	Wire.write(Data);
	Wire.endTransmission();
}