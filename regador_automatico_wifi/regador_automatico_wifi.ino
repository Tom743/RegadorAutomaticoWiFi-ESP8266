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


COROUTINE() {

}

double readSensor(int sensor) {
	pinMode(S0, (sensor >> 0) & 1);
	pinMode(S1, (sensor >> 1) & 1);
	pinMode(S2, (sensor >> 2) & 1);
	pinMode(S3, (sensor >> 3) & 1);
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

	CoroutineScheduler::setup();
}

void loop() {
	CoroutineScheduler::loop();
}
