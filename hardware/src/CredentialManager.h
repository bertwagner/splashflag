#pragma once
#include <Preferences.h>
#include <tuple>

class CredentialManager {
    public:
        CredentialManager();

        void saveCredentials(const char* ssid, const char* password);
        std::pair<char*, char*> retrieveCredentials();

    private:
        Preferences _preferences;
};