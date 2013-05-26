#include "protocol.h"

void protocol_unregister(protocol *proto) {
	unsigned i;

	for(i=0; i<protos.nr; ++i) {
		if(strcmp(protos.listeners[i]->id,proto->id) == 0) {
		  protos.nr--;
		  protos.listeners[i] = protos.listeners[protos.nr];
		}
	}
}

void protocol_register(protocol *proto) {
	protocol_unregister(proto);
	protos.listeners[protos.nr++] = proto;
}