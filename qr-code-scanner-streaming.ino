
#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "quirc.h"
#include <WiFi.h>
#include "esp_http_server.h"

#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"           //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h"
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>

//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_WROVER_KIT)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 21
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#elif defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 4
#define SIOD_GPIO_NUM 18
#define SIOC_GPIO_NUM 23

#define Y9_GPIO_NUM 36
#define Y8_GPIO_NUM 37
#define Y7_GPIO_NUM 38
#define Y6_GPIO_NUM 39
#define Y5_GPIO_NUM 35
#define Y4_GPIO_NUM 14
#define Y3_GPIO_NUM 13
#define Y2_GPIO_NUM 34
#define VSYNC_GPIO_NUM 5
#define HREF_GPIO_NUM 27
#define PCLK_GPIO_NUM 25

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM 15
#define XCLK_GPIO_NUM 27
#define SIOD_GPIO_NUM 25
#define SIOC_GPIO_NUM 23

#define Y9_GPIO_NUM 19
#define Y8_GPIO_NUM 36
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 39
#define Y5_GPIO_NUM 5
#define Y4_GPIO_NUM 34
#define Y3_GPIO_NUM 35
#define Y2_GPIO_NUM 32
#define VSYNC_GPIO_NUM 22
#define HREF_GPIO_NUM 26
#define PCLK_GPIO_NUM 21

#elif defined(CAMERA_MODEL_M5STACK_WITHOUT_PSRAM)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM 15
#define XCLK_GPIO_NUM 27
#define SIOD_GPIO_NUM 25
#define SIOC_GPIO_NUM 23

#define Y9_GPIO_NUM 19
#define Y8_GPIO_NUM 36
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 39
#define Y5_GPIO_NUM 5
#define Y4_GPIO_NUM 34
#define Y3_GPIO_NUM 35
#define Y2_GPIO_NUM 17
#define VSYNC_GPIO_NUM 22
#define HREF_GPIO_NUM 26
#define PCLK_GPIO_NUM 21

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#else
#error "Camera model not selected"
#endif
/* ======================================== */

// LEDs GPIO
#define LED_OnBoard 4
#define LED_Green 12
#define LED_Blue 13

// creating a task handle
TaskHandle_t QRCodeReader_Task;

struct QRCodeData {
  bool valid;
  int dataType;
  uint8_t payload[1024];
  int payloadLen;
};

struct quirc* q = NULL;
uint8_t* image = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;
struct QRCodeData qrCodeData;
String QRCodeResult = "";
String QRCodeResultSend = "";
int connectingCnt=0;
bool ws_run = false;
int wsLive_val = 0;
int last_wsLive_val;
byte get_wsLive_interval = 0;
bool get_wsLive_val = true;
/* ======================================== */

/* ======================================== Variables for millis() */
unsigned long previousMillis = 0;
const long interval = 1000;
/* ======================================== */

/* ======================================== Replace with your network credentials */

IPAddress localIP;
IPAddress localGateway;

IPAddress subnet(255, 255, 0, 0);
const char* PARAM_INP_1 = "ssid";
const char* PARAM_INP_2 = "pass";
const char* PARAM_INP_3 = "ip";
const char* PARAM_INP_4 = "gateway";

AsyncWebServer server(80);
String ssid;
String password;
String ip;
String gateway;
/* ======================================== */

/* ======================================== */
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
/* ======================================== */

