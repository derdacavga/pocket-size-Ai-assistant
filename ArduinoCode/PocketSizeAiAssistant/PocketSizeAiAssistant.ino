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
#define PLAYBACK_BUFFER_SIZE 512

const char* ssid = "YOUR SSID";
const char* password = "YOUR SSID PASSWORD";
String server_url = "https://USERID-SERVERNAME.hf.space/process_audio";
String server_base_url = "https://USERID-SERVERNAME.hf.space";

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
    delay(50);
  }
}

void init_i2s_rx() {
  if (i2s_driver_installed) deinit_i2s();
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
    .data_in_num = I2S_DIN
  };
  if (i2s_driver_install(I2S_NUM_0, &rec_cfg, 0, NULL) != ESP_OK) {
    updateDisplay("I2S RX install Failed", 1);
    return;
  }
  if (i2s_set_pin(I2S_NUM_0, &rec_pin) != ESP_OK) {
    updateDisplay("I2S RX Failed", 1);
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  }
  i2s_driver_installed = true;
}

void init_i2s_tx() {
  if (i2s_driver_installed) deinit_i2s();

  i2s_config_t play_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t play_pin = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t install_result = i2s_driver_install(I2S_NUM_0, &play_cfg, 0, NULL);
  if (install_result != ESP_OK) {
    updateDisplay("I2S TX install Failed", 1);
    return;
  }

  esp_err_t pin_result = i2s_set_pin(I2S_NUM_0, &play_pin);
  if (pin_result != ESP_OK) {
    updateDisplay("I2S TX Failed", 1);
    i2s_driver_uninstall(I2S_NUM_0);
    return;
  }
  esp_err_t rate_result = i2s_set_sample_rates(I2S_NUM_0, SAMPLE_RATE);

  i2s_driver_installed = true;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  client.setInsecure();
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
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
  pinMode(I2S_DOUT, OUTPUT);
  digitalWrite(I2S_DOUT, LOW);
  delay(100);

  init_i2s_rx();
}

void loop() {
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    if (WiFi.status() != WL_CONNECTED) {
      updateDisplay("Wifi Reconnecting", 1);
      WiFi.reconnect();
    } else {
    }
    lastWiFiCheck = millis();
  }

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

  if (record_buffer != nullptr) {
    free(record_buffer);
    record_buffer = nullptr;
  }

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
    if (err == ESP_OK && bytes_read > 0) { record_position += bytes_read; }
  }
}

void stopRecording() {
  is_recording = false;
  if (record_position > 44) { createWavHeader(record_buffer, record_position - 44); }
}

void processAudio() {
  if (!record_buffer || record_position <= 44) { return; }
  deinit_i2s();
  bool success = sendAudioToServer();
  if (!success) {
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
  updateDisplay("Thinking...", 1);
  delay(50);
  HTTPClient http;
  if (!http.begin(client, server_url)) { return false; }
  http.addHeader("Content-Type", "audio/wav");
  http.setTimeout(20000);
  int httpCode = http.POST(record_buffer, record_position);
  if (httpCode == 200) {
    String response = http.getString();
    http.end();
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, response) == DeserializationError::Ok) {
      if (doc.containsKey("stream_url") && doc.containsKey("message")) {
        String stream_url = doc["stream_url"];
        String message = doc["message"];
        updateDisplay(message, 1);
        return streamAndPlayAudioWithHTTPClient(server_base_url + stream_url);
      } else {
        return false;
      }
    } else {
      return false;
    }
  } else {
    http.end();
    return false;
  }
}

