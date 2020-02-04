// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// modifed by bitluni 2020

#include "file.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "EEPROM.h"
#include "lapse.h"

const char *indexHtml =
#include "index.h"
	;

extern unsigned long frameInterval;

typedef struct
{
	httpd_req_t *req;
	size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
	jpg_chunking_t *j = (jpg_chunking_t *)arg;
	if (!index)
	{
		j->len = 0;
	}
	if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
	{
		return 0;
	}
	j->len += len;
	return len;
}

static esp_err_t capture_handler(httpd_req_t *req)
{
	camera_fb_t *fb = NULL;
	esp_err_t res = ESP_OK;

	fb = esp_camera_fb_get();
	if (!fb)
	{
		Serial.println("Camera capture failed");
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}
/*
	char path[20];
	sprintf(path, "/%04d", projectIndex);
	if (!fileExists(path))
		createDir(path);
	sprintf(path, "/%04d/%08d.jpg", projectIndex, fileIndex);
	Serial.println(path);
	writeBinary(path, (const unsigned char *)fb->buf, fb->len);
	fileIndex++;*/

	httpd_resp_set_type(req, "image/jpeg");
	httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

	size_t out_len, out_width, out_height;
	uint8_t *out_buf;
	bool s;
	bool detected = false;
	int face_id = 0;
	size_t fb_len = 0;
	if (fb->format == PIXFORMAT_JPEG)
	{
		fb_len = fb->len;
		res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
	}
	else
	{
		jpg_chunking_t jchunk = {req, 0};
		res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
		httpd_resp_send_chunk(req, NULL, 0);
		fb_len = jchunk.len;
	}
	esp_camera_fb_return(fb);
	return res;
}


static esp_err_t startLapseHandler(httpd_req_t *req)
{
	startLapse();
}

static esp_err_t stopLapseHandler(httpd_req_t *req)
{
	stopLapse();
}

static esp_err_t streamHandler(httpd_req_t *req)
{
	camera_fb_t *fb = NULL;
	esp_err_t res = ESP_OK;
	char *part_buf[64];

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
	if (res != ESP_OK)
		return res;
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	do
	{
		fb = esp_camera_fb_get();
		Serial.print("Frame size ");
		Serial.println(fb->len);
		if (!fb)
		{
			Serial.println("Camera capture failed");
			continue;
		}
		size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
		res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
		if (res == ESP_OK)
			res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
		if (res == ESP_OK)
			res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		esp_camera_fb_return(fb);
	} while(res == ESP_OK);
	return res;
}

