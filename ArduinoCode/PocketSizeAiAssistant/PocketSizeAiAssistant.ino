#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/i2s.h"
#include <WiFiClientSecure.h>
 
const unsigned char icon[] PROGMEM = {
  0x00, 0x3e, 0x00, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x00, 0xe1, 0xe0, 0x00, 0x01, 0xc0, 0x7e, 0x00,
  0x03, 0x00, 0xff, 0xc0, 0x07, 0x03, 0xf1, 0xe0, 0x1e, 0x07, 0xc0, 0x70, 0x3e, 0x0e, 0x00, 0x30,
  0x3a, 0x1c, 0x30, 0x38, 0x62, 0x18, 0x78, 0x18, 0x62, 0x3b, 0xee, 0x18, 0xc3, 0x33, 0x83, 0x98,
  0xc3, 0x3f, 0xe1, 0xd8, 0xc1, 0x38, 0x30, 0x78, 0xc1, 0xb0, 0x3c, 0x38, 0xe1, 0xf0, 0x1e, 0x18,
  0x70, 0xf0, 0x16, 0x1c, 0x78, 0x70, 0x12, 0x0c, 0x7c, 0x18, 0x33, 0x0c, 0x6f, 0x0c, 0x73, 0x0c,
  0x67, 0xc7, 0xd3, 0x0c, 0x61, 0xff, 0x33, 0x18, 0x60, 0x1c, 0x73, 0x38, 0x70, 0x00, 0xe3, 0x70,
  0x30, 0x00, 0xc3, 0xe0, 0x38, 0x03, 0x83, 0xc0, 0x1e, 0x0f, 0x03, 0x80, 0x07, 0xfc, 0x07, 0x00,
  0x01, 0xf0, 0x0e, 0x00, 0x00, 0x1c, 0x1c, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x03, 0xe0, 0x00
};
 
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define BUTTON_PIN 2
#define I2S_BCLK 7  
#define I2S_LRC 6    
#define I2S_DIN 4
#define I2S_DOUT 0   

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define MAX_RECORD_TIME_MS 3000
#define PLAYBACK_BUFFER_SIZE 1024
 
const char* ssid = "YOUR Wifi SSID";
const char* password = "Your Wifi PASSWORD";
String server_url = "https://UserID-ServerName.hf.space/process_audio";
String server_base_url = "https://UserID-ServerName.hf.space";
 
uint8_t* record_buffer = nullptr;
size_t record_buffer_size = 0;
size_t record_position = 0;
bool is_recording = false;
unsigned long record_start_time = 0;
bool i2s_driver_installed = false;
bool is_playing = false;

WiFiClientSecure client;

void updateDisplay(String text, int size = 1) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.drawBitmap(48, 25, icon, 32, 32, SSD1306_WHITE);
  display.display();
}

void deinit_i2s() {
  if (i2s_driver_installed) {
    i2s_zero_dma_buffer(I2S_NUM_0);
    delay(50);
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_driver_installed = false;
    Serial.println("I2S Driver Uninstalled");
    delay(50);
  }
}

void init_i2s_rx() {
  deinit_i2s();
  Serial.println("Initializing I2S RX");
  i2s_config_t rec_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 512
  };
  const i2s_pin_config_t rec_pin = { 
    .bck_io_num = I2S_BCLK, 
    .ws_io_num = I2S_LRC, 
    .data_out_num = I2S_PIN_NO_CHANGE, 
    .data_in_num = I2S_DIN };
  esp_err_t err_install = i2s_driver_install(I2S_NUM_0, &rec_cfg, 0, NULL);
  if (err_install != ESP_OK) {
    Serial.printf("!!! I2S RX driver install failed: %d\n", err_install);
    return;  
  }
  esp_err_t err_pin = i2s_set_pin(I2S_NUM_0, &rec_pin);
  if (err_pin != ESP_OK) {
    Serial.printf("!!! I2S RX pin set failed: %d\n", err_pin);
    i2s_driver_uninstall(I2S_NUM_0); 
    return;
  }
  i2s_driver_installed = true;
}

