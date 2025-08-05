#include "CredentialManager.h"

CredentialManager::CredentialManager()
{
}

void CredentialManager::saveCredentials(const char* ssid, const char* password)
{
    _preferences.begin("splashflag-app", false);
    _preferences.putString("wifi_ssid", ssid);
    _preferences.putString("wifi_pass", password);
    _preferences.end();
}

std::pair<char*, char*> CredentialManager::retrieveCredentials()
{
    _preferences.begin("splashflag-app", false);

    char ssid[50] = {0};
    char password[50] = {0};

    strncpy(ssid, _preferences.getString("wifi_ssid", "").c_str(), sizeof(ssid) - 1);
    strncpy(password, _preferences.getString("wifi_pass", "").c_str(), sizeof(password) - 1);

    _preferences.end();

    return {ssid, password};
}