static esp_err_t cmd_handler(httpd_req_t *req)
{
	char *buf;
	size_t buf_len;
	char variable[32] = {0,};
	char value[32] = {0,};

	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1)
	{
		buf = (char *)malloc(buf_len);
		if (!buf)
		{
			httpd_resp_send_500(req);
			return ESP_FAIL;
		}
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
		{
			if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
				httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK)
			{
			}
			else
			{
				free(buf);
				httpd_resp_send_404(req);
				return ESP_FAIL;
			}
		}
		else
		{
			free(buf);
			httpd_resp_send_404(req);
			return ESP_FAIL;
		}
		free(buf);
	}
	else
	{
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}

	int val = atoi(value);
	sensor_t *s = esp_camera_sensor_get();
	int res = 0;

	if (!strcmp(variable, "framesize"))
	{
		if (s->pixformat == PIXFORMAT_JPEG)
			res = s->set_framesize(s, (framesize_t)val);
	}
	else if (!strcmp(variable, "quality"))
		res = s->set_quality(s, val);
	else if (!strcmp(variable, "contrast"))
		res = s->set_contrast(s, val);
	else if (!strcmp(variable, "brightness"))
		res = s->set_brightness(s, val);
	else if (!strcmp(variable, "saturation"))
		res = s->set_saturation(s, val);
	else if (!strcmp(variable, "gainceiling"))
		res = s->set_gainceiling(s, (gainceiling_t)val);
	else if (!strcmp(variable, "colorbar"))
		res = s->set_colorbar(s, val);
	else if (!strcmp(variable, "awb"))
		res = s->set_whitebal(s, val);
	else if (!strcmp(variable, "agc"))
		res = s->set_gain_ctrl(s, val);
	else if (!strcmp(variable, "aec"))
		res = s->set_exposure_ctrl(s, val);
	else if (!strcmp(variable, "hmirror"))
		res = s->set_hmirror(s, val);
	else if (!strcmp(variable, "vflip"))
		res = s->set_vflip(s, val);
	else if (!strcmp(variable, "agc_gain"))
		res = s->set_agc_gain(s, val);
	else if (!strcmp(variable, "aec2"))
		res = s->set_aec2(s, val);
	else if (!strcmp(variable, "aec_value"))
		res = s->set_aec_value(s, val);
	else if (!strcmp(variable, "dcw"))
		res = s->set_dcw(s, val);
	else if (!strcmp(variable, "bpc"))
		res = s->set_bpc(s, val);
	else if (!strcmp(variable, "wpc"))
		res = s->set_wpc(s, val);
	else if (!strcmp(variable, "raw_gma"))
		res = s->set_raw_gma(s, val);
	else if (!strcmp(variable, "lenc"))
		res = s->set_lenc(s, val);
	else if (!strcmp(variable, "special_effect"))
		res = s->set_special_effect(s, val);
	else if (!strcmp(variable, "wb_mode"))
	{
		if (val == -1)
		{
			res = s->set_awb_gain(s, 0);
		}
		else
		{
			res = s->set_awb_gain(s, 1);
			res = s->set_wb_mode(s, val);
		}
	}
	else if (!strcmp(variable, "ae_level"))
		res = s->set_ae_level(s, val);
	else if (!strcmp(variable, "interval"))
		setInterval(val);
	else
	{
		res = -1;
	}

	if (res)
	{
		return httpd_resp_send_500(req);
	}

	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req)
{
	static char json_response[1024];

	sensor_t *s = esp_camera_sensor_get();
	char *p = json_response;
	*p++ = '{';

	p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
	p += sprintf(p, "\"quality\":%u,", s->status.quality);
	p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
	p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
	p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
	p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
	p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
	p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
	p += sprintf(p, "\"awb\":%u,", s->status.awb);
	p += sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
	p += sprintf(p, "\"aec\":%u,", s->status.aec);
	p += sprintf(p, "\"aec2\":%u,", s->status.aec2);
	p += sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
	p += sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
	p += sprintf(p, "\"agc\":%u,", s->status.agc);
	p += sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
	p += sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
	p += sprintf(p, "\"bpc\":%u,", s->status.bpc);
	p += sprintf(p, "\"wpc\":%u,", s->status.wpc);
	p += sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
	p += sprintf(p, "\"lenc\":%u,", s->status.lenc);
	p += sprintf(p, "\"vflip\":%u,", s->status.vflip);
	p += sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
	p += sprintf(p, "\"dcw\":%u,", s->status.dcw);
	p += sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
	p += sprintf(p, "\"interval\":%u", frameInterval);
	*p++ = '}';
	*p++ = 0;
	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	//httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	sensor_t *s = esp_camera_sensor_get();
	unsigned long l = strlen(indexHtml);
	return httpd_resp_send(req, (const char *)indexHtml, l);
}

void startCameraServer()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_uri_t index_uri = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = index_handler,
		.user_ctx = NULL};

	httpd_uri_t status_uri = {
		.uri = "/status",
		.method = HTTP_GET,
		.handler = status_handler,
		.user_ctx = NULL};

	httpd_uri_t cmd_uri = {
		.uri = "/control",
		.method = HTTP_GET,
		.handler = cmd_handler,
		.user_ctx = NULL};

	httpd_uri_t capture_uri = {
		.uri = "/capture",
		.method = HTTP_GET,
		.handler = capture_handler,
		.user_ctx = NULL};

	httpd_uri_t stream_uri = {
		.uri = "/stream",
		.method = HTTP_GET,
		.handler = streamHandler,
		.user_ctx = NULL};

	httpd_uri_t startLapse_uri = {
		.uri = "/startLapse",
		.method = HTTP_GET,
		.handler = startLapseHandler,
		.user_ctx = NULL};

	httpd_uri_t stopLapse_uri = {
		.uri = "/stopLapse",
		.method = HTTP_GET,
		.handler = stopLapseHandler,
		.user_ctx = NULL};		

	Serial.printf("Starting web server on port: '%d'\n", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK)
	{
		httpd_register_uri_handler(camera_httpd, &index_uri);
		httpd_register_uri_handler(camera_httpd, &cmd_uri);
		httpd_register_uri_handler(camera_httpd, &status_uri);
		httpd_register_uri_handler(camera_httpd, &capture_uri);

		httpd_register_uri_handler(camera_httpd, &startLapse_uri);
		httpd_register_uri_handler(camera_httpd, &stopLapse_uri);
	}

	config.server_port += 1;
	config.ctrl_port += 1;
	Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
	if (httpd_start(&stream_httpd, &config) == ESP_OK)
	{
		httpd_register_uri_handler(stream_httpd, &stream_uri);
	}
}
