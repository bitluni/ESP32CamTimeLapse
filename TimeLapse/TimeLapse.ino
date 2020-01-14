#include <WiFi.h>
#include "file.h"
#include "camera.h"
#include "lapse.h"

const char *ssid = "...";
const char *password = "...";

void startCameraServer();

void setup()
{
	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println();
	initFileSystem();
	initCamera();

	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	startCameraServer();
	Serial.print("Camera Ready! Use 'http://");
	Serial.print(WiFi.localIP());
	Serial.println("' to connect");
}

void loop()
{
	unsigned long t = millis();
	static unsigned long ot = 0;
	unsigned long dt = t - ot;
	ot = t;
	processLapse(dt);
}
