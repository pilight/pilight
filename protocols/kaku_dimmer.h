#ifndef KAKU_DIMMER_H_
#define KAKU_DIMMER_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_dimmer;

void kakuDimInit();
void kakuDimParseRaw();
void kakuDimParseCode();
void kakuDimParseBinary();

#endif