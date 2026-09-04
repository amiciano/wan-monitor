#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "secrets.h"

// Configuration

constexpr unsigned long POLL_INTERVAL_MS =
    10UL * 60UL * 1000UL; // 10 minutes

enum WanSpeed
{
    WAN_UNKNOWN,
    WAN_100,
    WAN_1000
};

WanSpeed lastWanSpeed = WAN_UNKNOWN;

// Wi-Fi

void connectWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.print("Connecting to Wi-Fi");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Wi-Fi connected.");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
}


// WAN HTML Parser

WanSpeed getWanSpeed(const String& html)
{
    const String wanMarker =
        "<span class=\"thead\">WAN</span>";

    int wanPosition = html.indexOf(wanMarker);

    if (wanPosition == -1)
    {
        return WAN_UNKNOWN;
    }

    // We only care about the HTML immediately following
    // the WAN label.
    int endPosition = min(
        wanPosition + 400,
        static_cast<int>(html.length())
    );

    String wanSection =
        html.substring(wanPosition, endPosition);

    if (wanSection.indexOf("1000M/Full") != -1)
    {
        return WAN_1000;
    }

    if (wanSection.indexOf("100M/Full") != -1)
    {
        return WAN_100;
    }

    return WAN_UNKNOWN;
}


// ntfy Notification

void sendNotification(const String& message)
{
    WiFiClientSecure secureClient;
    secureClient.setInsecure();

    HTTPClient http;

    String url =
        "https://ntfy.sh/" + String(NTFY_TOPIC);

    if (!http.begin(secureClient, url))
    {
        Serial.println("Could not start ntfy request.");
        return;
    }

    http.addHeader("Content-Type", "text/plain");

    int responseCode = http.POST(message);

    Serial.print("ntfy response: ");
    Serial.println(responseCode);

    http.end();
}


// Check Router

void checkWan()
{
    connectWiFi();

    Serial.println();
    Serial.println("Checking WAN link...");

    HTTPClient http;

    http.begin(ROUTER_STATS_URL);

    // Same HTTP Basic Authentication as curl -u
    http.setAuthorization(
        ROUTER_USERNAME,
        ROUTER_PASSWORD
    );

    int responseCode = http.GET();

    Serial.print("Router HTTP response: ");
    Serial.println(responseCode);

    if (responseCode != 200)
    {
        Serial.println("Failed to retrieve router statistics.");
        http.end();
        return;
    }

    String html = http.getString();

    http.end();

    WanSpeed currentSpeed = getWanSpeed(html);

    switch (currentSpeed)
    {
        case WAN_1000:
            Serial.println("WAN: 1000 Mbps / Full Duplex");
            break;

        case WAN_100:
            Serial.println("WAN: 100 Mbps / Full Duplex");
            break;

        default:
            Serial.println("WAN: UNKNOWN");
            return;
    }


    // State-change notifications

    // Device started while WAN was already degraded
    if (lastWanSpeed == WAN_UNKNOWN &&
        currentSpeed == WAN_100)
    {
        sendNotification(
            "WAN WARNING: Router WAN link is running at 100 Mbps."
        );
    }

    // Gigabit -> 100 Mbps
    else if (lastWanSpeed == WAN_1000 &&
             currentSpeed == WAN_100)
    {
        sendNotification(
            "WAN WARNING: Link dropped from 1000 Mbps to 100 Mbps."
        );
    }

    // 100 Mbps -> Gigabit
    else if (lastWanSpeed == WAN_100 &&
             currentSpeed == WAN_1000)
    {
        sendNotification(
            "WAN restored: Link is back at 1000 Mbps."
        );
    }

    lastWanSpeed = currentSpeed;
}


// Arduino entry points

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("=======================");
    Serial.println(" Netgear WAN Monitor");
    Serial.println("=======================");

    connectWiFi();

    // Immediately check on startup
    checkWan();
}


void loop()
{
    delay(POLL_INTERVAL_MS);

    checkWan();
}