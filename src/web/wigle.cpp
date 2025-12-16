// WiGLE wardriving service client implementation

#include "wigle.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SD.h>
#include <base64.h>
#include "../core/config.h"
#include "../core/sdlog.h"

// Static member initialization
char WiGLE::lastError[64] = "";
char WiGLE::statusMessage[64] = "Ready";
std::vector<String> WiGLE::uploadedFiles;
bool WiGLE::listLoaded = false;

void WiGLE::init() {
    uploadedFiles.clear();
    listLoaded = false;
    strcpy(lastError, "");
    strcpy(statusMessage, "Ready");
    loadUploadedList();
}

// ============================================================================
// WiFi Connection (Standalone)
// ============================================================================

bool WiGLE::connect() {
    if (WiFi.status() == WL_CONNECTED) {
        strcpy(statusMessage, "Already connected");
        return true;
    }
    
    String ssid = Config::wifi().otaSSID;
    String password = Config::wifi().otaPassword;
    
    if (ssid.isEmpty()) {
        strcpy(lastError, "No WiFi SSID configured");
        strcpy(statusMessage, "No WiFi SSID");
        return false;
    }
    
    strcpy(statusMessage, "Connecting...");
    Serial.printf("[WIGLE] Connecting to %s\n", ssid.c_str());
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Wait for connection with timeout
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
        delay(100);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(statusMessage, sizeof(statusMessage), "IP: %s", WiFi.localIP().toString().c_str());
        Serial.printf("[WIGLE] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    strcpy(lastError, "Connection timeout");
    strcpy(statusMessage, "Connect failed");
    Serial.println("[WIGLE] Connection failed");
    WiFi.disconnect(true);
    return false;
}

void WiGLE::disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    strcpy(statusMessage, "Disconnected");
    Serial.println("[WIGLE] Disconnected");
}

bool WiGLE::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

// ============================================================================
// Upload Tracking
// ============================================================================

bool WiGLE::loadUploadedList() {
    if (listLoaded) return true;  // Already loaded, skip SD read
    
    uploadedFiles.clear();
    
    if (!SD.exists(UPLOADED_FILE)) {
        listLoaded = true;
        return true;  // No file yet, that's OK
    }
    
    File f = SD.open(UPLOADED_FILE, FILE_READ);
    if (!f) return false;
    
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.isEmpty()) {
            uploadedFiles.push_back(line);
        }
    }
    
    f.close();
    listLoaded = true;
    Serial.printf("[WIGLE] Loaded %d uploaded files from tracking\n", uploadedFiles.size());
    return true;
}

bool WiGLE::saveUploadedList() {
    File f = SD.open(UPLOADED_FILE, FILE_WRITE);
    if (!f) return false;
    
    for (const auto& filename : uploadedFiles) {
        f.println(filename);
    }
    
    f.close();
    return true;
}

bool WiGLE::isUploaded(const char* filename) {
    loadUploadedList();
    String name = getFilenameFromPath(filename);
    for (const auto& uploaded : uploadedFiles) {
        if (uploaded == name) return true;
    }
    return false;
}

void WiGLE::markUploaded(const char* filename) {
    String name = getFilenameFromPath(filename);
    // Check if already in list
    for (const auto& uploaded : uploadedFiles) {
        if (uploaded == name) return;
    }
    uploadedFiles.push_back(name);
    saveUploadedList();
}

uint16_t WiGLE::getUploadedCount() {
    loadUploadedList();
    return uploadedFiles.size();
}

String WiGLE::getFilenameFromPath(const char* path) {
    String fullPath = path;
    int lastSlash = fullPath.lastIndexOf('/');
    if (lastSlash >= 0) {
        return fullPath.substring(lastSlash + 1);
    }
    return fullPath;
}

bool WiGLE::hasCredentials() {
    return !Config::wifi().wigleApiName.isEmpty() && 
           !Config::wifi().wigleApiToken.isEmpty();
}

// ============================================================================
// API Operations
// ============================================================================

