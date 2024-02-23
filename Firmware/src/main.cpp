#include <Arduino.h>
#include <ArduinoOTA.h>
#include <AsyncMqttClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <NeoPixelBus.h>
#include <Ticker.h>

#define ESP8266_DRD_USE_RTC true
#define DOUBLERESETDETECTOR_DEBUG true
#define DRD_TIMEOUT 2
#define DRD_ADDRESS 0
#include <ESP_DoubleResetDetector.h>
DoubleResetDetector *drd;

#include "main.h"

#define Sprintf(f, ...) ({ char* s; asprintf(&s, f, __VA_ARGS__); String r = s; free(s); r; })

const char *mqtt_server = "hoera10jaar.revspace.nl";
String my_hostname = "decennium-";

// NeoPixelBus settings
const uint16_t PixelCount = 15;
#define colorSaturation 32
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Sk6812Method> strip(PixelCount, (uint8_t)4);
RgbColor RgbRed(colorSaturation, 0, 0);
RgbColor RgbGreen(0, colorSaturation, 0);
RgbColor RgbBlue(0, 0, colorSaturation);
RgbColor RgbYellow(colorSaturation, colorSaturation, 0);
RgbColor RgbBlack(0);

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

void connectToWifi() {
    String ssid = read("/wifi-ssid");
    String pw = read("/wifi-password");

    Serial.printf("Connecting to Wi-Fi SSID %s...\n", ssid.c_str());
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(my_hostname);
    WiFi.begin(ssid.c_str(), pw.c_str());
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

void connectToMqtt() {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
    Serial.println("Connected to Wi-Fi.");
    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event) {
    Serial.println("Disconnected from Wi-Fi.");
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);

    uint16_t packetIdSub = mqttClient.subscribe("hoera10jaar/+", 0);
    Serial.print("Subscribing at QoS 0, packetId: ");
    Serial.println(packetIdSub);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.println("Disconnected from MQTT.");

    if (WiFi.isConnected()) {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
    Serial.println("Subscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
    Serial.print("  qos: ");
    Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
    Serial.println("Unsubscribe acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    String t = topic;
    t.replace("hoera10jaar/", "");

    char str[len + 1];
    for (unsigned int i = 0; i < len; i++) {
        str[i] = (char)payload[i];
    }
    str[len] = '\0';
    String m = str;
    String h = "";

    int lednr = -1;
    if (t == "heerlen") {
        h = "ACKspace";
        lednr = 0;
    } else if (t == "venlo") {
        h = "TDvenlo";
        lednr = 1;
    } else if (t == "eindhoven") {
        h = "Hackalot";
        lednr = 2;
    } else if (t == "nijmegen") {
        h = "Hackerspace Nijmegen";
        lednr = 3;
    } else if (t == "wageningen") {
        h = "NURDspace";
        lednr = 4;
    } else if (t == "arnhem") {
        h = "Hack42";
        lednr = 5;
    } else if (t == "enschede") {
        h = "TkkrLab";
        lednr = 6;
    } else if (t == "zwolle") {
        h = "Bhack";
        lednr = 7;
    } else if (t == "leeuwarden") {
        h = "Frack";
        lednr = 8;
    } else if (t == "rotterdam") {
        h = "Pixelbar";
        lednr = 9;
    } else if (t == "denhaag") {
        h = "RevSpace";
        lednr = 10;
    } else if (t == "utrecht") {
        h = "RandomData";
        lednr = 11;
    } else if (t == "amersfoort") {
        h = "Bitlair";
        lednr = 12;
    } else if (t == "almere") {
        h = "Sk1llz";
        lednr = 13;
    } else if (t == "amsterdam") {
        h = "TechInc";
        lednr = 14;
    }

    if (lednr == -1) {
        return;
    }

    String spaceState = "";
    if (m == "red") {
        strip.SetPixelColor(lednr, RgbRed);
        spaceState = "closed";
    } else if (m == "green") {
        strip.SetPixelColor(lednr, RgbGreen);
        spaceState = "open";
    } else if (m == "yellow") {
        strip.SetPixelColor(lednr, RgbYellow);
        spaceState = "unknown";
    } else {
        strip.SetPixelColor(lednr, RgbBlack);
        spaceState = "unknown";
    }

    Serial.printf("MQTT: %s: %s, %s: %s\n", t.c_str(), m.c_str(), h.c_str(), spaceState.c_str());

    strip.Show();
}

void onMqttPublish(uint16_t packetId) {
    Serial.println("Publish acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void loop() {
    ArduinoOTA.handle();
}

void all(RgbColor color) {
    for (int i = 0; i < 15; i++) {
        strip.SetPixelColor(i, color);
    }
    strip.Show();
}

void setup_ota() {
    String ota = pwgen();
    Serial.printf("OTA-wachtwoord is %s\n", ota.c_str());

    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(my_hostname.c_str());
    ArduinoOTA.setPassword(ota.c_str());

    ArduinoOTA.onStart([]() {
        Serial.println(F("OTA Start"));
        all(RgbBlue);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf_P(PSTR("OTA Progress: %u%%\r"), (progress / (total / 100)));
        float p = (float)progress / total;
        strip.SetPixelColor(p * 15, RgbGreen);
        strip.Show();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf_P(PSTR("OTA Error[%u]: "), error);
        if (error == OTA_AUTH_ERROR) {
            Serial.println(F("OTA Auth Failed"));
        } else if (error == OTA_BEGIN_ERROR) {
            Serial.println(F("OTA Begin Failed"));
        } else if (error == OTA_CONNECT_ERROR) {
            Serial.println(F("OTA Connect Failed"));
        } else if (error == OTA_RECEIVE_ERROR) {
            Serial.println(F("OTA Receive Failed"));
        } else if (error == OTA_END_ERROR) {
            Serial.println(F("OTA End Failed"));
        }
        all(RgbBlack);
    });

    ArduinoOTA.onEnd([]() {
        all(RgbBlack);
    });

    ArduinoOTA.begin();
}

String read(const char *fn) {
    File f = LittleFS.open(fn, "r");
    String r = f.readString();
    f.close();
    return r;
}

void store(const char *fn, String content) {
    File f = LittleFS.open(fn, "w");
    f.print(content);
    f.close();
}

String pwgen() {
    const char *filename = "/ota-password";
    const char *passchars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxz123456789-#@;?";

    String password = read(filename);

    if (password.length() == 0) {
        for (int i = 0; i < 16; i++) {
            password.concat(passchars[random(strlen(passchars))]);
        }
        store(filename, password);
    }

    return password;
}

String html_entities(String raw) {
    String r;
    for (unsigned int i = 0; i < raw.length(); i++) {
        char c = raw.charAt(i);
        if (c >= '!' && c <= 'z' && c != '&' && c != '<' && c != '>') {
            // printable ascii minus html and {}
            r += c;
        } else {
            r += Sprintf("&#%d;", raw.charAt(i));
        }
    }
    return r;
}

void setup_wifi_portal() {
    Serial.println("Start Wi-Fi AP for configuration.\n");

    static ESP8266WebServer http(80);
    static DNSServer dns;
    static int num_networks = -1;
    String ota = pwgen();

    WiFi.disconnect();
    WiFi.softAP(my_hostname.c_str());

    delay(500);
    dns.setTTL(0);
    dns.start(53, "*", WiFi.softAPIP());
    setup_ota();

    Serial.println(WiFi.softAPIP().toString());

    http.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html>\n<meta charset=UTF-8>"
      "<title>{hostname}</title>"
      "<form action=/restart method=post>"
        "Hallo, ik ben {hostname}."
        "<p>Huidig ingestelde SSID: {ssid}<br>"
        "<input type=submit value='Opnieuw starten'>"
      "</form>"
      "<hr>"
      "<h2>Configureren</h2>"
      "<form method=post>"
        "SSID: <select name=ssid onchange=\"document.getElementsByName('password')[0].value=''\">{options}</select> "
        "<a href=/rescan onclick=\"this.innerHTML='scant...';\">opnieuw scannen</a>"
        "</select><br>Wifi WEP/WPA-wachtwoord: <input name=password value='{password}'><br>"
        "<p>Mijn eigen OTA/WPA-wachtwoord: <input name=ota value='{ota}' minlength=8 required> (8+ tekens, en je wilt deze waarschijnlijk *nu* ergens opslaan)<br>"
        "<p><input type=submit value=Opslaan>"
      "</form>";

    String current = read("/wifi-ssid");
    String pw = read("/wifi-password");

    html.replace("{hostname}",  my_hostname);
    html.replace("{ssid}",      current.length() ? html_entities(current) : "(not set)");
    html.replace("{ota}",       html_entities(pwgen()));

    String options;
    if (num_networks < 0) {
        num_networks = WiFi.scanNetworks();
    }
    uint8_t found = 0;
    for (int i = 0; i < num_networks; i++) {
      String opt = "<option value='{ssid}'{sel}>{ssid} {lock} {1x}</option>";
      String ssid = WiFi.SSID(i);
      uint8_t mode = WiFi.encryptionType(i);

      opt.replace("{sel}",  ssid == current && !(found++) ? " selected" : "");
      opt.replace("{ssid}", html_entities(ssid));
      opt.replace("{lock}", mode != ENC_TYPE_NONE ? "&#x1f512;" : "");
      opt.replace("{1x}",   mode == ENC_TYPE_TKIP ? "(werkt niet: 802.1x wordt niet ondersteund)" : "");
      options += opt;
    }
    html.replace("{password}", found && pw.length() ? "##**##**##**" : "");
    html.replace("{options}",  options);
    http.send(200, "text/html", html); });

    http.on("/", HTTP_POST, []() {
        String pw = http.arg("password");
        if (pw != "##**##**##**") {
            store("/wifi-password", pw);
        }
        store("/wifi-ssid", http.arg("ssid"));
        store("/ota-password", http.arg("ota"));
        http.sendHeader("Location", "/");
        http.send(302, "text/plain", "ok");
    });

    http.on("/restart", HTTP_POST, []() {
        http.send(200, "text/plain", "Doei!");
        all(RgbBlack);
        ESP.restart();
    });

    http.on("/rescan", HTTP_GET, []() {
        http.sendHeader("Location", "/");
        http.send(302, "text/plain", "wait for it...");
        num_networks = WiFi.scanNetworks();
    });

    http.onNotFound([]() {
        http.sendHeader("Location", "http://" + my_hostname + "/");
        http.send(302, "text/plain", "hoi");
    });

    http.begin();

    for (;;) {
        unsigned long m = millis();
        if (m % 1000 < 20) {
            bool x = m % 2000 < 1000;
            if (x) {
                all(RgbGreen);
            } else {
                all(RgbRed);
            }
        }
        http.handleClient();
        dns.processNextRequest();
        ArduinoOTA.handle();
    }
}

void setup_wifi() {
    String ssid = read("/wifi-ssid");
    String pw = read("/wifi-password");
    if (ssid.length() == 0 || drd->detectDoubleReset()) {
        setup_wifi_portal();
    }

    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onSubscribe(onMqttSubscribe);
    mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.onPublish(onMqttPublish);
    mqttClient.setClientId(my_hostname.c_str());
    mqttClient.setServer(mqtt_server, 1883);

    setup_ota();
    connectToWifi();
}

void setup() {
    drd = new DoubleResetDetector(DRD_TIMEOUT, DRD_ADDRESS);

    strip.Begin();
    strip.Show();
    all(RgbBlue);

    Serial.begin(115200);
    Serial.println("o hai");

    Serial.print(F("ESP.getFullVersion(): "));
    Serial.println(ESP.getFullVersion());
    Serial.print(F("ESP.getResetReason(): "));
    Serial.println(ESP.getResetReason());
    Serial.print(F("ESP.getChipId(): "));
    Serial.println(ESP.getChipId());
    Serial.print(F("ESP.getCpuFreqMHz(): "));
    Serial.println(ESP.getCpuFreqMHz());
    Serial.print(F("ESP.getFlashChipSize(): "));
    Serial.println(ESP.getFlashChipSize());
    Serial.print(F("ESP.getFlashChipRealSize(): "));
    Serial.println(ESP.getFlashChipRealSize());
    Serial.print(F("ESP.getSketchSize(): "));
    Serial.println(ESP.getSketchSize());
    Serial.print(F("ESP.getFreeSketchSpace(): "));
    Serial.println(ESP.getFreeSketchSpace());

    LittleFS.begin();
    FSInfo fs_info;
    LittleFS.info(fs_info);
    Serial.print(F("LittleFS totalBytes: "));
    Serial.println(fs_info.totalBytes);
    Serial.print(F("LittleFS usedBytes: "));
    Serial.println(fs_info.usedBytes);

    randomSeed(ESP.random());

    String mac = WiFi.macAddress();
    mac.replace(F(":"), F(""));
    mac.toLowerCase();
    my_hostname += mac;
    Serial.print(F("My hostname: "));
    Serial.println(my_hostname);

    setup_wifi();
    all(RgbBlack);
}