/* ======================================== Empty handle to esp_http_server */
httpd_handle_t index_httpd = NULL;
httpd_handle_t stream_httpd = NULL;
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>

  <head>
  
    <title>Welcome To Peekseller</title>
    
    <meta name="viewport" content="width=device-width, initial-scale=1">
    
    <style>
    
      body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 10px;}

      /* ----------------------------------- Slider */
      .slidecontainer {
        width: 100%;
      }

      .slider {
        -webkit-appearance: none;
        width: 50%;
        height: 10px;
        border-radius: 5px;
        background: #d3d3d3;
        outline: none;
        opacity: 0.7;
        -webkit-transition: .2s;
        transition: opacity .2s;
      }

      .slider:hover {
        opacity: 1;
      }

      .slider::-webkit-slider-thumb {
        -webkit-appearance: none;
        appearance: none;
        width: 20px;
        height: 20px;
        border-radius: 50%;
        background: #04AA6D;
        cursor: pointer;
      }

      .slider::-moz-range-thumb {
        width: 20px;
        height: 20px;
        border-radius: 50%;
        background: #04AA6D;
        cursor: pointer;
      }
      /* ----------------------------------- */

      /* ----------------------------------- Stream Viewer */
      img {
        width: auto ;
        max-width: 100% ;
        height: auto ; 
      }
      /* ----------------------------------- */
      
    </style>
    
  </head>
  
  <body>
  
    <h3>QR Code Reader Stream Web Server</h3>
    
    <img src="" id="vdstream">
    
    <br><br>
    
    <div class="slidecontainer">
      <span style="font-size:15;">LED Flash &nbsp;</span>
      <input type="range" min="0" max="20" value="0" class="slider" id="mySlider">
    </div>

    <br>

    <p>QR Code Scan Result :</p>
    <div style="padding: 5px; border: 3px solid #075264; text-align: center; width: 70%; margin: auto; color:#0A758F;" id="showqrcodeval"></div>

    <br>

    <button type="button" onclick="CopyQRCodeRslt()">Copy Result</button>
    <button type="button" onclick="send_btn_cmd('clr')">Clear Result</button>
    
    <script>
      /* ----------------------------------- Calls the video stream link and displays it */ 
      window.onload = document.getElementById("vdstream").src = window.location.href.slice(0, -1) + ":81/stream";
      /* ----------------------------------- */
      
      var slider = document.getElementById("mySlider");
      
      /* ----------------------------------- Variable declaration and timer to display QR Code reading results. */
      var myTmr;
      let qrcodeval = "...";
      start_timer();
      /* ----------------------------------- */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Update the current slider value (each time you drag the slider handle) */
      slider.oninput = function() {
        let slider_pwm_val = "S," + slider.value;
        send_cmd(slider_pwm_val);
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Function for sending commands */
      function send_cmd(cmds) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/action?go=" + cmds, true);
        xhr.send();
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Start and stop the timer */
      function start_timer() {
        myTmr = setInterval(myTimer, 500)
      }
      
      function stop_timer() {
        clearInterval(myTmr)
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Timer to get QR Code reading result and display it. */
      function myTimer() {
        getQRCodeVal();
        textQRCodeVal();
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Function to display the results of reading the QR Code. */
      function textQRCodeVal() {
        document.getElementById("showqrcodeval").innerHTML = qrcodeval;
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Function to send commands to the ESP32 Cam whenever the button is clicked. */
      function send_btn_cmd(cmds) {
        let btn_cmd = "B," + cmds;
        send_cmd(btn_cmd);
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Function to copy QR Code reading result to clipboard. */
      // Source: https://techoverflow.net/2018/03/30/copying-strings-to-the-clipboard-using-pure-javascript/
      function CopyQRCodeRslt() {
        // Create new element
        var el = document.createElement('textarea');
        // Set value (string to be copied)
        el.value = qrcodeval;
        // Set non-editable to avoid focus and move outside of view
        el.setAttribute('readonly', '');
        el.style = {position: 'absolute', left: '-9999px'};
        document.body.appendChild(el);
        // Select text inside element
        el.select();
        // Copy text to clipboard
        document.execCommand('copy');
        // Remove temporary element
        document.body.removeChild(el);
        /* Alert the copied text */
        alert("The result of reading the QR Code has been copied.");
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */

      /* :::::::::::::::::::::::::::::::::::::::::::::::: Function to get QR Code reading results. */
      function getQRCodeVal() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            qrcodeval = this.responseText;
          }
        };
        xhttp.open("GET", "/getqrcodeval", true);
        xhttp.send();
      }
      /* :::::::::::::::::::::::::::::::::::::::::::::::: */
    </script>
  
  </body>
  
</html>
)rawliteral";
/* ======================================== */

/* ________________________________________________________________________________ Index handler function to be called during GET or uri request */
static esp_err_t index_handler(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char*)INDEX_HTML, strlen(INDEX_HTML));
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ stream handler function to be called during GET or uri request. */
static esp_err_t stream_handler(httpd_req_t* req) {
  ws_run = true;
  vTaskDelete(QRCodeReader_Task);
  Serial.print("stream_handler running on core ");
  Serial.println(xPortGetCoreID());

  camera_fb_t* fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t* _jpg_buf = NULL;
  char* part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  /* ---------------------------------------- Loop to show streaming video from ESP32 Cam camera and read QR Code. */
  while (true) {
    ws_run = true;
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed (stream_handler)");
      res = ESP_FAIL;
    } else {
      q = quirc_new();
      if (q == NULL) {
        Serial.print("can't create quirc object\r\n");
        continue;
      }

      quirc_resize(q, fb->width, fb->height);
      image = quirc_begin(q, NULL, NULL);
      memcpy(image, fb->buf, fb->len);
      quirc_end(q);

      int count = quirc_count(q);
      if (count > 0) {
        quirc_extract(q, 0, &code);
        err = quirc_decode(&code, &data);

        if (err) {
          QRCodeResult = "Decoding FAILED";
          Serial.println(QRCodeResult);
        } else {
          Serial.printf("Decoding successful:\n");
          dumpData(&data);
        }
        Serial.println();
      }

      image = NULL;
      quirc_destroy(q);

      if (fb->width > 200) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char*)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }

    wsLive_val++;
    if (wsLive_val > 999) wsLive_val = 0;
  }
  /* ---------------------------------------- */
  return res;
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ cmd handler function to be called during GET or uri request. */
static esp_err_t cmd_handler(httpd_req_t* req) {
  char* buf;
  size_t buf_len;
  char variable[32] = {
    0,
  };

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "go", variable, sizeof(variable)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  int res = 0;

  Serial.print("Incoming command : ");
  Serial.println(variable);
  Serial.println();
  String getData = String(variable);
  String resultData = getValue(getData, ',', 0);

  /* ---------------------------------------- Controlling the LEDs on the ESP32 Cam board with PWM. */
  // Example :
  // Incoming command = S,10
  // S = Slider
  // 10 = slider value
  // I set the slider value range from 0 to 20.
  // Then the slider value is changed from 0 - 20 or vice versa to 0 - 255 or vice versa.
  if (resultData == "S") {
    resultData = getValue(getData, ',', 1);
    int pwm = map(resultData.toInt(), 0, 20, 0, 255);
    ledcSetup(2, 5000, 8);
    ledcAttachPin(4, 2);
    ledcWrite(2, pwm);
  }
  /* ---------------------------------------- */

  /* ---------------------------------------- Clean the result of reading the QR Code. */
  // Incoming Command = B,clr
  // B = Button
  // clr = Command to clean the results of reading the QR Code.
  if (resultData == "B") {
    resultData = getValue(getData, ',', 1);
    if (resultData == "clr") {
      QRCodeResult = "";
    }
  }
  /* ---------------------------------------- */

  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ qrcoderslt handler function to be called during GET or uri request. */
static esp_err_t qrcoderslt_handler(httpd_req_t* req) {
  if (QRCodeResult != "Decoding FAILED") QRCodeResultSend = QRCodeResult;
  httpd_resp_send(req, QRCodeResultSend.c_str(), HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ Subroutine for starting the web server / startCameraServer. */
void startCameraWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL
  };

  httpd_uri_t cmd_uri = {
    .uri = "/action",
    .method = HTTP_GET,
    .handler = cmd_handler,
    .user_ctx = NULL
  };

  httpd_uri_t qrcoderslt_uri = {
    .uri = "/getqrcodeval",
    .method = HTTP_GET,
    .handler = qrcoderslt_handler,
    .user_ctx = NULL
  };

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&index_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(index_httpd, &index_uri);
    httpd_register_uri_handler(index_httpd, &cmd_uri);
    httpd_register_uri_handler(index_httpd, &qrcoderslt_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }

  Serial.println();
  Serial.println("Camera Server started successfully");
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.localIP());
  Serial.println();
}
/* ________________________________________________________________________________ */

