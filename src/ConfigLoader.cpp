#include "ConfigLoader.h"
#include <SDCardManager.h>
#include <WiFi.h>

TrmnlConfig ConfigLoader::config;

ConfigResult ConfigLoader::load(const char* path) {
    if (!SdMan.ready()) {
        return ConfigResult(ConfigError::SD_NOT_READY, "SD card not ready");
    }

    if (!SdMan.exists(path)) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "Config file not found: %s", path);
        return ConfigResult(ConfigError::FILE_NOT_FOUND, errorMsg);
    }

    FsFile file = SdMan.open(path, O_RDONLY);
    if (!file || !file.isOpen()) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "Failed to open config file: %s", path);
        return ConfigResult(ConfigError::FILE_OPEN_FAILED, errorMsg);
    }

    size_t fileSize = file.size();
    if (fileSize == 0) {
        file.close();
        return ConfigResult(ConfigError::INVALID_VALUE, "Config file is empty");
    }

    std::unique_ptr<char[]> buffer(new char[fileSize + 1]);
    if (!buffer) {
        file.close();
        return ConfigResult(ConfigError::INVALID_VALUE, "Failed to allocate memory for config");
    }

    size_t bytesRead = file.read((uint8_t*)buffer.get(), fileSize);
    file.close();

    if (bytesRead != fileSize) {
        return ConfigResult(ConfigError::INVALID_VALUE, "Failed to read config file completely");
    }

    buffer[bytesRead] = '\0';

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buffer.get());

    if (error) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "JSON parse error: %s", error.c_str());
        return ConfigResult(ConfigError::JSON_PARSE_FAILED, errorMsg);
    }

    config.wifiSsid = doc["wifi_ssid"].as<String>();
    config.wifiPassword = doc["wifi_password"].as<String>();
    config.serverUrl = doc["server_url"].as<String>();
    config.apiKey = doc["api_key"].as<String>();
    config.deviceId = doc["device_id"].as<String>();

    if (doc["refresh_interval"].is<uint32_t>()) {
        config.refreshInterval = doc["refresh_interval"].as<uint32_t>();
    } else {
        config.refreshInterval = 1800;
    }

    if (doc["use_insecure_tls"].is<bool>()) {
        config.useInsecureTls = doc["use_insecure_tls"].as<bool>();
    } else {
        config.useInsecureTls = true;
    }

    if (doc["standalone_mode"].is<bool>()) {
        config.standaloneMode = doc["standalone_mode"].as<bool>();
    } else {
        config.standaloneMode = false;
    }

    if (config.deviceId.isEmpty()) {
        config.deviceId = WiFi.macAddress();
    }

    ConfigResult validation = validateRequiredFields();
    if (validation.error != ConfigError::SUCCESS) {
        return validation;
    }

    return ConfigResult(ConfigError::SUCCESS, "Config loaded successfully");
}

ConfigResult ConfigLoader::validateRequiredFields() {
    if (config.wifiSsid.isEmpty()) {
        return ConfigResult(ConfigError::MISSING_REQUIRED_FIELD, "Missing required field: wifi_ssid");
    }

    if (config.wifiPassword.isEmpty()) {
        return ConfigResult(ConfigError::MISSING_REQUIRED_FIELD, "Missing required field: wifi_password");
    }

    if (config.serverUrl.isEmpty()) {
        return ConfigResult(ConfigError::MISSING_REQUIRED_FIELD, "Missing required field: server_url");
    }

    if (config.apiKey.isEmpty()) {
        return ConfigResult(ConfigError::MISSING_REQUIRED_FIELD, "Missing required field: api_key");
    }

    return ConfigResult(ConfigError::SUCCESS, "");
}
