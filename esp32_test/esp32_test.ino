#include <ArduinoJson.h>
#include <IRremote.hpp>
#include <Preferences.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include "config.h"

const uint32_t BAUD_RATE = 115200;
const int LED_PIN = 5;
const int BUTTON_PIN = 6;
const int IR_RECEIVE_PIN = 4;
const uint32_t DEBOUNCE_MS = 35;
const uint32_t IR_REPEAT_GUARD_MS = 140;
const uint32_t WIFI_RETRY_MS = 10000;
const bool NEC_CODE_0x16_IS_RIGHT = false;  // false = ENTER, true = RIGHT

const char* WIFI_SSID = WIFI_SSID_VALUE;
const char* WIFI_PASSWORD = WIFI_PASSWORD_VALUE;
const char* TV_IP = TV_IP_VALUE;
const uint16_t TV_PORT = TV_PORT_VALUE;
const char* DEFAULT_ONEPLAY_APP_ID = ONEPLAY_APP_ID_VALUE;

const char REGISTRATION_PAYLOAD[] PROGMEM = R"json({
  "id":"register_0",
  "type":"register",
  "payload":{
    "forcePairing":false,
    "pairingType":"PROMPT",
    "manifest":{
      "manifestVersion":1,
      "appVersion":"1.1",
      "signed":{
        "created":"20140509",
        "appId":"com.lge.test",
        "vendorId":"com.lge",
        "localizedAppNames":{"":"LG Remote App"},
        "localizedVendorNames":{"":"LG Electronics"},
        "permissions":[
          "TEST_SECURE",
          "CONTROL_INPUT_TEXT",
          "CONTROL_MOUSE_AND_KEYBOARD",
          "READ_INSTALLED_APPS",
          "READ_LGE_SDX",
          "READ_NOTIFICATIONS",
          "SEARCH",
          "WRITE_SETTINGS",
          "WRITE_NOTIFICATION_ALERT",
          "CONTROL_POWER",
          "READ_CURRENT_CHANNEL",
          "READ_RUNNING_APPS",
          "READ_UPDATE_INFO",
          "UPDATE_FROM_REMOTE_APP",
          "READ_LGE_TV_INPUT_EVENTS",
          "READ_TV_CURRENT_TIME"
        ],
        "serial":"2f930e2d2cfe083771f68e4fe7bb07"
      },
      "permissions":[
        "LAUNCH",
        "LAUNCH_WEBAPP",
        "APP_TO_APP",
        "CLOSE",
        "TEST_OPEN",
        "TEST_PROTECTED",
        "CONTROL_AUDIO",
        "CONTROL_DISPLAY",
        "CONTROL_INPUT_JOYSTICK",
        "CONTROL_INPUT_MEDIA_RECORDING",
        "CONTROL_INPUT_MEDIA_PLAYBACK",
        "CONTROL_INPUT_TV",
        "CONTROL_POWER",
        "READ_APP_STATUS",
        "READ_CURRENT_CHANNEL",
        "READ_INPUT_DEVICE_LIST",
        "READ_NETWORK_STATE",
        "READ_RUNNING_APPS",
        "READ_TV_CHANNEL_LIST",
        "WRITE_NOTIFICATION_TOAST",
        "READ_POWER_STATE",
        "READ_COUNTRY_INFO",
        "READ_SETTINGS",
        "CONTROL_TV_SCREEN",
        "CONTROL_TV_STANBY",
        "CONTROL_FAVORITE_GROUP",
        "CONTROL_USER_INFO",
        "CHECK_BLUETOOTH_DEVICE",
        "CONTROL_BLUETOOTH",
        "CONTROL_TIMER_INFO",
        "STB_INTERNAL_CONNECTION",
        "CONTROL_RECORDING",
        "READ_RECORDING_STATE",
        "WRITE_RECORDING_LIST",
        "READ_RECORDING_LIST",
        "READ_RECORDING_SCHEDULE",
        "WRITE_RECORDING_SCHEDULE",
        "READ_STORAGE_DEVICE_LIST",
        "READ_TV_PROGRAM_INFO",
        "CONTROL_BOX_CHANNEL",
        "READ_TV_ACR_AUTH_TOKEN",
        "READ_TV_CONTENT_STATE",
        "READ_TV_CURRENT_TIME",
        "ADD_LAUNCHER_CHANNEL",
        "SET_CHANNEL_SKIP",
        "RELEASE_CHANNEL_SKIP",
        "CONTROL_CHANNEL_BLOCK",
        "DELETE_SELECT_CHANNEL",
        "CONTROL_CHANNEL_GROUP",
        "SCAN_TV_CHANNELS",
        "CONTROL_TV_POWER",
        "CONTROL_WOL"
      ],
      "signatures":[
        {
          "signatureVersion":1,
          "signature":"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw=="
        }
      ]
    }
  }
})json";