/*-------wifi setting for none hardcoding*/

bool checkHasWifiUserPass() {

  ssid = readingFile("/ssid.txt");
  if (ssid == "") {
    return false;
  }
  Serial.printf("reading info success:%s", ssid);
  password = readingFile("/password.txt");

  ip = readingFile("/ip.txt");

  if (ip == "") {
    return false;
  }
  gateway = readingFile("/gateway.txt");

  if (gateway == "") {
    return false;
  }
  return true;
}


bool connectToWifi(bool isserver) {
  Serial.println("befor check");
  
  if (ssid == "" || ip == "") {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  if (isserver) {
    WiFi.mode(WIFI_STA);
  }
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  if (isserver && !WiFi.config(localIP, localGateway, subnet)) {
    Serial.println("STA Failed to configure");
    return false;
  }
 
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.println("Connecting to Wifi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  // while (WiFi.status() != WL_CONNECTED) {
  //   currentMillis = millis();
  //   if (currentMillis - previousMillis >= interval) {
  //     Serial.println("Failed to connect.");
  //     return false;
  //   }
  //   Serial.println("try to connecting");
  // }
  
  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(LED_OnBoard, HIGH);
    delay(250);
    digitalWrite(LED_OnBoard, LOW);
    delay(250);
    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    if(connecting_process_timed_out == 0) {
      delay(1000);
      
      writingFile("/ssid.txt", "");
      writingFile("/password.txt", "");
      ssid="";
      password="";
      ESP.restart();
    }
  }
  Serial.println("Wifi connected with ip:");
  Serial.print(WiFi.localIP());
  Serial.println("Wifi Status is:");
  Serial.print(WiFi.status());
  return true;
}

const char index_html[] = R"rawliteral(
   <!DOCTYPE html>
<html>
<head>
  <title>ESP Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
  font-family: Arial, Helvetica, sans-serif; 
  display: inline-block; 
  text-align: center;
}

