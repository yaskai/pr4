#ifndef LOG_MESSAGE_H_
#define LOG_MESSAGE_H_

#include "ansi_codes.h"

void SetLogState(short state);
short GetLogState();

void Message(char *text, char *color);
void MessageError(char *text, char *param);
void MessageDiag(char *text, char *param, char *color);
void MessageKeyValPair(char *key, char *val);

#endif