WebSocketsClient ws;
WebSocketsClient pointerWs;
Preferences prefs;
String clientKey;
String detectedOnePlayAppId;

bool wsConnected = false;
bool pointerWsConnected = false;
bool tvRegistered = false;
bool listRequested = false;
bool pendingLaunch = false;
bool ledState = false;
bool appInfoRequested = false;
bool pointerSocketRequested = false;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
uint32_t lastDebounceMs = 0;
uint32_t lastWifiRetryMs = 0;
uint32_t lastIrActionMs = 0;

bool containsIgnoreCase(const String& haystack, const String& needle) {
  String h = haystack;
  String n = needle;
  h.toLowerCase();
  n.toLowerCase();
  return h.indexOf(n) >= 0;
}

bool parseWebSocketUrl(const String& url, bool& useSsl, String& host, uint16_t& port, String& path) {
  const int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) {
    return false;
  }

  const String scheme = url.substring(0, schemeEnd);
  useSsl = scheme.equalsIgnoreCase("wss");

  const String rest = url.substring(schemeEnd + 3);
  const int pathStart = rest.indexOf('/');
  const String hostPort = pathStart >= 0 ? rest.substring(0, pathStart) : rest;
  path = pathStart >= 0 ? rest.substring(pathStart) : "/";

  if (hostPort.length() == 0) {
    return false;
  }

  const int colonPos = hostPort.lastIndexOf(':');
  if (colonPos >= 0) {
    host = hostPort.substring(0, colonPos);
    port = (uint16_t)hostPort.substring(colonPos + 1).toInt();
  } else {
    host = hostPort;
    port = useSsl ? 3001 : 3000;
  }

  return host.length() > 0 && port > 0;
}

bool isNecProtocol(decode_type_t protocol) {
  return protocol == NEC || protocol == NEC2;
}