h1 {
  font-size: 1.8rem; 
  color: white;
}

p { 
  font-size: 1.4rem;
}

.topnav { 
  overflow: hidden; 
  background-color: #0A1128;
}

body {  
  margin: 0;
}

.content { 
  padding: 5%;
}

.card-grid { 
  max-width: 800px; 
  margin: 0 auto; 
  display: grid; 
  grid-gap: 2rem; 
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
}

.card { 
  background-color: white; 
  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
}

.card-title { 
  font-size: 1.2rem;
  font-weight: bold;
  color: #034078
}

input[type=submit] {
  border: none;
  color: #FEFCFB;
  background-color: #034078;
  padding: 15px 15px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  width: 100px;
  margin-right: 10px;
  border-radius: 4px;
  transition-duration: 0.4s;
  }

input[type=submit]:hover {
  background-color: #1282A2;
}

input[type=text], input[type=number], select {
  width: 50%;
  padding: 12px 20px;
  margin: 18px;
  display: inline-block;
  border: 1px solid #ccc;
  border-radius: 4px;
  box-sizing: border-box;
}

label {
  font-size: 1.2rem; 
}
.value{
  font-size: 1.2rem;
  color: #1282A2;  
}
.state {
  font-size: 1.2rem;
  color: #1282A2;
}
button {
  border: none;
  color: #FEFCFB;
  padding: 15px 32px;
  text-align: center;
  font-size: 16px;
  width: 100px;
  border-radius: 4px;
  transition-duration: 0.4s;
}
.button-on {
  background-color: #034078;
}
.button-on:hover {
  background-color: #1282A2;
}
.button-off {
  background-color: #858585;
}
.button-off:hover {
  background-color: #252524;
} 
  </style>