bool streamAndPlayAudioWithHTTPClient(const String& url) {
  is_playing = true;
  HTTPClient http;

  http.setTimeout(30000);

  if (!http.begin(client, url)) {
    updateDisplay("GET begin failed", 1);
    is_playing = false;
    return false;
  }

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Connection", "keep-alive");

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    updateDisplay("GET failed: %d\n", 1);
    http.end();
    is_playing = false;
    return false;
  }

  int contentLength = http.getSize();

  if (contentLength <= 0 || contentLength > 500000) {
    updateDisplay("!!! Invalid content length", 1);
    http.end();
    is_playing = false;
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    updateDisplay("Stream ptr null", 1);
    http.end();
    is_playing = false;
    return false;
  }

  init_i2s_tx();
  if (!i2s_driver_installed) {
    updateDisplay("!!! I2S TX init failed", 1);
    http.end();
    is_playing = false;
    return false;
  }

  i2s_start(I2S_NUM_0);
  delay(100);

  const size_t HEADER_BUFFER_SIZE = 256;
  uint8_t* header_buffer = (uint8_t*)malloc(HEADER_BUFFER_SIZE);

  if (!header_buffer) {
    updateDisplay("!!! Header buffer allocation failed", 1);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  size_t header_bytes_read = 0;
  unsigned long header_start = millis();

  while (header_bytes_read < HEADER_BUFFER_SIZE && millis() - header_start < 10000) {
    int available = stream->available();
    if (available > 0) {
      int to_read = min((int)(HEADER_BUFFER_SIZE - header_bytes_read), available);
      int actual = stream->read(header_buffer + header_bytes_read, to_read);
      if (actual > 0) {
        header_bytes_read += actual;
        if (header_bytes_read >= 44) break;
      } else if (actual < 0) break;
    } else {
      if (!stream->connected()) break;
      delay(5);
    }
    yield();  // Watchdog
  }

  if (header_bytes_read < 44) {
    free(header_buffer);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  if (header_buffer[0] != 'R' || header_buffer[1] != 'I' || header_buffer[2] != 'F' || header_buffer[3] != 'F') {
    updateDisplay("!!! Not a RIFF file", 1);
    free(header_buffer);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  uint32_t file_size = header_buffer[4] | (header_buffer[5] << 8) | (header_buffer[6] << 16) | (header_buffer[7] << 24);

  if (header_buffer[8] != 'W' || header_buffer[9] != 'A' || header_buffer[10] != 'V' || header_buffer[11] != 'E') {
    updateDisplay("!!! Not a WAVE file", 1);
    free(header_buffer);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  size_t pos = 12;
  uint32_t sample_rate = 0;
  uint16_t num_channels = 0;
  uint16_t bits_per_sample = 0;
  size_t data_offset = 0;
  uint32_t data_size = 0;

  while (pos + 8 <= header_bytes_read) {
    char chunk_id[5] = { 0 };
    memcpy(chunk_id, header_buffer + pos, 4);
    pos += 4;

    uint32_t chunk_size = header_buffer[pos] | (header_buffer[pos + 1] << 8) | (header_buffer[pos + 2] << 16) | (header_buffer[pos + 3] << 24);
    pos += 4;

    if (strcmp(chunk_id, "fmt ") == 0) {
      if (pos + 16 <= header_bytes_read) {
        uint16_t audio_format = header_buffer[pos] | (header_buffer[pos + 1] << 8);
        num_channels = header_buffer[pos + 2] | (header_buffer[pos + 3] << 8);
        sample_rate = header_buffer[pos + 4] | (header_buffer[pos + 5] << 8) | (header_buffer[pos + 6] << 16) | (header_buffer[pos + 7] << 24);
        bits_per_sample = header_buffer[pos + 14] | (header_buffer[pos + 15] << 8);
      }
      pos += chunk_size;

    } else if (strcmp(chunk_id, "data") == 0) {
      data_offset = pos;
      data_size = chunk_size;
      break;

    } else {
      pos += chunk_size;
    }

    if (chunk_size % 2 == 1) pos++;
    yield();
  }

  if (data_offset == 0 || data_size == 0) {
    updateDisplay("!!! 'data' chunk not found", 1);
    free(header_buffer);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  if (sample_rate != SAMPLE_RATE) {
    i2s_set_sample_rates(I2S_NUM_0, sample_rate);
  }

  size_t total_bytes_written = 0;

  if (data_offset < header_bytes_read) {
    size_t audio_in_header = header_bytes_read - data_offset;

    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, header_buffer + data_offset, audio_in_header,
              &bytes_written, portMAX_DELAY);
    total_bytes_written += bytes_written;
  }

  free(header_buffer);
  header_buffer = nullptr;

  const size_t PLAY_BUFFER_SIZE = 512;
  uint8_t* playback_buffer = (uint8_t*)malloc(PLAY_BUFFER_SIZE);

  if (!playback_buffer) {
    updateDisplay("!!! Playback buffer allocation failed", 1);
    http.end();
    deinit_i2s();
    init_i2s_rx();
    is_playing = false;
    return false;
  }

  size_t total_bytes_read = total_bytes_written;
  unsigned long lastDataTime = millis();
  unsigned long lastYield = millis();
  const unsigned long dataTimeoutMs = 5000;

  while (total_bytes_read < data_size) {
    if (millis() - lastYield > 100) {
      yield();
      lastYield = millis();
    }

    if (millis() - lastDataTime > dataTimeoutMs) {
      updateDisplay("Timeout", 1);
      break;
    }

    if (!stream->connected() && stream->available() == 0) {
      break;
    }

    int available = stream->available();

    if (available > 0) {
      size_t remaining = data_size - total_bytes_read;
      int to_read = min(available, (int)PLAY_BUFFER_SIZE);
      to_read = min(to_read, (int)remaining);

      int bytes_read = stream->readBytes(playback_buffer, to_read);

      if (bytes_read > 0) {
        lastDataTime = millis();
        total_bytes_read += bytes_read;

        size_t bytes_written = 0;
        esp_err_t result = i2s_write(I2S_NUM_0, playback_buffer, bytes_read,
                                     &bytes_written, 1000 / portTICK_PERIOD_MS);

        if (result != ESP_OK) {
          updateDisplay("!!! I2S error: %d\n", 1);
          break;
        }

        total_bytes_written += bytes_written;

        int progress = (total_bytes_read * 100) / data_size;
        static int last_prog = -1;
        if (progress / 20 != last_prog / 20) {
          last_prog = progress;
        }
      } else if (bytes_read < 0) {
        updateDisplay("!!! Read error", 1);
        break;
      }
    } else {
      delay(2);
    }
  }

  free(playback_buffer);

  delay(150);
  i2s_stop(I2S_NUM_0);
  delay(50);
  i2s_zero_dma_buffer(I2S_NUM_0);
  delay(250);

  http.end();
  deinit_i2s();
  delay(50);
  init_i2s_rx();

  updateDisplay("How Can I Help You", 1);
  is_playing = false;

  bool success = total_bytes_written > 0 && total_bytes_read >= data_size * 0.95;

  return success;
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
  header[23] = 0;
  header[24] = (byte)(SAMPLE_RATE & 0xFF);
  header[25] = (byte)((SAMPLE_RATE >> 8) & 0xFF);
  header[26] = 0;
  header[27] = 0;
  unsigned int byteRate = SAMPLE_RATE * 1 * BITS_PER_SAMPLE / 8;
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = 0;
  header[31] = 0;
  header[32] = (1 * BITS_PER_SAMPLE / 8);
  header[33] = 0;
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
