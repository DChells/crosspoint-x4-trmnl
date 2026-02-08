#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

#include "ConfigLoader.h"

class BatteryMonitor;

/**
 * @brief Error codes for API operations
 */
enum class ApiError {
    SUCCESS = 0,
    WIFI_NOT_CONNECTED,
    HTTP_REQUEST_FAILED,
    HTTP_UNAUTHORIZED,        // 401
    HTTP_FORBIDDEN,          // 403
    HTTP_NOT_FOUND,          // 404
    HTTP_ERROR_4XX,          // Other 4xx
    HTTP_ERROR_5XX,          // 5xx errors
    JSON_PARSE_FAILED,
    MISSING_REQUIRED_FIELD,
    IMAGE_DOWNLOAD_FAILED,
    TIMEOUT,
    INVALID_URL,
    IMAGE_TOO_LARGE
};

/**
 * @brief Status from TRMNL API JSON response
 */
enum class TrmnlStatus {
    SUCCESS = 0,
    NO_UPDATE = 202,  // 202 = no update needed
    ERROR_OTHER
};

/**
 * @brief Result structure for API operations
 */
struct ApiResult {
    ApiError error;
    String errorMessage;     ///< Human-readable error message
    int httpStatus;          ///< HTTP status code from response

    ApiResult() : error(ApiError::SUCCESS), httpStatus(0) {}
    ApiResult(ApiError err, const char* msg) : error(err), errorMessage(msg), httpStatus(0) {}
    ApiResult(ApiError err, const char* msg, int httpStat) : error(err), errorMessage(msg), httpStatus(httpStat) {}
};

/**
 * @brief Result structure for display fetch operations
 */
struct DisplayFetchResult {
    ApiResult result;                ///< API operation result
    std::vector<uint8_t> imageData;  ///< Image data buffer
    String imageUrl;                  ///< Image URL from server
    uint32_t refreshRate;            ///< Refresh rate in seconds from server
    TrmnlStatus trmnlStatus;         ///< TRMNL status from JSON response

    DisplayFetchResult() : refreshRate(1800), trmnlStatus(TrmnlStatus::SUCCESS) {}
};

/**
 * @brief TRMNL-compatible HTTP(S) API client
 *
 * Handles communication with TRMNL server:
 * - Fetch display configuration and image URL from /api/display
 * - Download image from returned URL
 * - Handle all TRMNL-specific headers and response formats
 *
 * Uses WiFiClientSecure for HTTPS connections with optional insecure TLS mode.
 */
class ApiClient {
public:
    /**
     * @brief Set the global battery monitor instance
     *
     * Must be called before fetchDisplay() to enable battery voltage reporting.
     *
     * @param battery Pointer to BatteryMonitor instance
     */
    static void setBatteryMonitor(BatteryMonitor* battery);

    /**
     * @brief Fetch display information and image from TRMNL server
     *
     * Makes GET request to {config.serverUrl}/api/display with required headers:
     * - ID: {deviceId}
     * - Access-Token: {apiKey}
     * - Refresh-Rate: {refreshInterval}
     * - Battery-Voltage: {voltage}
     * - FW-Version: 0.1.0
     * - RSSI: {wifiRssi}
     *
     * Response JSON format:
     * {
     *   "status": 0,              // 0 = success, 202 = no update
     *   "image_url": "https://...",
     *   "refresh_rate": "1800"     // May be string or int
     * }
     *
     * @param config TrmnlConfig with server URL, API key, device ID, etc.
     * @return DisplayFetchResult Contains image data, metadata, and status
     */
    static DisplayFetchResult fetchDisplay(const TrmnlConfig& config);

private:
    /**
     * @brief Build full API URL from server URL
     */
    static String buildApiUrl(const String& serverUrl);

    /**
     * @brief Get battery voltage as string
     *
     * Uses BatteryMonitor SDK to read current voltage.
     *
     * @return Battery voltage in volts as String, or "0.0" if not available
     */
    static String getBatteryVoltage();

    /**
     * @brief Get WiFi RSSI as string
     *
     * @return RSSI in dBm as String, or "0" if not connected
     */
    static String getWifiRssi();

    /**
     * @brief Parse JSON response from API
     *
     * @param responseBody JSON string from API
     * @param imageUrl Output parameter for image URL
     * @param refreshRate Output parameter for refresh rate
     * @param trmnlStatus Output parameter for TRMNL status code
     * @return ApiResult Result of parsing operation
     */
    static ApiResult parseApiResponse(const String& responseBody,
                                       String& imageUrl,
                                       uint32_t& refreshRate,
                                       TrmnlStatus& trmnlStatus);

    /**
     * @brief Download image from URL into buffer
     *
     * @param imageUrl Full URL to image
     * @param imageData Output vector for image data
     * @param config TrmnlConfig for TLS settings
     * @return ApiResult Result of download operation
     */
    static ApiResult downloadImage(const String& imageUrl,
                                   std::vector<uint8_t>& imageData,
                                   const TrmnlConfig& config);

    static constexpr uint32_t API_TIMEOUT_MS = 30000;     // 30 seconds for API call
    static constexpr uint32_t IMAGE_TIMEOUT_MS = 60000;    // 60 seconds for image download
    static constexpr size_t MAX_IMAGE_SIZE = 10 * 1024 * 1024;  // 10 MB max image size
     static constexpr const char* FW_VERSION = "0.1.0";
};
