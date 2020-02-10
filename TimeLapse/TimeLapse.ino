#include <WiFi.h>
#include "file.h"
#include "camera.h"
#include "lapse.h"

const char* ssid = "...";
const char* password = "...";
const char* ap_password = "esp32_time_lapse"; // password to connect to this esp32 when in AP mode

void startCameraServer();

void setup()
{
	Serial.begin(115200);
	Serial.setDebugOutput(true);
	Serial.println();
 
	initFileSystem();
	initCamera();

  int wifiMode = determineWifiMode();
  Serial.printf("Wifi Mode: %u\n", wifiMode);
  initWifi(wifiMode);

	startCameraServer();
	Serial.print("Camera Ready! Use 'http://");
  if (wifiMode == 0)
  {
	  Serial.print(WiFi.softAPIP());
  }
  else
  {
    Serial.print(WiFi.localIP());
  }
	Serial.println("' to connect");
}

// Determine on boot: 0 = AP Mode, 1 = STA Mode
int determineWifiMode()
{
    Serial.println("Scan start..");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("Scan complete :)");
    if (n == 0) {
        Serial.println("No networks found :(");
        return 0;
    } else {
        Serial.print(n);
        Serial.println(" networks found:");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);
            if (WiFi.SSID(i).equals(ssid)) {
              return 1;
            }
        }
    }

    return 0;
}

void initWifi(int wifiMode)
{
  if (wifiMode == 0)
  {
    // IP address of the ESP32 when in AP mode
    IPAddress apIP = IPAddress(192, 168, 1, 101);
    const char *hostname = "esp32TimeLapseCam";
    
    Serial.println("Starting softAP");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    bool result = WiFi.softAP(hostname, ap_password, 1, 0);
    if (!result)
    {
        Serial.println("AP Config failed.");
        return;
    }
    else
    {
        Serial.println("=======================================");
        Serial.println("AP Config Success.");
        Serial.print("AP MAC: ");
        Serial.println(WiFi.softAPmacAddress());

        IPAddress ip = WiFi.softAPIP();
        Serial.print("IP Address: ");
        Serial.println(ip);

        Serial.print("AP Name: ");
        Serial.println(hostname);

        Serial.print("AP Password: ");
        Serial.println(ap_password);
        Serial.println("=======================================");
    }
  }
  else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
  }  
}

void loop()
{
	unsigned long t = millis();
	static unsigned long ot = 0;
	unsigned long dt = t - ot;
	ot = t;
	processLapse(dt);
}
