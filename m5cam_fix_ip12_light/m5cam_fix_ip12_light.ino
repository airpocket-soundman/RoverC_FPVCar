/*
 * M5CAMERAをネットワークカメラとして使うプログラム
 * 
 * スケッチ例>ESP32>Camera>CameraWebServerで表示されるサンプルコードを元に
 * ビデオストリーミングに不必要な処理を外してスマートコンフィグに対応させました。
 * 
 * 作成者：伊藤浩之 (Hiroyuki ITO)
 * 作成日：2020年5月3日
 * ブログ：https://intellectualcuriosity.hatenablog.com/
 * このプログラムはパブリックドメインソフトウェアです。
 */
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
 
#define CAMERA_VFLIP    0               // カメラの設置向き（本体のM5文字の向き） 1:正常 0:逆さ
#define FRAME_SIZE      FRAMESIZE_VGA   // サイズ：FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
 
// スマートコンフィグが使えないときは、以下の2行のコメントを外してSSIDとパスワードを設定します
#define WIFI_SSID       "raspberry"
#define WIFI_PASS       "minoru0869553434"

IPAddress ip(192, 168, 4, 12);           // for fixed IP Address
IPAddress gateway(192,168, 4, 1);        //
IPAddress subnet(255, 255, 255, 0);      //
IPAddress DNS(192, 168, 4, 1);          //

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
 
httpd_handle_t stream_httpd = NULL;
 
// カメラの初期化処理
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer =   LEDC_TIMER_0;
  config.pin_d0 =       32;             // Y2_GPIO_NUM
  config.pin_d1 =       35;             // Y3_GPIO_NUM
  config.pin_d2 =       34;             // Y4_GPIO_NUM
  config.pin_d3 =        5;             // Y5_GPIO_NUM
  config.pin_d4 =       39;             // Y6_GPIO_NUM
  config.pin_d5 =       18;             // Y7_GPIO_NUM
  config.pin_d6 =       36;             // Y8_GPIO_NUM
  config.pin_d7 =       19;             // Y9_GPIO_NUM
  config.pin_xclk =     27;             // XCLK_GPIO_NUM
  config.pin_pclk =     21;             // PCLK_GPIO_NUM
  config.pin_vsync =    25;             // VSYNC_GPIO_NUM
  config.pin_href =     26;             // HREF_GPIO_NUM
  config.pin_sscb_sda = 22;             // SIOD_GPIO_NUM
  config.pin_sscb_scl = 23;             // SIOC_GPIO_NUM
  config.pin_pwdn =     -1;             // PWDN_GPIO_NUM
  config.pin_reset =    15;             // RESET_GPIO_NUM
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size =   FRAME_SIZE;
  config.jpeg_quality = 10;
  config.fb_count =      2;
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, CAMERA_VFLIP);
  s->set_hmirror(s, 0);
}
 
// ストリーミングハンドラー
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];
 
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
 
  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 400) {
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
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
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
  }
  return res;
}
 
// ビデオストリーミングの開始
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
 
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
 
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}
 
// 接続処理
boolean connect() {
  if (WiFi.status() != WL_CONNECTED) {  // WiFiに繋がっているか？
    #if defined(WIFI_SSID)              // WIFI_SSIDが定義されているとき
      WiFi.begin(WIFI_SSID, WIFI_PASS); // WiFiに接続
    #else
      WiFi.begin();                     // WiFiに前回の設定で接続
    #endif
    Serial.print("Connecting");
    for (int i=0; WiFi.status() != WL_CONNECTED; i++) {
      Serial.print(".");
      delay(500);
      if (i > 20) {                     // 10秒経ったか（500ms * 20回）
        return false;
      }
    }
    Serial.println(String("\nConnected to ") + WiFi.SSID());
  }
  return true;
}
 
// 初期化時の接続処理
void connectInit() {
  WiFi.mode(WIFI_STA);                  // WiFiをステーションモードに設定
  if (!connect()) {                     // 接続処理
    WiFi.beginSmartConfig();            // スマートコンフィグを実行
    Serial.print("\nSmartConfig Start!");
    while(!WiFi.smartConfigDone()) {    // スマートコンフィグの完了確認
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nSmartConfig received.");
    Serial.println(String("Connected to ") + WiFi.SSID());
  }
}
 
void setup() {
  Serial.begin(115200);
  initCamera();                         // カメラの初期化処理
  WiFi.config(ip, gateway, subnet, DNS);   // Set fixed IP address
  connectInit();                        // 初期化時の接続処理 
 
  startCameraServer();                  // ビデオストリーミングの開始
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}
 
void loop() {
  while (!connect());                   // WiFiの接続確認
  delay(1000);
}
