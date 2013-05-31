#ifndef KAKU_DIMMER_H_
#define KAKU_DIMMER_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_dimmer;

void kakuDimInit();
void kakuDimParseRaw();
void kakuDimParseCode();
int kakuDimParseBinary();
void kakuDimCreateCode(int id, int unit, int state, int all, int dimlevel);
void kakuDimCreateLow(int s, int e);
void kakuDimCreateHigh(int s, int e);
void kakuDimClearCode();
void kakuDimCreateStart();
void kakuDimCreateId(int id);
void kakuDimCreateAll(int all);
void kakuDimCreateState(int state);
void kakuDimCreateUnit(int unit);
void kakuDimCreateDimlevel(int dimlevel);
void kakuDimCreateFooter();

#endif