</head>
<body>
  <div class="topnav">
    <h1>WELCOME TO PEEKSELLER</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <label for="ip">IP Address</label>
            <input type="text" id ="ip" name="ip" value="192.168.1.200"><br>
            <label for="gateway">Gateway Address</label>
            <input type="text" id ="gateway" name="gateway" value="192.168.1.1"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";



String processor(const String& var) {
  delay(4000);
  Serial.println("callback is called");
  return String();
}

void initWifi() {
  Serial.println("Setting AP (Access Point)");
  WiFi.softAP("Peekseller", NULL);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address: ");
  Serial.println(IP);


  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
  });
  Serial.println("before ");
  server.serveStatic("/", SPIFFS, "/");
  Serial.println("After static");
  server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
    int params = request->params();

    for (int i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        // HTTP POST ssid value
        if (p->name() == PARAM_INP_1) {
          ssid = p->value().c_str();
          Serial.print("SSID set to: ");
          Serial.println(ssid);
        }
        // HTTP POST pass value
        if (p->name() == PARAM_INP_2) {
          password = p->value().c_str();
          Serial.print("Password set to: ");
          Serial.println(password);
        }
        // HTTP POST ip value
        if (p->name() == PARAM_INP_3) {
          ip = p->value().c_str();
          Serial.print("IP Address set to: ");
          Serial.println(ip);
        }
        // HTTP POST gateway value
        if (p->name() == PARAM_INP_4) {
          gateway = p->value().c_str();
          Serial.print("Gateway set to: ");
          Serial.println(gateway);
          // Write file to save value
          //writeFile(SPIFFS, gatewayPath, gateway.c_str());
        }
        //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }
    wifiSettingSaving();
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
    delay(3000);
    ESP.restart();
  });
  server.begin();
  Serial.println("fifi");
}


bool writingFile(String path, String data) {
  File wFile = SPIFFS.open(path, FILE_WRITE);
  if (!wFile) {
    Serial.println("There was an error openning the file for writing");
    return false;
  }

  if (wFile.print(data)) {
    Serial.print("File  written ");
  } else {
    Serial.print(" write failed ");
  }
  wFile.close();
  return true;
}
void wifiSettingSaving() {

  if (writingFile("/ssid.txt", ssid)) {
    Serial.println("ssid written");
  } else {
    Serial.print("ssid writing failed");
  }

  if (writingFile("/password.txt", password)) {
    Serial.println("password written");
  } else {
    Serial.print("password writing failed");
  }
  if (writingFile("/ip.txt", ip)) {
    Serial.println("ip written");
  } else {
    Serial.print("ip writing failed");
  }

  if (writingFile("/gateway.txt", gateway)) {
    Serial.println("gateway written");
  } else {
    Serial.print("gateway writing failed");
  }
}
String readingFile(String path) {
  File rFile = SPIFFS.open(path, "r");
  if (!rFile) {
    Serial.println("There was an error openning the file for reading");
    return "";
  }
  String info;
  while (rFile.available()) {
    info = rFile.readStringUntil('\n');
  }
  Serial.println("reading file from: " + path);
  Serial.println("information is:" + info);
  rFile.close();
  return info;
}


