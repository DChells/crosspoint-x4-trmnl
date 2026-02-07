#include "ApiClient.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../open-x4-sdk/libs/hardware/BatteryMonitor/include/BatteryMonitor.h"

// Global battery monitor instance - initialized in main task (not yet implemented)
// For now, return default values if not initialized
static BatteryMonitor* g_batteryMonitor = nullptr;

void ApiClient::setBatteryMonitor(BatteryMonitor* battery) {
    g_batteryMonitor = battery;
}

String ApiClient::buildApiUrl(const String& serverUrl) {
    String url = serverUrl;
    if (url.endsWith("/")) {
        url = url.substring(0, url.length() - 1);
    }
    return url + "/api/display";
}

String ApiClient::getBatteryVoltage() {
    if (g_batteryMonitor != nullptr) {
        double volts = g_batteryMonitor->readVolts();
        return String(volts, 1);
    }
    return "0.0";
}

String ApiClient::getWifiRssi() {
    if (WiFi.status() == WL_CONNECTED) {
        return String(WiFi.RSSI());
    }
    return "0";
}

DisplayFetchResult ApiClient::fetchDisplay(const TrmnlConfig& config) {
    DisplayFetchResult result;

    if (WiFi.status() != WL_CONNECTED) {
        result.result = ApiResult(ApiError::WIFI_NOT_CONNECTED,
                                   "WiFi not connected");
        return result;
    }

    String url = buildApiUrl(config.serverUrl);
    WiFiClientSecure client;

    if (config.useInsecureTls) {
        client.setInsecure();
    }

    HTTPClient http;
    http.setTimeout(API_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        result.result = ApiResult(ApiError::INVALID_URL,
                                   "Failed to begin HTTP request");
        return result;
    }

    http.addHeader("ID", config.deviceId);
    http.addHeader("Access-Token", config.apiKey);
    http.addHeader("Refresh-Rate", String(config.refreshInterval));
    http.addHeader("Battery-Voltage", getBatteryVoltage());
    http.addHeader("FW-Version", FW_VERSION);
    http.addHeader("RSSI", getWifiRssi());

    int httpCode = http.GET();
    result.result.httpStatus = httpCode;

    if (httpCode == HTTP_CODE_OK || httpCode == 202) {
        String responseBody = http.getString();

        ApiResult parseResult = parseApiResponse(responseBody,
                                                   result.imageUrl,
                                                   result.refreshRate,
                                                   result.trmnlStatus);

        if (parseResult.error != ApiError::SUCCESS) {
            result.result = parseResult;
            http.end();
            return result;
        }

        http.end();

        if (result.trmnlStatus == TrmnlStatus::NO_UPDATE) {
            result.result = ApiResult(ApiError::SUCCESS,
                                       "No update available",
                                       httpCode);
            return result;
        }

        if (!result.imageUrl.isEmpty()) {
            ApiResult downloadResult = downloadImage(result.imageUrl,
                                                     result.imageData,
                                                     config);

            if (downloadResult.error != ApiError::SUCCESS) {
                result.result = downloadResult;
                return result;
            }
        } else {
            result.result = ApiResult(ApiError::MISSING_REQUIRED_FIELD,
                                       "Image URL not found in response");
            return result;
        }

        result.result = ApiResult(ApiError::SUCCESS,
                                   "Display fetched successfully",
                                   httpCode);

    } else if (httpCode == HTTP_CODE_UNAUTHORIZED) {
        result.result = ApiResult(ApiError::HTTP_UNAUTHORIZED,
                                   "Unauthorized: Invalid API key",
                                   httpCode);

    } else if (httpCode == HTTP_CODE_FORBIDDEN) {
        result.result = ApiResult(ApiError::HTTP_FORBIDDEN,
                                   "Forbidden: Access denied",
                                   httpCode);

    } else if (httpCode == HTTP_CODE_NOT_FOUND) {
        result.result = ApiResult(ApiError::HTTP_NOT_FOUND,
                                   "API endpoint not found",
                                   httpCode);

    } else if (httpCode >= 400 && httpCode < 500) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "HTTP 4xx error: %d", httpCode);
        result.result = ApiResult(ApiError::HTTP_ERROR_4XX,
                                   errorMsg,
                                   httpCode);

    } else if (httpCode >= 500) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "HTTP 5xx error: %d", httpCode);
        result.result = ApiResult(ApiError::HTTP_ERROR_5XX,
                                   errorMsg,
                                   httpCode);

    } else {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "HTTP request failed: %d", httpCode);
        result.result = ApiResult(ApiError::HTTP_REQUEST_FAILED,
                                   errorMsg,
                                   httpCode);
    }

    http.end();
    return result;
}

