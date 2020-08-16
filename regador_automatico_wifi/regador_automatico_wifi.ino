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


COROUTINE() {

}

void setup()
{
 	Serial.begin(115200);

	CoroutineScheduler::setup();
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

void loop()
{
	CoroutineScheduler::loop();
}
