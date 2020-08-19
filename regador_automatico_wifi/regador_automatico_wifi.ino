//********************************************************************//
//  Nombre  : Regador automático                                      //
//  Autor   : Tomás Badenes                                           //
//  Fecha   :                                                         //
//  Versión : 2.0                                                     //
//  Notas   :                                                         //
//********************************************************************//

#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <AceRoutine.h>
using namespace ace_routine;
#include "params.h"


COROUTINE(checkAndIrrigate) {
	int lastWatered = -WAIT_DELAY_SECONDS;
	COROUTINE_LOOP() {
		if (readSensor(HUMIDITY_SENSOR) > HUMIDITY_THRESHOLD && ((millis()/1000)-lastWatered) >= WAIT_DELAY_SECONDS) {
		    digitalWrite(WATER_PUMP_PIN, HIGH);
		    COROUTINE_DELAY_SECONDS(WATER_SECONDS);
		    digitalWrite(WATER_PUMP_PIN, LOW);
		    lastWatered = millis()/1000;
		}
		// TODO sendTelemetry(). This should trigger a coroutine and pass instantly to the next line, dont wait to send it
		COROUTINE_DELAY_SECONDS(CHECK_TIME_SECONDS);
	}
}

double readSensor(int sensor) {
	// Gets the individual bits from the sensor conneciton number and sends them to the CD74HC4067 IC
	digitalWrite(S0, (sensor >> 0) & 1);
	digitalWrite(S1, (sensor >> 1) & 1);
	digitalWrite(S2, (sensor >> 2) & 1);
	digitalWrite(S3, (sensor >> 3) & 1);
	return analogRead(COMMON_ANALOG_INPUT);
}

void conectWiFi() {
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print("connecting");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println();
	Serial.print("connected: ");
	Serial.println(WiFi.localIP());
}

void setup() {
 	Serial.begin(115200);

 	pinMode(S0, OUTPUT);
 	pinMode(S1, OUTPUT);
 	pinMode(S2, OUTPUT);
 	pinMode(S3, OUTPUT);
 	pinMode(WATER_PUMP_PIN, OUTPUT);

	CoroutineScheduler::setup();
}

void loop() {
	CoroutineScheduler::loop();
}
