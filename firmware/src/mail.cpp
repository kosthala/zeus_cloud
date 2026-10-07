#include <ESP_Mail_Client.h>
#include "settings.h"
#include "mail.h"

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT esp_mail_smtp_port_465   // SSL

static SMTPSession smtp;
static Session_Config config;


static void smtpCallback(SMTP_Status status)
{
    Serial.println(status.info());

    if (!status.success())
        Serial.printf("[mail] Error: %d, %s\n", smtp.statusCode(), smtp.errorReason().c_str());
}


// Called from the notifier task only (never from loop()).
bool MAIL_SendMail(const String &subject, const String &content)
{
    if (!cfg.mailUser[0] || !cfg.mailPass[0] || !cfg.mailTo[0])
        return true;   // email not configured

    SMTP_Message message;

    MailClient.networkReconnect(false);   // WiFi reconnection is handled in main.cpp

    smtp.debug(0);
    smtp.callback(smtpCallback);
    smtp.setTCPTimeout(10);

    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = cfg.mailUser;
    config.login.password = cfg.mailPass;
    config.login.user_domain = "127.0.0.1";

    message.sender.name = "Zeus Cloud";
    message.sender.email = cfg.mailUser;
    message.addRecipient("Zeus", cfg.mailTo);
    message.subject = subject;
    message.text.content = content;

    if (!smtp.connect(&config))
    {
        Serial.printf("[mail] Connection error, status %d, code %d, reason: %s\n",
                      smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        return false;
    }

    if (!MailClient.sendMail(&smtp, &message))
    {
        Serial.printf("[mail] Send error, status %d, code %d, reason: %s\n",
                      smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        return false;
    }

    return true;
}
