#ifndef KAKU_SWITCH_H_
#define KAKU_SWITCH_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_switch;

void kakuSwInit();
void kakuSwParseRaw();
void kakuSwParseCode();
void kakuSwParseBinary();

#endif