bool WiGLE::uploadFile(const char* csvPath) {
    if (!isConnected()) {
        strcpy(lastError, "Not connected to WiFi");
        return false;
    }
    
    if (!hasCredentials()) {
        strcpy(lastError, "No WiGLE API credentials");
        return false;
    }
    
    if (!SD.exists(csvPath)) {
        snprintf(lastError, sizeof(lastError), "File not found");
        return false;
    }
    
    File csvFile = SD.open(csvPath, FILE_READ);
    if (!csvFile) {
        strcpy(lastError, "Cannot open file");
        return false;
    }
    
    size_t fileSize = csvFile.size();
    // WiGLE limit is 180MB, but we'll be more conservative on ESP32
    if (fileSize > 1000000) {  // 1MB limit for ESP32 memory
        strcpy(lastError, "File too large");
        csvFile.close();
        return false;
    }
    
    strcpy(statusMessage, "Uploading...");
    Serial.printf("[WIGLE] Uploading %s (%d bytes)\n", csvPath, fileSize);
    
    // Read file into buffer
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        strcpy(lastError, "Out of memory");
        csvFile.close();
        return false;
    }
    
    csvFile.read(buffer, fileSize);
    csvFile.close();
    
    // Build multipart form data
    String boundary = "----PorkchopWiGLE" + String(millis());
    
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate validation
    
    if (!client.connect(API_HOST, 443)) {
        strcpy(lastError, "Connection failed");
        free(buffer);
        return false;
    }
    
    // Extract filename from path
    String filename = getFilenameFromPath(csvPath);
    
    // Build Basic Auth header
    String apiName = Config::wifi().wigleApiName;
    String apiToken = Config::wifi().wigleApiToken;
    String credentials = apiName + ":" + apiToken;
    String authHeader = "Basic " + base64::encode(credentials);
    
    // Build body parts
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
    bodyStart += "Content-Type: text/csv\r\n\r\n";
    
    String bodyEnd = "\r\n--" + boundary + "--\r\n";
    
    size_t contentLength = bodyStart.length() + fileSize + bodyEnd.length();
    
    // Send request
    client.print("POST " + String(UPLOAD_PATH) + " HTTP/1.1\r\n");
    client.print("Host: " + String(API_HOST) + "\r\n");
    client.print("Authorization: " + authHeader + "\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(contentLength) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    
    client.print(bodyStart);
    client.write(buffer, fileSize);
    client.print(bodyEnd);
    
    free(buffer);
    
    // Read response
    uint32_t timeout = millis();
    while (client.connected() && !client.available() && millis() - timeout < 30000) {
        delay(10);
    }
    
    if (!client.available()) {
        strcpy(lastError, "No response");
        client.stop();
        return false;
    }
    
    // Read full response for debugging
    String statusLine = client.readStringUntil('\n');
    Serial.printf("[WIGLE] Response: %s\n", statusLine.c_str());
    
    // Skip headers to get to body
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.isEmpty()) break;
    }
    
    // Read body (JSON response)
    String body = "";
    while (client.available()) {
        body += (char)client.read();
    }
    client.stop();
    
    Serial.printf("[WIGLE] Body: %s\n", body.c_str());
    
    // Check for success (200 response and "success":true in body)
    if (statusLine.indexOf("200") > 0 && body.indexOf("\"success\":true") >= 0) {
        strcpy(statusMessage, "Upload OK");
        Serial.println("[WIGLE] Upload successful");
        SDLog::log("WIGLE", "Upload OK: %s", filename.c_str());
        
        // Mark as uploaded
        markUploaded(csvPath);
        
        return true;
    }
    
    // Parse error message if present
    int msgStart = body.indexOf("\"message\":\"");
    if (msgStart >= 0) {
        msgStart += 11;
        int msgEnd = body.indexOf("\"", msgStart);
        if (msgEnd > msgStart) {
            String errMsg = body.substring(msgStart, msgEnd);
            snprintf(lastError, sizeof(lastError), "%.60s", errMsg.c_str());
        }
    } else {
        snprintf(lastError, sizeof(lastError), "HTTP %s", statusLine.substring(9, 12).c_str());
    }
    
    strcpy(statusMessage, "Upload failed");
    SDLog::log("WIGLE", "Upload failed: %s", filename.c_str());
    Serial.printf("[WIGLE] Upload failed: %s\n", lastError);
    return false;
}

const char* WiGLE::getLastError() {
    return lastError;
}

const char* WiGLE::getStatus() {
    return statusMessage;
}

