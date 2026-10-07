#ifndef MAIL_H
#define MAIL_H

#include <Arduino.h>

bool MAIL_SendMail(const String &subject, const String &content);

#endif
