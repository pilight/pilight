#ifndef KAKU_OLD_H_
#define KAKU_OLD_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_old;

void kakuOldInit();
void kakuOldParseRaw();
void kakuOldParseCode();
int kakuOldParseBinary();
void kakuOldCreateCode(int id, int unit, int state, int all, int dimlevel);
void kakuOldCreateUnit(int unit);
void kakuOldCreateId(int id);
void kakuOldCreateState(int state);

#endif