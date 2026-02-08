#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Configuration structure for TRMNL dashboard
 *
 * Contains all configuration parameters loaded from /trmnl-config.json
 */
struct TrmnlConfig {
    String wifiSsid;         ///< WiFi SSID (required)
    String wifiPassword;      ///< WiFi password (required)
    String serverUrl;         ///< Server URL e.g. "https://usetrmnl.com" (required)
    String apiKey;            ///< TRMNL API key (required)
    String deviceId;          ///< Custom device ID (optional, empty = use WiFi MAC)
    uint32_t refreshInterval; ///< Seconds between refreshes (default 1800)
    bool useInsecureTls;      ///< Skip TLS cert validation (default true for MVP)
    bool standaloneMode;      ///< If true, Back button ignored (no CrossPoint to return to)

    /**
     * @brief Constructor with default values
     */
    TrmnlConfig()
        : refreshInterval(1800)
        , useInsecureTls(true)
        , standaloneMode(false) {
    }
};

/**
 * @brief Error codes for config loading
 */
enum class ConfigError {
    SUCCESS = 0,
    SD_NOT_READY,
    FILE_NOT_FOUND,
    FILE_OPEN_FAILED,
    JSON_PARSE_FAILED,
    MISSING_REQUIRED_FIELD,
    INVALID_VALUE
};

/**
 * @brief Result structure for config loading
 */
struct ConfigResult {
    ConfigError error;
    String errorMessage; ///< Human-readable error message

    ConfigResult() : error(ConfigError::SUCCESS) {}
    ConfigResult(ConfigError err, const char* msg) : error(err), errorMessage(msg) {}
};

/**
 * @brief Configuration loader for TRMNL dashboard
 *
 * Loads and parses JSON configuration file from SD card.
 * Uses SDCardManager for file operations and ArduinoJson for parsing.
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from SD card
     *
     * @param path Path to config file (default: "/trmnl-config.json")
     * @return ConfigResult Result with success/error status and message
     */
    static ConfigResult load(const char* path = "/trmnl-config.json");

    /**
     * @brief Get the loaded configuration
     *
     * Must only be called after a successful load()
     *
     * @return const TrmnlConfig& Reference to loaded config
     */
    static const TrmnlConfig& getConfig() { return config; }

private:
    static TrmnlConfig config; ///< Loaded configuration

    /**
     * @brief Validate required configuration fields
     *
     * @return ConfigResult Result with success/error status
     */
    static ConfigResult validateRequiredFields();
};
