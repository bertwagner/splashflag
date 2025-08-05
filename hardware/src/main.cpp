#include "SplashFlagController.h"

Lcd lcd(0x27, 16, 2);
ServoFlag servoFlag(9);
CredentialManager credentialManager;
CaptivePortal portal(credentialManager, lcd);
SplashFlagController controller(lcd, servoFlag, credentialManager, portal);


void setup() {
    Serial.begin(115200);

    pinMode(4, INPUT);
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    servoFlag.init();
    lcd.init();
    controller.init();
    controller.setDisplayMessage(("Welcome to SplashFlag! v" + String(FIRMWARE_VERSION)).c_str());
    
    auto [ssid, password] = credentialManager.retrieveCredentials();
    char ssidarr[strlen(ssid) + 1];
    strcpy(ssidarr, ssid);
    
    char passwordarr[strlen(password) + 1];
    strcpy(passwordarr, password);

    if (ssid == nullptr || strlen(ssidarr) == 0 || ssid[0] == '\0') {
        portal.init();
        controller.setDisplayMessage("Please connect to SplashFlag Wifi AP with your phone and visit http://4.3.2.1");
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssidarr, passwordarr);

        int wifiConnectTries = 0;
        const int maxTries = 20;
        while (WiFi.status() != WL_CONNECTED && wifiConnectTries < maxTries) {
            Serial.print('.');
            delay(1000);
            wifiConnectTries++;
            controller.handleResetButton();
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connected!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());

            controller.setDisplayMessageWithDuration("SplashFlag wifi connected! The screen will now go blank until a pool announcement is made.", 3, true);
            servoFlag.init();
        } else {
            Serial.println("\nFailed to connect to WiFi.");
            controller.setDisplayMessage("Wifi connection failed. Check your wifi connection. If problem persists, hold factory reset button for 10 seconds and re-enter Wifi password.");
            
            while (true) {
                controller.handleResetButton();
                delay(50);
            }
        }
    }
}

void loop() {
    controller.update();
}