/* ________________________________________________________________________________ VOID SETUP() */
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println();
  /* ---------------------------------------- */

  pinMode(LED_OnBoard, OUTPUT);
  pinMode(LED_Green, OUTPUT);
  pinMode(LED_Blue, OUTPUT);

  /* ---------------------------------------- Camera configuration. */
  Serial.println("Start configuring and initializing the camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  Serial.println("Configure and initialize the camera successfully.");
  Serial.println();


  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occured while mounting SPIFFS");
    return;
  }
  if (!checkHasWifiUserPass()) {
    if (!connectToWifi(true)) {
      initWifi();
    }
    // Serial.println("do it");
  } else {

    if (connectToWifi(false)) {
      Serial.println("has wifi user");
      Serial.println("end of setup");



      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("------------");
      Serial.println("");
      startCameraWebServer();

      createTaskQRCodeReader();
    }
  }
}
/* ________________________________________________________________________________ */

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (ws_run == true) {
      if (get_wsLive_val == true) {
        last_wsLive_val = wsLive_val;
        get_wsLive_val = false;
      }

      get_wsLive_interval++;
      if (get_wsLive_interval > 2) {
        get_wsLive_interval = 0;
        get_wsLive_val = true;
        if (wsLive_val == last_wsLive_val) {
          ws_run = false;
          last_wsLive_val = 0;
          createTaskQRCodeReader();
        }
      }
    }
  }
  /* ---------------------------------------- */
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ create "QRCodeReader_Task" using the xTaskCreatePinnedToCore() function */
void createTaskQRCodeReader() {
  xTaskCreatePinnedToCore(
    QRCodeReader,        /* Task function. */
    "QRCodeReader_Task", /* name of task. */
    10000,               /* Stack size of task */
    NULL,                /* parameter of the task */
    1,                   /* priority of the task */
    &QRCodeReader_Task,  /* Task handle to keep track of created task */
    0);                  /* pin task to core 0 */
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ QRCodeReader() */
void QRCodeReader(void* pvParameters) {
  Serial.print("QRCodeReader running on core ");
  Serial.println(xPortGetCoreID());

  while (!ws_run) {
    camera_fb_t* fb = NULL;
    q = quirc_new();

    if (q == NULL) {
      Serial.print("can't create quirc object\r\n");
      continue;
    }

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed (QRCodeReader())");
      continue;
    }

    quirc_resize(q, fb->width, fb->height);
    image = quirc_begin(q, NULL, NULL);
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);

    int count = quirc_count(q);
    if (count > 0) {
      //Serial.println(count);
      quirc_extract(q, 0, &code);
      err = quirc_decode(&code, &data);

      if (err) {
        QRCodeResult = "Decoding FAILED";
        Serial.println(QRCodeResult);
      } else {
        Serial.printf("Decoding successful:\n");
        dumpData(&data);
      }
      Serial.println();
    }

    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;
    quirc_destroy(q);
  }
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ Function to display the results of reading the QR Code on the serial monitor. */
void dumpData(const struct quirc_data* data) {
  Serial.printf("-Version: %d\n", data->version);
  Serial.printf("-ECC level: %c\n", "MLHQ"[data->ecc_level]);
  Serial.printf("-Mask: %d\n", data->mask);
  Serial.printf("-Length: %d\n", data->payload_len);
  Serial.printf("-Payload: %s\n", data->payload);

  QRCodeResult = (const char*)data->payload;
  cmd_execution();
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ Subroutine to execute commands based on the results of reading the QR Code. */
void cmd_execution() {
  if (QRCodeResult == "LED BLUE ON") digitalWrite(LED_Blue, HIGH);
  if (QRCodeResult == "LED BLUE OFF") digitalWrite(LED_Blue, LOW);
  if (QRCodeResult == "LED GREEN ON") digitalWrite(LED_Green, HIGH);
  if (QRCodeResult == "LED GREEN OFF") digitalWrite(LED_Green, LOW);
}
/* ________________________________________________________________________________ */

/* ________________________________________________________________________________ String function to process the data command */
// I got this from : https://www.electroniclinic.com/reyax-lora-based-multiple-sensors-monitoring-using-arduino/
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}