void init_i2s_tx() {
  deinit_i2s();
  Serial.println("Initializing I2S TX");
  i2s_config_t play_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,  
    .dma_buf_len = 1024, 
    .use_apll = false,   
    .tx_desc_auto_clear = true
  };
  const i2s_pin_config_t play_pin = { 
    .bck_io_num = I2S_BCLK, 
  .ws_io_num = I2S_LRC, 
  .data_out_num = I2S_DOUT, 
  .data_in_num = I2S_PIN_NO_CHANGE };
  esp_err_t err_install = i2s_driver_install(I2S_NUM_0, &play_cfg, 0, NULL);
  if (err_install != ESP_OK) {
    Serial.printf("!!! I2S TX driver install failed: %d\n", err_install);
    return;
  }
  esp_err_t err_pin = i2s_set_pin(I2S_NUM_0, &play_pin);
  if (err_pin != ESP_OK) {
    Serial.printf("!!! I2S TX pin set failed: %d\n", err_pin);
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  } 
  i2s_driver_installed = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  client.setInsecure(); 

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    delay(1000);
    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  }

  updateDisplay("Starting...");

  WiFi.begin(ssid, password);
  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    timeout--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    updateDisplay("How Can I Help You", 1);
  } else {
    updateDisplay("WiFi Error!", 1);
  }
  init_i2s_rx();  
} 

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && !is_recording && !is_playing) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) { startRecording(); }
  }
  if (digitalRead(BUTTON_PIN) == HIGH && is_recording) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == HIGH) {
      stopRecording();
      processAudio();
    }
  }
  if (is_recording) {
    recordAudioData();
    if (millis() - record_start_time > MAX_RECORD_TIME_MS) {
      stopRecording();
      processAudio();
    }
  }
  delay(10);
}

void startRecording() {
  init_i2s_rx();
  is_recording = true;
  record_start_time = millis();

  record_buffer_size = (MAX_RECORD_TIME_MS / 1000) * SAMPLE_RATE * (BITS_PER_SAMPLE / 8) + 44;
  if (record_buffer != nullptr) { free(record_buffer); } 
  record_buffer = (uint8_t*)malloc(record_buffer_size);

  if (!record_buffer) {
    updateDisplay("Memory Error!", 1);
    is_recording = false;
    delay(2000);
    updateDisplay("How Can I Help You", 1);
    return;
  }
  record_position = 44;
  updateDisplay("Recording...", 1);
}

void recordAudioData() {
  size_t bytes_read;
  if (record_position < record_buffer_size - 1024) {
    esp_err_t err = i2s_read(I2S_NUM_0, record_buffer + record_position, 1024, &bytes_read, 100 / portTICK_PERIOD_MS);
    if (err == ESP_OK && bytes_read > 0) {
      record_position += bytes_read;
    } else if (err != ESP_ERR_TIMEOUT) {
      Serial.printf("I2S Read Error: %d\n", err);
    }
  }
}

void stopRecording() {
  is_recording = false;
  Serial.println("Recording stopped.");
  if (record_position > 44) {
    createWavHeader(record_buffer, record_position - 44);
  }
}

void processAudio() {
  if (!record_buffer || record_position <= 44) {
    Serial.println("No audio data.");
    return;
  }
  deinit_i2s();
  bool success = sendAudioToServer();
  if (!success) {
    Serial.println("Processing failed.");
    init_i2s_rx();
    updateDisplay("Unsuccessful!", 1);
    delay(2000);
    updateDisplay("How Can I Help You", 1);
  }
  if (record_buffer) {
    free(record_buffer);
    record_buffer = nullptr;
  }
  record_position = 0;
}


