#ifndef ELRO_H_
#define ELRO_H_

#include "protocol.h"
#include "binary.h"

protocol elro;

void elroInit();
void elroParseRaw();
void elroParseCode();
void elroParseBinary();
void elroCreateCode(int id, int unit, int state, int all, int dimlevel);
void elroCreateUnit(int unit);
void elroCreateId(int id);
void elroCreateState(int state);

#endif