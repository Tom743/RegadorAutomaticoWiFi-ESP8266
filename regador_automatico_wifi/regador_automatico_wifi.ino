//********************************************************************//
//  Nombre  : Regador automático                                      //
//  Autor   : Tomás Badenes                                           //
//  Fecha   :                                                         //
//  Versión : 2.0                                                     //
//  Notas   :                                                         //
//********************************************************************//

#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>  // Needs ArduinoJson library version 5. Doesn't compile with v6
#include <Scheduler.h>
#include "params.h"


double readSensor(int sensor) {
	// Gets the individual bits from the sensor conneciton number and sends them to the CD74HC4067 IC
	digitalWrite(S0, (sensor >> 0) & 1);
	digitalWrite(S1, (sensor >> 1) & 1);
	digitalWrite(S2, (sensor >> 2) & 1);
	digitalWrite(S3, (sensor >> 3) & 1);
	return analogRead(COMMON_ANALOG_INPUT);
}

bool isWiFiConnected() {
	return WiFi.status() == WL_CONNECTED;
}

class BaseTask: public Task {
protected:
	void loop() {
		while (suspended) {
			yield();
		}
	}
	void suspendTask() {
		suspended = true;
	}
public:
	void runTask() {
		suspended = false;
	}
	bool isSuspended() {
		return suspended;
	}
	bool isRunning() {
		return !suspended;
	}
private:
	bool suspended = true;
};

class Connect: public BaseTask {
public:
	void loop() {
		BaseTask::loop();
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		Serial.print("connecting");
		while (!isWiFiConnected()) {
			Serial.print(".");
			this->delay(500);
		}
		Serial.println();
		Serial.print("Connected: ");
		Serial.println(WiFi.localIP());

		//Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
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
			Serial.println("Connected to firebase successfuly");
			suspendTask();
		} else {
			Serial.println("Trying again");
		}
	}
private:
	bool tryAgain = false;
} connect;

class SendTelemetry: public BaseTask {
public:
	void setup() {
		suspendTask();
	}
	void loop() {
		BaseTask::loop();
		if (!isWiFiConnected() && connect.isSuspended()) {
			connect.runTask();
			while(connect.isRunning()) {
				yield();
			}
		}
		// TODO send for real
		Serial.println(readSensor(HUMIDITY_SENSOR));
		Serial.println(readSensor(TEMP_SENSOR));
		Serial.println(readSensor(LIGHT_SENSOR));
		Serial.println(readSensor(WATER_LEVEL_SENSOR));
		Serial.println("ENVIANDO");
		suspendTask();
	}
} sendTelemetry;

class Irrigate: public BaseTask {
public:
	void setup() {
		suspendTask();
	}
	void loop() {
		BaseTask::loop();
		digitalWrite(WATER_PUMP_PIN, HIGH);
		Serial.println("regando");
		this->delay(WATER_SECONDS*1000);
		digitalWrite(WATER_PUMP_PIN, LOW);
		suspendTask();
	}
} irrigate;

class Check: public BaseTask {
public:
	void loop() {
		Serial.println("checking");
		if (readSensor(HUMIDITY_SENSOR) > HUMIDITY_THRESHOLD && ((millis() / 1000) - lastWatered) >= WAIT_DELAY_SECONDS) {
			Serial.println("deberia regar");
			if (irrigate.isSuspended()) {
				irrigate.runTask();
			} else {
				Serial.println("ya estoy regando");
			}
			lastWatered = millis() / 1000;
		}
		Serial.println("deberia enviar");
		if (sendTelemetry.isSuspended()) {
			sendTelemetry.runTask();
		} else {
			Serial.println("ya estoy enviando");
		}
		this->delay(CHECK_TIME_SECONDS*1000);
	}
private:
	float lastWatered = -WAIT_DELAY_SECONDS;
} check;

class ListenStream: public BaseTask {
public:
	void loop() {
		if (Firebase.failed()) {
			Serial.println("Firebase error: ");
			Serial.println(Firebase.error());
			if (connect.isSuspended()) {
				connect.runTask();
			}
			while (connect.isRunning()) {
				yield();
			}
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
						sendTelemetry.runTask();
					}
					Firebase.setBool(STREAM_DATA_REQUEST, false);
				}
				if (eventPath == STREAM_WATER_NOW_REQUEST && event.getBool("data")) {
					if (irrigate.isSuspended()) {
						irrigate.runTask();
					}
					if (sendTelemetry.isSuspended()) {
						sendTelemetry.runTask();
					}
					Firebase.setBool(STREAM_WATER_NOW_REQUEST, false);
				}
			}
		}
		this->delay(500);
	}
} listenStream;

void setup() {
	Serial.begin(115200);
	pinMode(S0, OUTPUT);
	pinMode(S1, OUTPUT);
	pinMode(S2, OUTPUT);
	pinMode(S3, OUTPUT);
	pinMode(WATER_PUMP_PIN, OUTPUT);

	Scheduler.start(&connect);
	Scheduler.start(&sendTelemetry);
	Scheduler.start(&irrigate);
	Scheduler.start(&check);
	Scheduler.start(&listenStream);
	Scheduler.begin();
}

void loop() {}