void setLed(bool on) {
  ledState = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void sendJson(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  ws.sendTXT(out);
}

void sendSimpleRequest(const char* requestId, const char* uri) {
  if (!tvRegistered) {
    Serial.print("TV not registered, skipped request: ");
    Serial.println(requestId);
    return;
  }

  DynamicJsonDocument doc(256);
  doc["id"] = requestId;
  doc["type"] = "request";
  doc["uri"] = uri;
  sendJson(doc);
}

void requestPointerSocket() {
  if (!tvRegistered) {
    return;
  }

  DynamicJsonDocument doc(384);
  doc["id"] = "pointer_socket";
  doc["type"] = "request";
  doc["uri"] = "ssap://com.webos.service.networkinput/getPointerInputSocket";
  sendJson(doc);
  pointerSocketRequested = true;
  Serial.println("Requested pointer input socket");
}

void connectPointerSocket(const String& socketPath) {
  bool useSsl = true;
  String host;
  String path;
  uint16_t port = 0;

  if (!parseWebSocketUrl(socketPath, useSsl, host, port, path)) {
    Serial.print("Failed to parse pointer socket URL: ");
    Serial.println(socketPath);
    return;
  }

  pointerWsConnected = false;
  if (useSsl) {
    pointerWs.beginSSL(host.c_str(), port, path.c_str());
  } else {
    pointerWs.begin(host.c_str(), port, path.c_str());
  }
  pointerWs.setReconnectInterval(5000);
  pointerWs.enableHeartbeat(15000, 3000, 2);

  Serial.print("Connecting pointer socket: ");
  Serial.println(socketPath);
}

void sendPointerButton(const char* buttonName) {
  if (!pointerWsConnected) {
    Serial.print("Pointer socket not ready, dropped button: ");
    Serial.println(buttonName);
    if (tvRegistered && !pointerSocketRequested) {
      requestPointerSocket();
    }
    return;
  }

  String payload = "type:button\nname:";
  payload += buttonName;
  payload += "\n\n";
  pointerWs.sendTXT(payload);
  Serial.print("Sent TV button: ");
  Serial.println(buttonName);
}

void requestLaunchPoints() {
  DynamicJsonDocument doc(512);
  doc["id"] = "list_apps";
  doc["type"] = "request";
  doc["uri"] = "ssap://com.webos.applicationManager/listLaunchPoints";
  sendJson(doc);
  Serial.println("Requested app list from TV");
}

void requestCurrentAppInfo() {
  DynamicJsonDocument doc1(512);
  doc1["id"] = "current_app";
  doc1["type"] = "request";
  doc1["uri"] = "ssap://com.webos.applicationManager/getForegroundAppInfo";
  sendJson(doc1);
  Serial.println("Requested foreground app info");

  DynamicJsonDocument doc2(512);
  doc2["id"] = "current_app_legacy";
  doc2["type"] = "request";
  doc2["uri"] = "ssap://com.webos.applicationManager/getCurrentAppInfo";
  sendJson(doc2);
  Serial.println("Requested current app info (legacy)");

  DynamicJsonDocument doc3(512);
  doc3["id"] = "current_app_service";
  doc3["type"] = "request";
  doc3["uri"] = "ssap://com.webos.service.applicationmanager/getForegroundAppInfo";
  sendJson(doc3);
  Serial.println("Requested foreground app info (service)");

  DynamicJsonDocument doc4(512);
  doc4["id"] = "current_app_launcher";
  doc4["type"] = "request";
  doc4["uri"] = "ssap://system.launcher/getForegroundAppInfo";
  sendJson(doc4);
  Serial.println("Requested foreground app info (launcher)");
}

void launchOnePlay() {
  if (!tvRegistered) {
    pendingLaunch = true;
    Serial.println("TV not registered yet, launch deferred");
    return;
  }

  const String appId = detectedOnePlayAppId.length() > 0 ? detectedOnePlayAppId : String(DEFAULT_ONEPLAY_APP_ID);
  DynamicJsonDocument doc(512);
  doc["id"] = "launch_oneplay";
  doc["type"] = "request";
  doc["uri"] = "ssap://com.webos.applicationManager/launch";
  doc["payload"]["id"] = appId;
  sendJson(doc);

  Serial.print("Launch requested for app id: ");
  Serial.println(appId);
}

void handleNecCommand(uint8_t command, bool isRepeat) {
  if (isRepeat && (millis() - lastIrActionMs) < IR_REPEAT_GUARD_MS) {
    return;
  }
  lastIrActionMs = millis();

  Serial.print("IR NEC command: 0x");
  if (command < 0x10) {
    Serial.print("0");
  }
  Serial.println(command, HEX);

  switch (command) {
    case 0x0C:  // OFF
      sendSimpleRequest("tv_turn_off", "ssap://system/turnOff");
      break;
    case 0x6B:  // RED
      sendPointerButton("RED");
      break;
    case 0x6C:  // GREEN
      sendPointerButton("GREEN");
      break;
    case 0x6D:  // YELLOW
      sendPointerButton("YELLOW");
      break;
    case 0x6E:  // BLUE
      sendPointerButton("BLUE");
      break;
    case 0x51:  // REWIND
      sendPointerButton("REWIND");
      break;
    case 0x36:  // STOP
      sendPointerButton("STOP");
      break;
    case 0x50:  // FAST FORWARD
      sendPointerButton("FASTFORWARD");
      break;
    case 0x12:  // MENU
      sendPointerButton("MENU");
      break;
    case 0x16:  // Ambiguous in notes: OK / RIGHT
      sendPointerButton(NEC_CODE_0x16_IS_RIGHT ? "RIGHT" : "ENTER");
      break;
    case 0x10:  // UP
      sendPointerButton("UP");
      break;
    case 0x11:  // DOWN
      sendPointerButton("DOWN");
      break;
    case 0x15:  // LEFT
      sendPointerButton("LEFT");
      break;
    case 0x1F:  // BACK
      sendPointerButton("BACK");
      break;
    case 0x4A:  // INFO
      sendPointerButton("INFO");
      break;
    case 0x1D:  // TV
      launchOnePlay();
      break;
    case 0x0E:  // VOLUME+
      sendPointerButton("VOLUMEUP");
      break;
    case 0x0A:  // VOLUME-
      sendPointerButton("VOLUMEDOWN");
      break;
    case 0x14:  // CHANNEL+
      sendPointerButton("CHANNELUP");
      break;
    case 0x17:  // CHANNEL-
      sendPointerButton("CHANNELDOWN");
      break;
    case 0x0D:  // MUTE
      sendPointerButton("MUTE");
      break;
    case 0x00:
      sendPointerButton("0");
      break;
    case 0x01:
      sendPointerButton("1");
      break;
    case 0x02:
      sendPointerButton("2");
      break;
    case 0x03:
      sendPointerButton("3");
      break;
    case 0x04:
      sendPointerButton("4");
      break;
    case 0x05:
      sendPointerButton("5");
      break;
    case 0x06:
      sendPointerButton("6");
      break;
    case 0x07:
      sendPointerButton("7");
      break;
    case 0x08:
      sendPointerButton("8");
      break;
    case 0x09:
      sendPointerButton("9");
      break;
    default:
      Serial.print("IR code has no mapping yet: 0x");
      if (command < 0x10) {
        Serial.print("0");
      }
      Serial.println(command, HEX);
      break;
  }
}

void updateIrReceiver() {
  if (!IrReceiver.decode()) {
    return;
  }

  const decode_type_t protocol = IrReceiver.decodedIRData.protocol;
  const bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
  const uint16_t rawCommand = IrReceiver.decodedIRData.command;

  if (!isNecProtocol(protocol)) {
    Serial.print("Ignored non-NEC IR protocol: ");
    Serial.println((int)protocol);
    IrReceiver.resume();
    return;
  }

  handleNecCommand((uint8_t)(rawCommand & 0xFF), isRepeat);
  IrReceiver.resume();
}

void sendRegister() {
  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, REGISTRATION_PAYLOAD);
  if (err) {
    Serial.print("Registration JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  if (clientKey.length() > 0) {
    doc["payload"]["client-key"] = clientKey;
    Serial.println("Trying registration with saved client-key");
  } else {
    Serial.println("No client-key saved, expecting pairing prompt on TV");
  }

  sendJson(doc);
}

void handleWsText(const char* text) {
  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, text);
  if (err) {
    Serial.print("WS JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  const String type = doc["type"] | "";
  const String id = doc["id"] | "";

  if (type == "registered") {
    tvRegistered = true;
    setLed(true);
    String newKey = doc["payload"]["client-key"] | "";
    if (newKey.length() > 0 && newKey != clientKey) {
      clientKey = newKey;
      prefs.putString("client_key", clientKey);
      Serial.println("Saved new client-key");
    }
    Serial.println("Registered with TV");

    if (!listRequested) {
      listRequested = true;
      requestLaunchPoints();
    }
    if (!appInfoRequested) {
      appInfoRequested = true;
      requestCurrentAppInfo();
    }
    if (!pointerSocketRequested) {
      requestPointerSocket();
    }
    if (pendingLaunch) {
      pendingLaunch = false;
      launchOnePlay();
    }
    return;
  }

  if (id == "list_apps" && type == "response") {
    JsonArray apps = doc["payload"]["launchPoints"].as<JsonArray>();
    Serial.println("Installed TV apps:");
    for (JsonObject app : apps) {
      const String title = app["title"] | "";
      const String appId = app["id"] | "";
      Serial.print("- ");
      Serial.print(title);
      Serial.print(" [");
      Serial.print(appId);
      Serial.println("]");

      if (containsIgnoreCase(title, "oneplay") || containsIgnoreCase(appId, "oneplay")) {
        detectedOnePlayAppId = appId;
      }
    }
    if (detectedOnePlayAppId.length() > 0) {
      Serial.print("Detected OnePlay app id: ");
      Serial.println(detectedOnePlayAppId);
    } else {
      Serial.print("OnePlay not auto-detected. Using DEFAULT_ONEPLAY_APP_ID: ");
      Serial.println(DEFAULT_ONEPLAY_APP_ID);
    }
    return;
  }

  if (id == "launch_oneplay" && type == "response") {
    bool ok = doc["payload"]["returnValue"] | false;
    Serial.println(ok ? "OnePlay launch command accepted" : "OnePlay launch command rejected");
    return;
  }

  if (id == "pointer_socket" && type == "response") {
    const String socketPath = doc["payload"]["socketPath"] | "";
    if (socketPath.length() == 0) {
      Serial.println("Pointer socket response missing socketPath");
      pointerSocketRequested = false;
      return;
    }
    connectPointerSocket(socketPath);
    return;
  }

  if ((id == "current_app" || id == "current_app_legacy" || id == "current_app_service" || id == "current_app_launcher") && type == "response") {
    Serial.print("Current app raw response: ");
    serializeJson(doc, Serial);
    Serial.println();

    const String appId = doc["payload"]["appId"] | doc["payload"]["id"] | "";
    const String title = doc["payload"]["title"] | "";
    Serial.print("Current app: ");
    Serial.print(appId);
    if (title.length() > 0) {
      Serial.print(" (");
      Serial.print(title);
      Serial.print(")");
    }
    Serial.println();

    if (containsIgnoreCase(appId, "oneplay") || containsIgnoreCase(title, "oneplay")) {
      detectedOnePlayAppId = appId;
      Serial.print("Detected OnePlay app id from current app: ");
      Serial.println(detectedOnePlayAppId);
    }
    return;
  }

  if (type == "error") {
    Serial.print("TV error: ");
    serializeJson(doc, Serial);
    Serial.println();
  }
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      tvRegistered = false;
      listRequested = false;
      appInfoRequested = false;
      pointerSocketRequested = false;
      pointerWsConnected = false;
      pointerWs.disconnect();
      setLed(false);
      Serial.println("WS disconnected");
      break;
    case WStype_CONNECTED:
      wsConnected = true;
      tvRegistered = false;
      listRequested = false;
      appInfoRequested = false;
      pointerSocketRequested = false;
      Serial.println("WS connected");
      sendRegister();
      break;
    case WStype_TEXT:
      handleWsText((const char*)payload);
      break;
    default:
      break;
  }
}

void onPointerWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      pointerWsConnected = false;
      pointerSocketRequested = false;
      Serial.println("Pointer socket disconnected");
      break;
    case WStype_CONNECTED:
      pointerWsConnected = true;
      Serial.println("Pointer socket connected");
      break;
    case WStype_TEXT:
      if (length > 0) {
        String msg;
        msg.reserve(length);
        for (size_t i = 0; i < length; ++i) {
          msg += (char)payload[i];
        }
        Serial.print("Pointer socket message: ");
        Serial.println(msg);
      }
      break;
    default:
      break;
  }
}

