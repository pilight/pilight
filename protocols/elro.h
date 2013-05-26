#ifndef ELRO_H_
#define ELRO_H_

#include "protocol.h"
#include "binary.h"

protocol elro;

void elroInit();
void elroParseRaw();
void elroParseCode();
void elroParseBinary();

#endif