bool sendAudioToServer() {
  HTTPClient http; 
  if (!http.begin(client, server_url)) {
    Serial.println("POST begin failed");
    return false;
  }

  http.addHeader("Content-Type", "audio/wav");
  http.setTimeout(20000);

  Serial.printf("Sending %zu bytes to server...\n", record_position);  
  int httpCode = http.POST(record_buffer, record_position);

  if (httpCode == 200) {
    String response = http.getString();
    http.end();
    Serial.println("POST success, response:");
    Serial.println(response);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print("JSON Parse Error: ");
      Serial.println(error.c_str());
      return false;
    }

    if (doc.containsKey("stream_url") && doc.containsKey("message")) {
      String stream_url = doc["stream_url"];
      String message = doc["message"];
      updateDisplay(message, 1); 
      return streamAndPlayAudioWithHTTPClient(server_base_url + stream_url);
    } else {
      Serial.println("JSON missing fields");
      return false;
    }
  } else {
    Serial.printf("POST failed, error: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}
 
bool streamAndPlayAudioWithHTTPClient(const String& url) {
  is_playing = true;
  HTTPClient http;

  Serial.print("Connecting to stream: ");
  Serial.println(url); 
  if (!http.begin(client, url)) {
    Serial.println("GET begin failed");
    is_playing = false;
    return false;
  }

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("GET failed, error: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    is_playing = false;
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("Failed to get stream pointer");
    http.end();
    is_playing = false;
    return false;
  }

  init_i2s_tx();        
  if (!i2s_driver_installed) {  
    Serial.println("!!! Failed to init I2S TX for playback");
    http.end();
    is_playing = false;
    return false;
  }
  delay(100); 
 
  size_t header_bytes_read = 0;
  unsigned long header_start_time = millis();
  while (header_bytes_read < 44 && millis() - header_start_time < 5000) { 
    int bytes_available = stream->available();
    if (bytes_available > 0) {
      int bytes_to_read = min((int)(44 - header_bytes_read), bytes_available);
      uint8_t dummy_buffer[bytes_to_read];
      int read_len = stream->read(dummy_buffer, bytes_to_read);
      if (read_len > 0) {
        header_bytes_read += read_len;
      } else if (read_len < 0) {
        Serial.println("!!! Header read error");
        break;  
      }
    } else if (!stream->connected()) {
      Serial.println("!!! Stream disconnected during header read");
      break;  
    } else {
      delay(5); 
    }
  }
  if (header_bytes_read < 44) {
    Serial.println("!!! Failed to read full WAV header or timeout.");
    http.end();
    deinit_i2s();  
    init_i2s_rx();  
    is_playing = false;
    return false;
  }
  Serial.println("WAV header skipped.");


  uint8_t playback_buffer[PLAYBACK_BUFFER_SIZE];
  size_t total_bytes_written = 0;
  unsigned long lastDataTime = millis();
  const unsigned long streamTimeoutMs = 2000;  

  Serial.println("Starting audio playback..."); 
  while (millis() - lastDataTime < streamTimeoutMs) {
    if (!stream->connected() && !stream->available()) {
      Serial.println("Stream disconnected and no data left.");
      break; 
    }

    int len = stream->readBytes(playback_buffer, PLAYBACK_BUFFER_SIZE);
    if (len > 0) {
      lastDataTime = millis(); 
      size_t bytes_written = 0;
      esp_err_t write_err = i2s_write(I2S_NUM_0, playback_buffer, len, &bytes_written, 500 / portTICK_PERIOD_MS); 
      if (write_err != ESP_OK) {
        Serial.printf("!!! I2S Write Error: %d, bytes written: %zu\n", write_err, bytes_written); 
      }
      total_bytes_written += bytes_written;
      if (bytes_written != len) {
        Serial.printf("!!! I2S Write Warning: Tried %d, wrote %zu\n", len, bytes_written);
      }
    } else { 
      delay(10);
    }
  }

  if (millis() - lastDataTime >= streamTimeoutMs) {
    Serial.println("Stream finished due to inactivity timeout.");
  } else {
    Serial.println("Stream finished.");
  }
 
  Serial.println("Waiting for I2S buffer to empty...");
  esp_err_t zero_err = i2s_zero_dma_buffer(I2S_NUM_0);
  if (zero_err != ESP_OK) {
    Serial.printf("!!! I2S zero DMA buffer error: %d\n", zero_err);
  }
  delay(300);   

  http.end();  

  deinit_i2s();  
  init_i2s_rx();  

  updateDisplay("How Can I Help You", 1);
  is_playing = false;

  Serial.printf("Total bytes played: %zu\n", total_bytes_written);
  return total_bytes_written > 0;
}
 
void createWavHeader(byte* header, int wavDataSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavDataSize + 36;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 16;
  header[17] = 0;
  header[18] = 0;
  header[19] = 0;
  header[20] = 1;
  header[21] = 0;
  header[22] = 1;
  header[23] = 0;  // Mono
  header[24] = (byte)(SAMPLE_RATE & 0xFF);
  header[25] = (byte)((SAMPLE_RATE >> 8) & 0xFF);
  header[26] = 0;
  header[27] = 0;
  unsigned int byteRate = SAMPLE_RATE * 1 * BITS_PER_SAMPLE / 8;  // 1 = Mono
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = 0;
  header[31] = 0;
  header[32] = (1 * BITS_PER_SAMPLE / 8);
  header[33] = 0;  // Block align
  header[34] = BITS_PER_SAMPLE;
  header[35] = 0;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavDataSize & 0xFF);
  header[41] = (byte)((wavDataSize >> 8) & 0xFF);
  header[42] = (byte)((wavDataSize >> 16) & 0xFF);
  header[43] = (byte)((wavDataSize >> 24) & 0xFF);
}
