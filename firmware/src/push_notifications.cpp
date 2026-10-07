#include <WiFiClientSecure.h>
#include "config.h"
#include "certs.h"
#include "push_notifications.h"
#include "time_helper.h"

static const char *NTFY_HOST = "ntfy.sh";
static const int   NTFY_PORT = 443;


static bool ntfy_post(const char *topic, const char *title, const char *message, const char *priority)
{
    WiFiClientSecure client;

#if NTFY_VERIFY_TLS
    // Certificate validation needs a valid clock; fall back only if time isn't synced yet.
    if (TIME_HELPER_IsSynced())
        client.setCACert(ISRG_ROOT_X1);
    else
        client.setInsecure();
#else
    client.setInsecure();
#endif

    client.setHandshakeTimeout(10);

    if (!client.connect(NTFY_HOST, NTFY_PORT, 8000))
    {
        Serial.println("[ntfy] Connection failed");
        return false;
    }

    String payload = String(message);

    String request = String("POST /") + topic + " HTTP/1.1\r\n" +
                     "Host: " + NTFY_HOST + "\r\n" +
                     "Content-Type: text/plain\r\n";

    if (title != nullptr)
        request += String("X-Title: ") + title + "\r\n";

    if (priority != nullptr)
        request += String("X-Priority: ") + priority + "\r\n";

    request += String("Content-Length: ") + payload.length() + "\r\n" +
               "Connection: close\r\n\r\n" +
               payload;

    client.print(request);

    // Wait (bounded) for the status line instead of reading until the server closes.
    uint32_t t0 = millis();

    while (!client.available() && client.connected() && (millis() - t0) < 8000)
        delay(20);

    String status = client.readStringUntil('\n');
    client.stop();

    bool ok = status.indexOf(" 200") >= 0;

    Serial.printf("[ntfy] %s (%s)\n", ok ? "sent" : "FAILED", status.c_str());

    return ok;
}


bool ntfy_send(const char *topic, const char *message)
{
    return ntfy_post(topic, nullptr, message, nullptr);
}


bool ntfy_send_advanced(const char *topic, const char *title, const char *message, const char *priority)
{
    return ntfy_post(topic, title, message, priority);
}
