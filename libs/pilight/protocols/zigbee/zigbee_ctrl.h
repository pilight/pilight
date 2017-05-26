/*
 * zigbee_ctrl.h
 *
 *  Created on: Nov 10, 2016
 *      Author: ma-ca
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _PROTOCOL_ZIGBEE_CTRL_H_
#define _PROTOCOL_ZIGBEE_CTRL_H_

#include "../protocol.h"

struct protocol_t *zigbee_ctrl;

void zigbeeCtrlInit(void);

#endif /* _PROTOCOL_ZIGBEE_CTRL_H_ */
