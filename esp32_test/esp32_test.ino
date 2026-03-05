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
const uint32_t IR_REPEAT_GUARD_MS = 220;
const uint32_t WIFI_RETRY_MS = 10000;
const bool NEC_CODE_0x16_IS_RIGHT = false;  // false = ENTER, true = RIGHT
const bool LOG_ALL_IR_PACKETS = true;
const bool LOG_RAW_IR_TIMINGS = true;
const uint8_t CLIENT_KEY_SCHEMA_VERSION = 2;  // Bump to force re-pairing when manifest permissions change.
const bool RUN_ARROW_URI_PROBE_ON_REGISTER = false;
const bool REQUEST_APP_LIST_ON_REGISTER = false;
const bool REQUEST_CURRENT_APP_INFO_ON_REGISTER = false;

struct PulseDistanceMapEntry {
  uint32_t rawData;
  uint8_t bitCount;
  uint8_t mappedCommand;
};

// Initial mapping from captured O2 STB remote PulseDistance frames.
// If any key behaves incorrectly, adjust values after a guided one-key capture.
const PulseDistanceMapEntry PULSE_DISTANCE_MAP[] = {
  // Mapping captured in fixed button order from infra_ovladac_kody.md (first 22 keys).
  {0x2090008, 29, 0x0C},  // OFF
  {0x2080008, 29, 0x6B},  // RED
  {0x950008, 26, 0x6C},   // GREEN
  {0xA8088, 26, 0x6D},    // YELLOW
  {0xC400008, 28, 0x6E},  // BLUE
  {0x5200088, 27, 0x51},  // REWIND
  {0x40088, 28, 0x50},    // FAST FORWARD
  {0x4400008, 29, 0x3F},  // REC
  {0x1200088, 28, 0x12},  // MENU
  {0x8D0008, 26, 0x41},   // EPG
  {0x68088, 26, 0x3C},    // TXT
  {0x28D0008, 26, 0x16},  // OK
  {0x2068088, 26, 0x10},  // UP
  {0x2040088, 27, 0x10},  // UP (live capture variant)
  {0x2180008, 27, 0x11},  // DOWN
  {0x3050008, 27, 0x11},  // DOWN (live capture frame A)
  {0x2028088, 27, 0x11},  // DOWN (live capture frame B)
  {0x3050008, 26, 0x15},  // LEFT (live capture frame A)
  {0x2028088, 26, 0x15},  // LEFT (live capture frame B)
  {0x8C0088, 26, 0x15},   // LEFT
  {0x1220008, 27, 0x18},  // RIGHT (custom command code, see switch case)
  {0x5060008, 27, 0x18},  // RIGHT (live capture frame A)
  {0x4030088, 27, 0x18},  // RIGHT (live capture frame B)
  {0x1110088, 26, 0x1F},  // BACK
  {0x1150008, 27, 0x1F},  // BACK (live capture frame A)
  {0x4A8088, 26, 0x1F},   // BACK (live capture frame B)
  {0x2220008, 27, 0x4A},  // INFO
  {0x910088, 26, 0x1D},   // TV
  {0x2400008, 28, 0x14},  // CHANNEL+ (live capture frame A)
  {0x200088, 28, 0x14},   // CHANNEL+ (live capture frame B)
  {0x4040008, 28, 0x17},  // CHANNEL- (live capture frame A)
  {0x1020088, 27, 0x17},  // CHANNEL- (live capture frame B)
  {0x6240008, 27, 0x17},  // CHANNEL- (live capture frame C)
  {0x2920088, 26, 0x17},  // CHANNEL- (live capture frame D)
  {0x10E0008, 27, 0x0A},  // VOLUME- (swapped per live test)
  {0x470088, 26, 0x0E},   // VOLUME+ (swapped per live test)
  // Alternate volume frames captured during focused VOL+/VOL- test.
  {0x2300008, 28, 0x0A},  // VOLUME- frame A (swapped per live test)
  {0x980088, 27, 0x0A},   // VOLUME- frame B (swapped per live test)
  {0x1190008, 26, 0x0E},  // VOLUME+ frame A (swapped per live test)
  {0x1191008, 26, 0x0E},  // VOLUME+ frame A (jitter variant, swapped)
  {0x4C8088, 25, 0x0E},   // VOLUME+ frame B (swapped per live test)
};

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
        "CONTROL_INPUT_TEXT",
        "CONTROL_MOUSE_AND_KEYBOARD",
        "READ_APP_STATUS",
        "READ_INSTALLED_APPS",
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
bool muteTogglePending = false;
String pendingPointerButton;
bool oneplayMenuOpened = false;
bool oneplaySessionActive = false;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
uint32_t lastDebounceMs = 0;
uint32_t lastWifiRetryMs = 0;
uint32_t lastIrActionMs = 0;
uint8_t lastIrCommand = 0xFF;

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

