//********************************************************************//
//  Nombre  : Regador automático                                      //
//  Autor   : Tomás Badenes                                           //
//  Fecha   :                                                         //
//  Versión : 2.0                                                     //
//  Notas   :                                                         //
//********************************************************************//

#include <FirebaseESP8266.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Scheduler.h>
#include "params.h"

FirebaseData firebaseData;
unsigned long lastIrrigation = 0;  // TODO 26/AUG/2020 Make it send this value when irrigation is called, waiting till refreshes it

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

		// TODO 25/AUG/2020 See full settings for firebase
		Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
		Firebase.reconnectWiFi(true);
		if (!Firebase.beginStream(firebaseData, "/")) {
			Serial.println("Can't begin stream connection...");
			Serial.println("REASON: " + firebaseData.errorReason());
			Serial.println();
		}

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
			Serial.println("Waiting for connection...");
			connect.runTask();
		}
		while (connect.isRunning()) {
			yield();
		}
		Serial.println("Sending data");

		timeClient.update();
		FirebaseJson json;
		json.set("time", String(timeClient.getEpochTime()));  // Can't set unsigned longs
		json.set("humidity", readSensor(HUMIDITY_SENSOR));
		json.set("light", readSensor(TEMP_SENSOR));
		json.set("temperature", readSensor(LIGHT_SENSOR));
		if (lastIrrigation != 0) {
			json.set("last irrigation", String(lastIrrigation));  // Can't set unsigned longs
		} else {
			json.set("last irrigation", "unknown");
		}

		if (Firebase.push(firebaseData, "/plants/plantita1/", json)) {
			Serial.println("Push passed");
		} else {
			Serial.println("Push failed");
			Serial.println("REASON: " + firebaseData.errorReason());
		}

		String str;
		json.toString(str, true);
		Serial.println(str);

		json.clear();
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
		if (isWiFiConnected()) {
			timeClient.update();
			lastIrrigation = timeClient.getEpochTime();
		} else {
			lastIrrigation = 0;
		}
		digitalWrite(WATER_PUMP_PIN, PUMP_HIGH_STATE);
		Serial.println("Watering plant");
		this->delay(WATER_SECONDS * 1000);
		digitalWrite(WATER_PUMP_PIN, !PUMP_HIGH_STATE);
		suspendTask();
	}
} irrigate;

class Check: public BaseTask {
public:
	void loop() {
		Serial.println("Checking");
		if (readSensor(HUMIDITY_SENSOR) > HUMIDITY_THRESHOLD && ((millis() / 1000) - lastWatered) >= WAIT_DELAY_SECONDS) {
			if (irrigate.isSuspended()) {
				irrigate.runTask();
			}
			lastWatered = millis() / 1000;
		}
		if (sendTelemetry.isSuspended()) {
			sendTelemetry.runTask();
		}
		this->delay(CHECK_TIME_SECONDS * 1000);
	}
private:
	float lastWatered = -WAIT_DELAY_SECONDS;
} check;

class ListenStream: public BaseTask {
public:
	void loop() {
		if (!isWiFiConnected() && connect.isSuspended()) {
			connect.runTask();
		}
		while (connect.isRunning()) {
			yield();
		}

		if (!Firebase.readStream(firebaseData)) {
			Serial.println("Can't read stream data...");
			Serial.println("REASON: " + firebaseData.errorReason());
			Serial.println();
		}
		if (firebaseData.streamAvailable()) {
			if (firebaseData.eventType() == "put" && firebaseData.dataType() == "boolean") {
				if (firebaseData.boolData() == 1) {
					if (firebaseData.dataPath() == PATH_DATA_REQUEST) {
						Serial.println("Telemetry requested\n");
						if (sendTelemetry.isSuspended()) {
							sendTelemetry.runTask();
						}
						Firebase.setBool(firebaseData, PATH_DATA_REQUEST, false);
					}
					if (firebaseData.dataPath() == PATH_WATER_NOW_REQUEST) {
						Serial.println("Irrigation requested\n");
						if (irrigate.isSuspended()) {
							irrigate.runTask();
						}
						if (sendTelemetry.isSuspended()) {
							sendTelemetry.runTask();
						}
						Firebase.setBool(firebaseData, PATH_WATER_NOW_REQUEST, false);
					}
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
	digitalWrite(WATER_PUMP_PIN, !PUMP_HIGH_STATE);

	Scheduler.start(&connect);
	Scheduler.start(&sendTelemetry);
	Scheduler.start(&irrigate);
	Scheduler.start(&check);
	Scheduler.start(&listenStream);
	Scheduler.begin();
}

void loop() {}
