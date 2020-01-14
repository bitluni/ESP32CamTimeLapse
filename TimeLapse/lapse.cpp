#include "Arduino.h"
#include "camera.h"
#include <stdio.h>
#include "file.h"

unsigned long fileIndex = 0;
unsigned long lapseIndex = 0;
unsigned long frameInterval = 1000;
bool mjpeg = true;
bool lapseRunning = false;
unsigned long lastFrameDelta = 0;

void setInterval(unsigned long delta)
{
    frameInterval = delta;
}

bool startLapse()
{
    if(lapseRunning) return true;
    fileIndex = 0;
    char path[32];
    for(; lapseIndex < 10000; lapseIndex++)
    {
        sprintf(path, "/lapse%03d", lapseIndex);
        if (!fileExists(path))
        {
            createDir(path);
            lastFrameDelta = 0;
            lapseRunning = true;
            return true;
        }
    }
	return false;
}

bool stopLapse()
{
    lapseRunning = false;
}

bool processLapse(unsigned long dt)
{
    if(!lapseRunning) return false;

    lastFrameDelta += dt;
    if(lastFrameDelta >= frameInterval)
    {
        lastFrameDelta -= frameInterval;
        camera_fb_t *fb = NULL;
        esp_err_t res = ESP_OK;
        fb = esp_camera_fb_get();
        if (!fb)
        {
	        Serial.println("Camera capture failed");
	        return false;
        }

        char path[32];
        sprintf(path, "/lapse%03d/pic%05d.jpg", lapseIndex, fileIndex);
        Serial.println(path);
        if(!writeFile(path, (const unsigned char *)fb->buf, fb->len))
        {
            lapseRunning = false;
            return false;
        }
        fileIndex++;
        esp_camera_fb_return(fb);
    }
    return true;
}
