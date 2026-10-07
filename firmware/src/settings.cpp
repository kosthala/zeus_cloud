#include <Preferences.h>
#include "settings.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef NTFY_TOPIC
#define NTFY_TOPIC ""
#endif
#ifndef AUTHOR_EMAIL
#define AUTHOR_EMAIL ""
#endif
#ifndef AUTHOR_PASSWORD
#define AUTHOR_PASSWORD ""
#endif
#ifndef RECIPIENT_EMAIL
#define RECIPIENT_EMAIL ""
#endif
#ifndef WM_AP_NAME
#define WM_AP_NAME "ZeusSetup"
#endif
#ifndef WM_AP_PASSWORD
#define WM_AP_PASSWORD "zeus-setup"
#endif

Settings cfg;
const char *const SETUP_AP_NAME     = WM_AP_NAME;
const char *const SETUP_AP_PASSWORD = WM_AP_PASSWORD;

static void ld(Preferences &p, const char *key, char *dst, size_t n, const char *def)
{
    if (p.isKey(key))
    {
        p.getString(key, dst, n);
    }
    else
    {
        strlcpy(dst, def, n);

        if (def[0])
            p.putString(key, def);   // first boot: seed NVS from the compile-time value
    }
}


void SETTINGS_Load()
{
    Preferences p;
    p.begin("zeus", false);

    ld(p, "mq_host", cfg.mqttHost,  sizeof(cfg.mqttHost),  MQTT_HOST);
    ld(p, "mq_user", cfg.mqttUser,  sizeof(cfg.mqttUser),  MQTT_USER);
    ld(p, "mq_pass", cfg.mqttPass,  sizeof(cfg.mqttPass),  MQTT_PASSWORD);
    ld(p, "ntfy",    cfg.ntfyTopic, sizeof(cfg.ntfyTopic), NTFY_TOPIC);
    ld(p, "ml_user", cfg.mailUser,  sizeof(cfg.mailUser),  AUTHOR_EMAIL);
    ld(p, "ml_pass", cfg.mailPass,  sizeof(cfg.mailPass),  AUTHOR_PASSWORD);
    ld(p, "ml_to",   cfg.mailTo,    sizeof(cfg.mailTo),    RECIPIENT_EMAIL);

    if (p.isKey("mq_port"))
    {
        cfg.mqttPort = p.getUShort("mq_port", MQTT_PORT);
    }
    else
    {
        cfg.mqttPort = MQTT_PORT;
        p.putUShort("mq_port", cfg.mqttPort);
    }

    p.end();
}


void SETTINGS_Save()
{
    Preferences p;
    p.begin("zeus", false);

    p.putString("mq_host", cfg.mqttHost);
    p.putUShort("mq_port", cfg.mqttPort);
    p.putString("mq_user", cfg.mqttUser);
    p.putString("mq_pass", cfg.mqttPass);
    p.putString("ntfy",    cfg.ntfyTopic);
    p.putString("ml_user", cfg.mailUser);
    p.putString("ml_pass", cfg.mailPass);
    p.putString("ml_to",   cfg.mailTo);

    p.end();
}


bool SETTINGS_Complete()
{
    return cfg.mqttHost[0] != 0 && cfg.mqttUser[0] != 0;
}
