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


bool isWiFiConnected() {
	return WiFi.status() == WL_CONNECTED;
}


static bool tryAgain;
COROUTINE(connect) {
	COROUTINE_LOOP() {
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		Serial.print("connecting");
		while (!isWiFiConnected()) {
			Serial.print(".");
			COROUTINE_DELAY(500);
		}
		Serial.println();
		Serial.print("Connected: ");
		Serial.println(WiFi.localIP());

		Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
		if (Firebase.success()) {
			Firebase.stream(STREAM_DATA_REQUEST);
			if (Firebase.failed()) {
				Serial.println("Stream begin failed");
				tryAgain = true;
			} else {
				tryAgain = false;
			}
			Firebase.stream(STREAM_WATER_NOW_REQUEST);
			if (Firebase.failed()) {
				Serial.println("Stream begin failed");
				tryAgain = true;
			} else {
				tryAgain = false;
			}
		} else {
			Serial.println("Stream begin failed");
			tryAgain = true;
		}

		if (!tryAgain) {
			Serial.println("suspended");
			connect.suspend();
		} else {
			Serial.println("Connected to firebase successfuly");
			COROUTINE_DELAY(1000);
		}
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

COROUTINE(sendTelemetry) {
	COROUTINE_LOOP() {
		if (!isWiFiConnected() && connect.isSuspended()) {
			connect.resume();
			COROUTINE_AWAIT(connect.isSuspended());
		}
		// TODO send for real
		Serial.println(readSensor(HUMIDITY_SENSOR));
		Serial.println(readSensor(TEMP_SENSOR));
		Serial.println(readSensor(LIGHT_SENSOR));
		Serial.println(readSensor(WATER_LEVEL_SENSOR));
		Serial.println("ENVIANDO");
		sendTelemetry.suspend();
	}
}

COROUTINE(irrigate) {
	COROUTINE_LOOP() {
		digitalWrite(WATER_PUMP_PIN, HIGH);
		Serial.println("regando");
		COROUTINE_DELAY_SECONDS(WATER_SECONDS);
		digitalWrite(WATER_PUMP_PIN, LOW);
		irrigate.suspend();
	}
}

COROUTINE(checkAndIrrigate) {
	COROUTINE_BEGIN();
	static float lastWatered = 0;
	while (true) {
		Serial.println("check");
		if (readSensor(HUMIDITY_SENSOR) > HUMIDITY_THRESHOLD && ((millis() / 1000) - lastWatered) >= WAIT_DELAY_SECONDS) {
			Serial.println("deberia regar");
			if (irrigate.isSuspended()) {
				irrigate.resume();
			} else {
				Serial.println("ya estoy regando");
			}
			lastWatered = millis() / 1000;
		}
		Serial.println("deberia enviar");
		if (sendTelemetry.isSuspended()) {
			sendTelemetry.resume();
		} else {
			Serial.println("ya estoy enviando");
		}
		COROUTINE_DELAY_SECONDS(CHECK_TIME_SECONDS);
	}
	COROUTINE_END();
}

COROUTINE(listenStream) {
	COROUTINE_LOOP() {
		if (Firebase.failed()) {
			Serial.println("Firebase error: ");
			Serial.println(Firebase.error());
			if (connect.isSuspended()) {
				connect.resume();
			}
			COROUTINE_AWAIT(connect.isSuspended());
		}
		if (Firebase.available()) {
			static FirebaseObject event = Firebase.readEvent();
			static String eventType = event.getString("type");
			eventType.toLowerCase();
			static String eventPath = event.getString("path");
			// TODO see what happens if both values change
			if (eventType == "put") {
				if (eventPath == STREAM_DATA_REQUEST && event.getBool("data")) {
					if (sendTelemetry.isSuspended()) {
						sendTelemetry.resume();
					}
					Firebase.setBool(STREAM_DATA_REQUEST, false);
				}
				if (eventPath == STREAM_WATER_NOW_REQUEST && event.getBool("data")) {
					if (irrigate.isSuspended()) {
						irrigate.resume();
					}
					if (sendTelemetry.isSuspended()) {
						sendTelemetry.resume();
					}
					Firebase.setBool(STREAM_WATER_NOW_REQUEST, false);
				}
			}
		}
		COROUTINE_DELAY(500);
	}
}

void setup() {
	Serial.begin(115200);
	pinMode(S0, OUTPUT);
	pinMode(S1, OUTPUT);
	pinMode(S2, OUTPUT);
	pinMode(S3, OUTPUT);
	pinMode(WATER_PUMP_PIN, OUTPUT);

	irrigate.suspend();
	sendTelemetry.suspend();
	CoroutineScheduler::setup();
}

void loop() {
	CoroutineScheduler::loop();
}
