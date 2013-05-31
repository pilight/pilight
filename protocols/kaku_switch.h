#ifndef KAKU_SWITCH_H_
#define KAKU_SWITCH_H_

#include "protocol.h"
#include "binary.h"

protocol kaku_switch;

void kakuSwInit();
void kakuSwParseRaw();
void kakuSwParseCode();
void kakuSwParseBinary();
void kakuSwCreateCode(int id, int unit, int state, int all, int dimlevel);
void kakuSwCreateLow(int s, int e);
void kakuSwCreateHigh(int s, int e);
void kakuSwClearCode();
void kakuSwCreateStart();
void kakuSwCreateId(int id);
void kakuSwCreateAll(int all);
void kakuSwCreateState(int state);
void kakuSwCreateUnit(int unit);
void kakuSwCreateFooter();

#endif