void connectWifiBlocking() {
  if (String(WIFI_SSID) == "YOUR_WIFI_SSID") {
    Serial.println("Set WIFI_SSID/WIFI_PASSWORD/TV_IP in code first.");
    return;
  }

  Serial.print("Connecting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 20000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout");
  }
}

void setupWebSocket() {
  ws.beginSSL(TV_IP, TV_PORT, "/");
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(15000, 3000, 2);

  pointerWs.onEvent(onPointerWsEvent);
}

void updateButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceMs = millis();
    lastButtonReading = reading;
  }

  if ((millis() - lastDebounceMs) > DEBOUNCE_MS && stableButtonState != reading) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      setLed(!ledState);
      Serial.println("Button pressed");
      launchOnePlay();
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setLed(false);

  Serial.begin(BAUD_RATE);
  delay(400);
  Serial.println("ESP32-S3 LG OnePlay launcher start");
  Serial.println("Button on IO6, LED on IO5");
  Serial.print("IR receiver on IO");
  Serial.println(IR_RECEIVE_PIN);

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  prefs.begin("lg_remote", false);
  clientKey = prefs.getString("client_key", "");

  connectWifiBlocking();
  setupWebSocket();
}

void loop() {
  updateButton();
  updateIrReceiver();

  if (WiFi.status() == WL_CONNECTED) {
    ws.loop();
    pointerWs.loop();
  } else {
    if (millis() - lastWifiRetryMs > WIFI_RETRY_MS) {
      lastWifiRetryMs = millis();
      Serial.println("WiFi disconnected, retrying...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  delay(2);
}