bool isSupportedIrProtocol(decode_type_t protocol) {
  return protocol == NEC || protocol == NEC2 || protocol == ONKYO || protocol == RC5 || protocol == RC6 || protocol == PULSE_DISTANCE;
}

void logIrPacket(decode_type_t protocol, uint16_t address, uint16_t rawCommand, bool isRepeat, uint8_t flags) {
  if (!LOG_ALL_IR_PACKETS) {
    return;
  }

  Serial.printf(
    "IR rx protocol=%d address=0x%04X command=0x%02X repeat=%d flags=0x%02X\n",
    (int)protocol,
    address,
    (uint8_t)(rawCommand & 0xFF),
    isRepeat ? 1 : 0,
    flags
  );
}

bool mapPulseDistanceRawToCommand(uint32_t rawData, uint8_t bitCount, uint8_t& mappedCommand) {
  for (size_t i = 0; i < (sizeof(PULSE_DISTANCE_MAP) / sizeof(PULSE_DISTANCE_MAP[0])); ++i) {
    if (PULSE_DISTANCE_MAP[i].rawData == rawData && PULSE_DISTANCE_MAP[i].bitCount == bitCount) {
      mappedCommand = PULSE_DISTANCE_MAP[i].mappedCommand;
      return true;
    }
  }
  return false;
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

bool isValidPointerButtonName(const String& buttonName) {
  static const char* VALID_BUTTONS[] = {
    "MUTE", "RED", "GREEN", "YELLOW", "BLUE", "HOME", "MENU", "VOLUMEUP", "VOLUMEDOWN",
    "CHANNELUP", "CHANNELDOWN", "*", "ASTERISK", "CC", "BACK", "UP", "DOWN", "LEFT", "RIGHT",
    "ENTER", "DASH", "INFO", "EXIT", "PLAY", "PAUSE", "STOP", "REWIND", "FASTFORWARD",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
  };
  for (size_t i = 0; i < (sizeof(VALID_BUTTONS) / sizeof(VALID_BUTTONS[0])); ++i) {
    if (buttonName.equalsIgnoreCase(VALID_BUTTONS[i])) {
      return true;
    }
  }
  return false;
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

void sendPointerOrSimpleRequest(const char* pointerButtonName, const char* requestId, const char* uri) {
  if (pointerWsConnected) {
    sendPointerButton(pointerButtonName);
  } else {
    sendSimpleRequest(requestId, uri);
  }
}

void sendSetMuteRequest(bool mute) {
  if (!tvRegistered) {
    Serial.println("TV not registered, skipped request: tv_set_mute");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["id"] = "tv_set_mute";
  doc["type"] = "request";
  doc["uri"] = "ssap://audio/setMute";
  doc["payload"]["mute"] = mute;
  sendJson(doc);
  Serial.printf("Requested mute=%d\n", mute ? 1 : 0);
}

void toggleMuteViaWifi() {
  if (!tvRegistered) {
    Serial.println("TV not registered, skipped request: tv_audio_status");
    return;
  }
  if (muteTogglePending) {
    return;
  }

  muteTogglePending = true;
  sendSimpleRequest("tv_audio_status", "ssap://audio/getStatus");
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
  if (!isValidPointerButtonName(buttonName)) {
    Serial.print("Invalid pointer button name, skipped: ");
    Serial.println(buttonName);
    return;
  }

  if (!pointerWsConnected) {
    Serial.print("Pointer socket not ready, dropped button: ");
    Serial.println(buttonName);
    pendingPointerButton = buttonName;
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

void sendVolumeAction(const char* requestId, const char* uri, const char* pointerButtonName) {
  if (pointerWsConnected) {
    sendPointerButton(pointerButtonName);
  } else {
    sendSimpleRequest(requestId, uri);
  }
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

void runArrowUriProbe() {
  if (!tvRegistered) {
    return;
  }

  // Diagnostic probe: test if any non-pointer directional endpoints are accepted.
  sendSimpleRequest("probe_services", "ssap://api/getServiceList");
  sendSimpleRequest("probe_key_up", "ssap://com.webos.service.tv.keycontrol/up");
  sendSimpleRequest("probe_key_down", "ssap://com.webos.service.tv.keycontrol/down");
  sendSimpleRequest("probe_key_left", "ssap://com.webos.service.tv.keycontrol/left");
  sendSimpleRequest("probe_key_right", "ssap://com.webos.service.tv.keycontrol/right");
  sendSimpleRequest("probe_send_enter", "ssap://com.webos.service.ime/sendEnterKey");
  Serial.println("Arrow URI probe requests sent");
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
  oneplayMenuOpened = false;
  oneplaySessionActive = true;

  Serial.print("Launch requested for app id: ");
  Serial.println(appId);
}

void handleOnePlayMenuShortcut() {
  // First press: launch OnePlay. Next presses: toggle in-app list/menu.
  if (!oneplaySessionActive) {
    launchOnePlay();
    Serial.println("OnePlay shortcut: launch");
    return;
  }

  handleMenuToggle();
}

void handleMenuToggle() {
  if (oneplayMenuOpened) {
    sendPointerButton("BACK");
    Serial.println("Menu toggle: BACK (return to playback)");
  } else {
    sendPointerButton("MENU");
    Serial.println("Menu toggle: MENU (open OnePlay menu)");
  }
  oneplayMenuOpened = !oneplayMenuOpened;
}

void handleNecCommand(uint8_t command, bool isRepeat) {
  if (command == lastIrCommand && (millis() - lastIrActionMs) < IR_REPEAT_GUARD_MS) {
    return;
  }
  if (isRepeat && (millis() - lastIrActionMs) < IR_REPEAT_GUARD_MS) {
    return;
  }
  lastIrCommand = command;
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
      sendPointerOrSimpleRequest("REWIND", "tv_rewind", "ssap://media.controls/rewind");
      break;
    case 0x36:  // STOP
      sendPointerOrSimpleRequest("STOP", "tv_stop", "ssap://media.controls/stop");
      break;
    case 0x50:  // FAST FORWARD
      sendPointerOrSimpleRequest("FASTFORWARD", "tv_fast_forward", "ssap://media.controls/fastForward");
      break;
    case 0x3F:  // REC
      Serial.println("REC has no configured SSAP action");
      break;
    case 0x12:  // MENU: launch OnePlay first, then toggle in-app menu list
      handleOnePlayMenuShortcut();
      break;
    case 0x41:  // EPG
      sendSimpleRequest("tv_epg_info", "ssap://tv/getChannelProgramInfo");
      break;
    case 0x3C:  // TXT
      sendPointerButton("CC");
      break;
    case 0x16:  // Ambiguous in notes: OK / RIGHT
      if (NEC_CODE_0x16_IS_RIGHT) {
        sendPointerButton("RIGHT");
      } else {
        sendPointerOrSimpleRequest("ENTER", "tv_enter", "ssap://com.webos.service.ime/sendEnterKey");
      }
      break;
    case 0x18:  // RIGHT (PulseDistance-specific mapping)
      sendPointerButton("RIGHT");
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
      oneplayMenuOpened = false;
      break;
    case 0x4A:  // INFO
      sendPointerOrSimpleRequest("INFO", "tv_info", "ssap://com.webos.applicationManager/getForegroundAppInfo");
      break;
    case 0x1D:  // TV key keeps direct OnePlay launch shortcut
      launchOnePlay();
      break;
    case 0x0E:  // VOLUME+
      sendVolumeAction("tv_volume_up", "ssap://audio/volumeUp", "VOLUMEUP");
      break;
    case 0x0A:  // VOLUME-
      sendVolumeAction("tv_volume_down", "ssap://audio/volumeDown", "VOLUMEDOWN");
      break;
    case 0x14:  // CHANNEL+
      sendPointerButton("CHANNELUP");
      break;
    case 0x17:  // CHANNEL-
      sendPointerButton("CHANNELDOWN");
      break;
    case 0x0D:  // MUTE
      if (pointerWsConnected) {
        sendPointerButton("MUTE");
      } else {
        toggleMuteViaWifi();
      }
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
  const uint16_t address = IrReceiver.decodedIRData.address;
  const uint16_t rawCommand = IrReceiver.decodedIRData.command;
  const uint32_t decodedRawData32 = (uint32_t)(IrReceiver.decodedIRData.decodedRawData & 0xFFFFFFFFULL);
  const uint8_t bitCount = IrReceiver.decodedIRData.numberOfBits;
  const uint8_t flags = IrReceiver.decodedIRData.flags;

  logIrPacket(protocol, address, rawCommand, isRepeat, flags);
  if (LOG_RAW_IR_TIMINGS) {
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRSendUsage(&Serial);
    IrReceiver.printIRResultRawFormatted(&Serial, true);
  }

  if (!isSupportedIrProtocol(protocol)) {
    Serial.printf("Ignored unsupported IR packet (protocol=%d)\n", (int)protocol);
    IrReceiver.resume();
    return;
  }

  if (protocol == PULSE_DISTANCE) {
    uint8_t mappedCommand = 0;
    if (mapPulseDistanceRawToCommand(decodedRawData32, bitCount, mappedCommand)) {
      Serial.printf("PulseDistance mapped: raw=0x%lX bits=%u -> command=0x%02X\n", decodedRawData32, bitCount, mappedCommand);
      handleNecCommand(mappedCommand, isRepeat);
    } else {
      Serial.printf("PulseDistance has no mapping yet: raw=0x%lX bits=%u\n", decodedRawData32, bitCount);
    }
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

    if (REQUEST_APP_LIST_ON_REGISTER && !listRequested) {
      listRequested = true;
      requestLaunchPoints();
    }
    if (REQUEST_CURRENT_APP_INFO_ON_REGISTER && !appInfoRequested) {
      appInfoRequested = true;
      requestCurrentAppInfo();
    }
    if (!pointerSocketRequested) {
      requestPointerSocket();
    }
    if (RUN_ARROW_URI_PROBE_ON_REGISTER) {
      runArrowUriProbe();
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
    if (!ok) {
      oneplaySessionActive = false;
    }
    Serial.println(ok ? "OnePlay launch command accepted" : "OnePlay launch command rejected");
    return;
  }

  if (id == "tv_audio_status" && type == "response") {
    muteTogglePending = false;
    bool muted = doc["payload"]["mute"] | doc["payload"]["muteStatus"] | false;
    sendSetMuteRequest(!muted);
    return;
  }

  if (id == "tv_set_mute" && type == "response") {
    bool ok = doc["payload"]["returnValue"] | false;
    Serial.println(ok ? "Mute toggle command accepted" : "Mute toggle command rejected");
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
      oneplaySessionActive = false;
      oneplayMenuOpened = false;
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
      if (pendingPointerButton.length() > 0) {
        const String button = pendingPointerButton;
        pendingPointerButton = "";
        sendPointerButton(button.c_str());
      }
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
  Serial.println("IR protocol filter: NEC/NEC2/ONKYO/RC5/RC6/PULSE_DISTANCE");

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  prefs.begin("lg_remote", false);
  const uint8_t storedKeySchema = prefs.getUChar("client_key_schema", 0);
  if (storedKeySchema != CLIENT_KEY_SCHEMA_VERSION) {
    prefs.remove("client_key");
    prefs.putUChar("client_key_schema", CLIENT_KEY_SCHEMA_VERSION);
    Serial.println("Saved client-key cleared (schema update). Pairing prompt expected on TV.");
  }
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
