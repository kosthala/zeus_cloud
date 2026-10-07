#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "config.h"
#include "settings.h"
#include "notifier.h"
#include "push_notifications.h"
#include "mail.h"
#include "time_helper.h"

typedef struct
{
    char   message[112];
    char   priority[8];     // "urgent" | "high" | "default" | "low"
    time_t ts;              // when the event happened (0 = clock not yet synced)
} NotifyEvent;

static QueueHandle_t notifyQueue = nullptr;


static void notifierTask(void *)
{
    NotifyEvent ev;

    for (;;)
    {
        if (xQueueReceive(notifyQueue, &ev, portMAX_DELAY) != pdTRUE)
            continue;

        // Hold the event until WiFi is back. New events keep queuing meanwhile.
        while (WiFi.status() != WL_CONNECTED)
            vTaskDelay(pdMS_TO_TICKS(2000));

        String when = TIME_HELPER_Format(ev.ts);

        bool ok = false;

        for (int i = 0; i < NOTIFY_RETRIES && !ok; i++)
        {
            ok = !cfg.ntfyTopic[0] || ntfy_send_advanced(cfg.ntfyTopic, "Zeus Panel", ev.message, ev.priority);

            if (!ok)
                vTaskDelay(pdMS_TO_TICKS(3000));
        }

        ok = false;

        for (int i = 0; i < NOTIFY_RETRIES && !ok; i++)
        {
            ok = MAIL_SendMail(String(ev.message), String(ev.message) + ' ' + when);

            if (!ok)
                vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}


void NOTIFIER_Init()
{
    notifyQueue = xQueueCreate(NOTIFY_QUEUE_LEN, sizeof(NotifyEvent));

    // Large stack: TLS handshake + SMTP client. Core 0 keeps loop() (core 1) free.
    xTaskCreatePinnedToCore(notifierTask, "notifier", 20480, nullptr, 1, nullptr, 0);
}


void NOTIFIER_Enqueue(const char *message, const char *priority)
{
    if (notifyQueue == nullptr)
        return;

    NotifyEvent ev;
    memset(&ev, 0, sizeof(ev));

    strlcpy(ev.message, message, sizeof(ev.message));
    strlcpy(ev.priority, priority, sizeof(ev.priority));

    time_t now = time(nullptr);
    ev.ts = (now > 100000) ? now : 0;

    if (xQueueSend(notifyQueue, &ev, 0) != pdTRUE)
    {
        // Queue full: drop the oldest event, keep the newest.
        NotifyEvent dropped;
        xQueueReceive(notifyQueue, &dropped, 0);
        xQueueSend(notifyQueue, &ev, 0);

        Serial.println("[notify] queue full, dropped oldest event");
    }
}