ApiResult ApiClient::parseApiResponse(const String& responseBody,
                                        String& imageUrl,
                                        uint32_t& refreshRate,
                                        TrmnlStatus& trmnlStatus) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, responseBody);

    if (error) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "JSON parse error: %s", error.c_str());
        return ApiResult(ApiError::JSON_PARSE_FAILED, errorMsg);
    }

    if (doc["status"].is<int>()) {
        int status = doc["status"].as<int>();
        trmnlStatus = static_cast<TrmnlStatus>(status);

        if (trmnlStatus == TrmnlStatus::NO_UPDATE) {
            return ApiResult(ApiError::SUCCESS, "");
        }
    } else {
        return ApiResult(ApiError::MISSING_REQUIRED_FIELD,
                          "Missing required field: status");
    }

    if (doc["image_url"].is<const char*>()) {
        imageUrl = doc["image_url"].as<String>();
    } else {
        return ApiResult(ApiError::MISSING_REQUIRED_FIELD,
                          "Missing required field: image_url");
    }

    if (doc["refresh_rate"].is<const char*>()) {
        refreshRate = doc["refresh_rate"].as<uint32_t>();
    } else if (doc["refresh_rate"].is<int>()) {
        refreshRate = doc["refresh_rate"].as<uint32_t>();
    } else {
        refreshRate = 1800;
    }

    return ApiResult(ApiError::SUCCESS, "");
}

ApiResult ApiClient::downloadImage(const String& imageUrl,
                                    std::vector<uint8_t>& imageData,
                                    const TrmnlConfig& config) {
    WiFiClientSecure client;

    if (config.useInsecureTls) {
        client.setInsecure();
    }

    HTTPClient http;
    http.setTimeout(IMAGE_TIMEOUT_MS);

    if (!http.begin(client, imageUrl)) {
        return ApiResult(ApiError::INVALID_URL,
                          "Failed to begin image download");
    }

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();

        if (contentLength <= 0) {
            http.end();
            return ApiResult(ApiError::IMAGE_DOWNLOAD_FAILED,
                              "Content length not available");
        }

        if ((size_t)contentLength > MAX_IMAGE_SIZE) {
            http.end();
            return ApiResult(ApiError::IMAGE_TOO_LARGE,
                              "Image exceeds maximum size");
        }

        imageData.resize(contentLength);
        WiFiClient* stream = http.getStreamPtr();
        
        size_t bytesRead = 0;
        unsigned long startTime = millis();
        while (http.connected() && (bytesRead < (size_t)contentLength) && (millis() - startTime < IMAGE_TIMEOUT_MS)) {
            size_t available = stream->available();
            if (available) {
                int read = stream->read(imageData.data() + bytesRead, available);
                if (read > 0) {
                    bytesRead += read;
                    startTime = millis(); // Reset timeout on successful read
                }
            }
            delay(1);
        }

        http.end();

        if (bytesRead != (size_t)contentLength) {
            imageData.clear();
            return ApiResult(ApiError::IMAGE_DOWNLOAD_FAILED,
                              "Failed to read complete image data");
        }

        return ApiResult(ApiError::SUCCESS,
                          "Image downloaded successfully",
                          httpCode);

    } else {
        http.end();
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg),
                 "Image download failed with HTTP code: %d", httpCode);
        return ApiResult(ApiError::IMAGE_DOWNLOAD_FAILED, errorMsg, httpCode);
    }
}
