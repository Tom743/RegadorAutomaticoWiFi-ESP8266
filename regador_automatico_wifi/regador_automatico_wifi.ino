//********************************************************************//
//  Nombre  : Regador automático                                      //
//  Autor   : Tomás Badenes                                           //
//  Fecha   :                                                         //
//  Versión : 2.0                                                     //
//  Notas   :                                                         //
//********************************************************************//

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseArduino.h>  // Needs ArduinoJson library version 5. Doesn't compile with v6
#include <Scheduler.h>
#include "params.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", TIME_ZONE * 3600);

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
	bool suspended = false;
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

		timeClient.begin();

		Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
		Firebase.stream("/");

		suspendTask();
	}
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
		}
		while (connect.isRunning()) {
			yield();
		}

		timeClient.update();
		// TODO send for real
		Serial.println(timeClient.getEpochTime());
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
		this->delay(WATER_SECONDS * 1000);
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
		this->delay(CHECK_TIME_SECONDS * 1000);
	}
private:
	float lastWatered = -WAIT_DELAY_SECONDS;
} check;

class ListenStream: public BaseTask {
public:
	void loop() {
		//TODO 25/AUG/2020 For some reason Firebase.setXXX() called from the loop of a task makes the program crash
		if (!isWiFiConnected() && connect.isSuspended()) {
			connect.runTask();
		}
		while (connect.isRunning()) {
			yield();
		}

		if (Firebase.failed()) {
			Serial.println("Streaming error: ");
			Serial.println(Firebase.error());
		}

		if (Firebase.available()) {
			FirebaseObject event = Firebase.readEvent();
			String path = event.getString("path");
			String eventType = event.getString("type");
			eventType.toLowerCase();
			// TODO see what happens if both values change
			if (eventType == "put") {
				if (path == STREAM_DATA_REQUEST && event.getBool("data")) {
					Serial.println("send request");
					if (sendTelemetry.isSuspended()) {
						sendTelemetry.runTask();
					}
					Firebase.setBool(STREAM_DATA_REQUEST, false);
				}
				if (path == STREAM_WATER_NOW_REQUEST && event.getBool("data")) {
					Serial.println("water request");
					if (irrigate.isSuspended()) {
						irrigate.runTask();
					}
					if (sendTelemetry.isSuspended()) {
						sendTelemetry.runTask();
					}
					Firebase.setBool(STREAM_WATER_NOW_REQUEST, false);
				}
			}
			this->delay(500);
		}
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
	// Scheduler.start(&listenStream); // See TODO in ListenStream task
	Scheduler.begin();
}

void loop() {}
