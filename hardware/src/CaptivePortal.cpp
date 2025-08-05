#include <CaptivePortal.h>


const char *ssid = "SplashFlag";  // FYI The SSID can't have a space in it.
const char *password = NULL;  // no password

#define MAX_CLIENTS 4	// ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6	// 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);		   // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);		   // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage

const char index_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>ESP32 Captive Portal</title>
      <style>
        body {background-color:#8cd5ff;}
        h1 {color: white;}
        h2 {color: white;}
      </style>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>SplashFlag WiFi setup</h1>
      <p>This SplashFlag device needs to connect to your home's WiFi so it can receive statuses for when the Wagner's are swimming.</p>

    <form action="/save" method="POST">
        <label for="ssid">WiFi SSID:</label><br>
        <input type="text" id="ssid" name="ssid" required><br><br>
        <label for="password">WiFi Password:</label><br>
        <input type="password" id="password" name="password"><br><br>
        <input type="submit" value="Connect">
    </form>
    </body>
  </html>
)=====";

const char save_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>ESP32 Captive Portal</title>
      <style>
        body {background-color:#8cd5ff;}
        h1 {color: white;}
        h2 {color: white;}
      </style>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>Thanks!</h1>
      <p>Saving your credentials...</p>
      <p>If the SplashFlag successfully connects to your Wifi, the device will restart and display a success message.</p>
      <p>If you are returned to the credentials screen, please try your credentials again.</p>

    </body>
  </html>
)=====";

DNSServer dnsServer;
AsyncWebServer server(80);

CaptivePortal::CaptivePortal(CredentialManager& credentialManager, Lcd& lcd) : credentialManager(credentialManager), lcd(lcd)
{   

}

void CaptivePortal::init() 
{

    // Print a welcome message to the Serial port.
	Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");  //__DATE__ is provided by the platformio ide
	Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

	startSoftAccessPoint(ssid, password, localIP, gatewayIP);

	setUpDNSServer(dnsServer, localIP);

	setUpWebserver(server, localIP);
	server.begin();

	Serial.print("\n");
	Serial.print("Startup Time:");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
	Serial.println(millis());
	Serial.print("\n");
}

void CaptivePortal::setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

	// Set the TTL for DNS response and start the DNS server
	dnsServer.setTTL(3600);
	dnsServer.start(53, "*", localIP);
}

void CaptivePortal::startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

	// Set the WiFi mode to access point and station
	WiFi.mode(WIFI_MODE_AP);

	// Define the subnet mask for the WiFi network
	const IPAddress subnetMask(255, 255, 255, 0);

	// Configure the soft access point with a specific IP and subnet mask
	WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

	// Start the soft access point with the given ssid, password, channel, max number of clients
	WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

	// Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
	esp_wifi_stop();
	esp_wifi_deinit();
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
	my_config.ampdu_rx_enable = false;
	esp_wifi_init(&my_config);
	esp_wifi_start();
	vTaskDelay(100 / portTICK_PERIOD_MS);  // Add a small delay
}

void CaptivePortal::setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
	//======================== Webserver ========================
	// WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
	// SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
	// SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
	// SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

	// Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home

	// B Tier (uncommon)
	//  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
	//  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
	//  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
	//  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

	// return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

	// Serve Basic HTML Page
	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Served index page");
	});

    server.on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {

        char input_ssid[50] = {0};  // Static char array, initialized to zeros
        char input_password[50] = {0};

        if (request->hasParam("ssid",true)) {
            const char* value = request->getParam("ssid", true)->value().c_str();
            strncpy(input_ssid, value, sizeof(input_ssid) - 1);
            input_ssid[sizeof(input_ssid) - 1] = '\0'; // Ensure null termination

            Serial.printf("SSID: %s\n", input_ssid);
        }

        if (request->hasParam("password",true)) {
            const char* value = request->getParam("password", true)->value().c_str();
            strncpy(input_password, value, sizeof(input_password) - 1);
            input_password[sizeof(input_password) - 1] = '\0'; // Ensure null termination

            //Serial.printf("Password: %s\n", input_password);
        }

		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", save_html);
		response->addHeader("Refresh", "5; url=/");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Served save wifi page");

        // Save credentials and reset Arduino
        credentialManager.saveCredentials(input_ssid,input_password);
        lcd.write("Restarting SplashFlag...");
        delay(2000);
        esp_restart();
	});

	// the catch all
	server.onNotFound([](AsyncWebServerRequest *request) {
		request->redirect(localIPURL);
		Serial.print("onnotfound ");
		Serial.print(request->host());	// This gives some insight into whatever was being requested on the serial monitor
		Serial.print(" ");
		Serial.print(request->url());
		Serial.print(" sent redirect to " + localIPURL + "\n");
	});

    
}

void CaptivePortal::processNextDNSRequest()
{
    dnsServer.processNextRequest();	 // I call this atleast every 10ms in my other projects (can be higher but I haven't tested it for stability)
    delay(DNS_INTERVAL);			 // seems to help with stability, if you are doing other things in the loop this may not be needed
};


