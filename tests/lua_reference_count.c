/*
	Copyright (C) 2013 - 2016 CurlyMo

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "../libs/pilight/core/log.h"
#include "../libs/pilight/core/CuTest.h"
#include "../libs/pilight/core/pilight.h"
#include "../libs/pilight/core/common.h"
#include "../libs/pilight/core/mqtt.h"
#include "../libs/pilight/lua_c/lua.h"
#include "../libs/pilight/lua_c/lualibrary.h"
#include "../libs/pilight/lua_c/async/timer.h"
#include "../libs/pilight/lua_c/async/thread.h"
#include "../libs/pilight/lua_c/async/event.h"
#include "../libs/pilight/lua_c/network/mqtt.h"

static int run = 0;
static int test = 0;
static int stopped = 0;
static int testing_plua_reference_count = 0;
static CuTest *gtc = NULL;
static uv_timer_t *timer_req = NULL;

static void *ptr1 = NULL;
static void *ptr2 = NULL;

static int plua_print(lua_State* L) {
	// printf("%d= ", run);
	// plua_stack_dump(L);

	CuAssertIntEquals(gtc, stopped, 0);
	switch(test) {
		case 1:
		case 2:
		case 3:
		case 4: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("timer", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("timer1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("timer", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("timer1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					if(test == 4) {
						CuAssertTrue(gtc, strcmp("timer1", lua_tostring(L, -1)) == 0 || strcmp("timer", lua_tostring(L, -1)) == 0);
					} else {
						CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
					}
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					if(test == 4) {
						CuAssertTrue(gtc, strcmp("timer1", lua_tostring(L, -1)) == 0 || strcmp("timer", lua_tostring(L, -1)) == 0);
					} else {
						CuAssertStrEquals(gtc, "timer1", lua_tostring(L, -1));
					}
				} break;
				case 9: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "timer1", lua_tostring(L, -1));
				} break;
			}
		} break;
		case 5: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("timer", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					}
				} break;
				case 2: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
				} break;
			}
		} break;
		case 10:
		case 11:
		case 12:
		case 13: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("thread", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("thread1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("thread", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("thread1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "thread", lua_tostring(L, -1));
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "thread1", lua_tostring(L, -1));
				} break;
				case 7:
				case 8: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "thread1", lua_tostring(L, -1));
				} break;
			}
		} break;
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("event", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("event1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("event", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("event1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "event", lua_tostring(L, -1));
				} break;
				case 5: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "event", lua_tostring(L, -1));
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "event", lua_tostring(L, -1));
				} break;
				case 8: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "event1", lua_tostring(L, -1));
				} break;
			}
		} break;
		case 30:
		case 31:
		case 32:
		case 34: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("mqtt", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("mqtt1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("mqtt", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("mqtt1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "mqtt", lua_tostring(L, -1));
				} break;
				case 4: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					if(test == 32) {
						CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
					} else {
						CuAssertStrEquals(gtc, "mqtt", lua_tostring(L, -1));
					}
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "mqtt", lua_tostring(L, -1));
				} break;
			}
		} break;
		case 33: {
			switch(run++) {
				case 0: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("mqtt", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("mqtt1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 1: {
					CuAssertIntEquals(gtc, LUA_TLIGHTUSERDATA, lua_type(L, -1));
					if(strcmp("mqtt", lua_tostring(L, -2)) == 0) {
						ptr1 = lua_touserdata(L, -1);
					} else if(strcmp("mqtt1", lua_tostring(L, -2)) == 0) {
						ptr2 = lua_touserdata(L, -1);
					}
				} break;
				case 3: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "timer", lua_tostring(L, -1));
				} break;
				case 6: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "mqtt", lua_tostring(L, -1));
				} break;
				case 7: {
					CuAssertIntEquals(gtc, LUA_TSTRING, lua_type(L, -1));
					CuAssertStrEquals(gtc, "mqtt", lua_tostring(L, -1));
				} break;
			}
		} break;
	}
	return 0;
}

void plua_network_mqtt_gc(void *ptr) {
	if(testing_plua_reference_count == 1 && test != 34) {
		CuAssertIntEquals(gtc, stopped, 0);
	}
	struct lua_mqtt_t *lua_mqtt = ptr;

	if(lua_mqtt != NULL) {
		switch(test) {
			case 30: {
				switch(run++) {
					case 0: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 1);
					} break;
				}
			} break;
			case 31: {
				switch(run++) {
					case 1: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 2);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 1);
					} break;
				}
			} break;
			case 32: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 3);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 2);
					} break;
					case 7: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 1);
					} break;
				}
			} break;
			case 33: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 2);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 3);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 2);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 1);
					} break;
				}
			} break;
			case 34: {
				switch(run++) {
					case 1: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 3);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 2);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr1 == lua_mqtt);
						CuAssertIntEquals(gtc, lua_mqtt->ref, 1);
					} break;
				}
			} break;
		}
		// printf("%s: %d %d\n", ((lua_mqtt == ptr1) ? "mqtt" : "mqtt1"), lua_mqtt->ref-1, run-1);
		int x = 0;
		if((x = atomic_dec(lua_mqtt->ref)) == 0) {
			plua_metatable_free(lua_mqtt->table);
			if(lua_mqtt->callback != NULL) {
				FREE(lua_mqtt->callback);
			}
			lua_mqtt->gc = NULL;
			plua_gc_unreg(NULL, lua_mqtt);
			FREE(lua_mqtt);
			lua_mqtt = NULL;
		}
		assert(x >= 0);
	}
}

void plua_network_mqtt_global_gc(void *ptr) {
	plua_network_mqtt_gc(ptr);
}

void plua_async_event_gc(void *ptr) {
	if(testing_plua_reference_count == 1) {
		if(test != 21 && test != 23 && test != 26 && test != 27 && test != 34) {
			CuAssertIntEquals(gtc, stopped, 0);
		}
	}

	struct lua_event_t *lua_event = ptr;

	if(lua_event != NULL) {
		switch(test) {
			case 20: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
				}
			} break;
			case 21: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 1);
					} break;
				};
			} break;
			case 22: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event || ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event || ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
					case 7: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 10: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 11: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
				}
			} break;
			case 23: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event || ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event || ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 9: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 10: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 1);
					} break;
					case 11: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 1);
					} break;
				}
			} break;
			case 24: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event || ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event || ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 3);
					} break;
					case 7: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 10: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 11: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
				}
			} break;
			case 25: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
					} break;
				}
			} break;
			case 26:
			case 27: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 2);
						CuAssertIntEquals(gtc, stopped, 0);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr1 == lua_event);
						CuAssertIntEquals(gtc, lua_event->ref, 1);
						CuAssertIntEquals(gtc, stopped, 1);
					} break;
				}
			} break;
		}
		int x = 0;
		if((x = atomic_dec(lua_event->ref)) == 0) {
			plua_metatable_free(lua_event->table);
			if(lua_event->callback != NULL) {
				FREE(lua_event->callback);
			}
			plua_gc_unreg(NULL, lua_event);
			FREE(lua_event);
		}
		assert(x >= 0);
	}
}

void plua_async_event_global_gc(void *ptr) {
	plua_async_event_gc(ptr);
}

void plua_async_timer_gc(void *ptr) {
	if(testing_plua_reference_count == 1) {
		CuAssertIntEquals(gtc, stopped, 0);
	}

	struct lua_timer_t *lua_timer = ptr;
	if(lua_timer != NULL) {
		switch(test) {
			case 1: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
				}
			} break;
			case 2: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr1 == lua_timer || ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr2 == lua_timer || ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
				};
			} break;
			case 3: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_timer || ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_timer || ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 3);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
					case 9: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
				}
			} break;
			case 4: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_timer || ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_timer || ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 8: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
					case 10: {
						CuAssertTrue(gtc, ptr2 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
				}
			} break;
			case 5: {
				switch(run++) {
					case 1: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 2);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr1 == lua_timer);
						CuAssertIntEquals(gtc, lua_timer->ref, 1);
					} break;
				}
			} break;
		}
		// printf("%s: %d %d\n", ((lua_timer == ptr1) ? "timer" : "timer1"), lua_timer->ref-1, run-1);
		int x = 0;
		if((x = atomic_dec(lua_timer->ref)) == 0) {
			if(lua_timer->callback != NULL) {
				FREE(lua_timer->callback);
			}
			if(lua_timer->table != NULL) {
				plua_metatable_free(lua_timer->table);
			}
			lua_timer->gc = NULL;
			plua_gc_unreg(NULL, lua_timer);
			FREE(lua_timer);
			lua_timer = NULL;
		} else {
			lua_timer->running = 0;
		}
		assert(x >= 0);
	}
}

void plua_async_timer_global_gc(void *ptr) {
	plua_async_timer_gc(ptr);
}

void thread_free(uv_work_t *req, int status) {
	if(testing_plua_reference_count == 1) {
		CuAssertIntEquals(gtc, stopped, 0);
	}

	struct lua_thread_t *lua_thread = req->data;

	if(lua_thread != NULL) {
		switch(test) {
			case 10: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
				}
			} break;
			case 11: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
					case 4: {
						CuAssertTrue(gtc, ptr1 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
				};
			} break;
			case 12: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_thread || ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_thread || ptr1 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 3);
					} break;
					case 6: {
						if(ptr1 == lua_thread) {
							CuAssertTrue(gtc, ptr1 == lua_thread);
							CuAssertIntEquals(gtc, lua_thread->ref, 1);
						} else {
							CuAssertTrue(gtc, ptr2 == lua_thread);
							CuAssertIntEquals(gtc, lua_thread->ref, 2);
						}
					} break;
					case 8: {
						if(ptr1 == lua_thread) {
							CuAssertTrue(gtc, ptr1 == lua_thread);
							CuAssertIntEquals(gtc, lua_thread->ref, 1);
						} else {
							CuAssertTrue(gtc, ptr2 == lua_thread);
							CuAssertIntEquals(gtc, lua_thread->ref, 2);
						}
					} break;
					case 9: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
				}
			} break;
			case 13: {
				switch(run++) {
					case 2: {
						CuAssertTrue(gtc, ptr1 == lua_thread || ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 3: {
						CuAssertTrue(gtc, ptr2 == lua_thread || ptr1 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 5: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 6: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 2);
					} break;
					case 7: {
						CuAssertTrue(gtc, ptr1 == lua_thread || ptr2 == lua_thread);
						CuAssertTrue(gtc, lua_thread->ref == 1 || lua_thread->ref == 2);
					} break;
					case 9: {
						CuAssertTrue(gtc, ptr2 == lua_thread);
						CuAssertIntEquals(gtc, lua_thread->ref, 1);
					} break;
				}
			} break;
		}
		// printf("%s: %d %d\n", ((lua_thread == ptr1) ? "thread" : "thread1"), lua_thread->ref-1, run-1);
		int x = 0;
		if((x = atomic_dec(lua_thread->ref)) == 0) {
			if(lua_thread->table != NULL) {
				plua_metatable_free(lua_thread->table);
			}
			if(lua_thread->work_req != NULL) {
				FREE(lua_thread->work_req);
			}
			if(lua_thread->callback != NULL) {
				FREE(lua_thread->callback);
			}
			lua_thread->gc = NULL;
			plua_gc_unreg(NULL, lua_thread);
			FREE(lua_thread);
			lua_thread = NULL;
		}
		assert(x >= 0);
	}
}

void plua_async_thread_global_gc(void *ptr) {
	thread_free(ptr, -99);
}

static int call(struct lua_State *L, char *file, char *func) {
	lua_getfield(L, -1, func);
	if(lua_type(L, -1) != LUA_TFUNCTION) {
		return -1;
	}

	if(plua_pcall(L, file, 0, 0) == -1) {
		assert(plua_check_stack(L, 0) == 0);
		return -1;
	}
	lua_pop(L, 1);

	return 1;
}

static void close_cb(uv_handle_t *handle) {
	if(handle != NULL) {
		FREE(handle);
	}
}

static void walk_cb(uv_handle_t *handle, void *arg) {
	if(!uv_is_closing(handle)) {
		uv_close(handle, close_cb);
	}
}

static void stop(uv_timer_t *req) {
	stopped = 1;
	uv_timer_stop(req);

	mqtt_gc();
	uv_stop(uv_default_loop());
}

static void test_lua_reference_count(CuTest *tc, char *mod, int time) {
	testing_plua_reference_count = 1;
	struct lua_state_t *state = NULL;
	struct lua_State *L = NULL;
	char path[1024], *p = path, name[255];
	char *file = NULL;

	stopped = 0;
	run = 0;

	gtc = tc;

	eventpool_init(EVENTPOOL_NO_THREADS);
	plua_init();
	plua_override_global("print", plua_print);
	plua_pause_coverage(1);

	file = STRDUP(__FILE__);
	CuAssertPtrNotNull(tc, file);

	str_replace("lua_reference_count.c", "", &file);

	memset(p, 0, 1024);
	snprintf(p, 1024, "%srefcount_%s.lua", file, mod);
	FREE(file);
	file = NULL;

	plua_module_load(path, UNITTEST);

	CuAssertIntEquals(tc, 0, plua_module_exists("refcount", UNITTEST));

	if((timer_req = MALLOC(sizeof(uv_timer_t))) == NULL) {
		OUT_OF_MEMORY /*LCOV_EXCL_LINE*/
	}

	uv_timer_init(uv_default_loop(), timer_req);
	uv_timer_start(timer_req, (void (*)(uv_timer_t *))stop, time, 0);

	state = plua_get_free_state();
	CuAssertPtrNotNull(tc, state);
	CuAssertPtrNotNull(tc, (L = state->L));

	p = name;

	sprintf(name, "unittest.%s", "refcount");
	lua_getglobal(L, name);
	CuAssertIntEquals(tc, LUA_TTABLE, lua_type(L, -1));

	struct plua_module_t *tmp = plua_get_modules();
	while(tmp) {
		if(strcmp("refcount", tmp->name) == 0) {
			file = tmp->file;
			state->module = tmp;
			break;
		}
		tmp = tmp->next;
	}
	CuAssertPtrNotNull(tc, file);
	CuAssertIntEquals(tc, 1, call(L, file, "run"));
	lua_pop(L, -1);

	plua_clear_state(state);

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	uv_walk(uv_default_loop(), walk_cb, NULL);
	uv_run(uv_default_loop(), UV_RUN_ONCE);

	while(uv_loop_close(uv_default_loop()) == UV_EBUSY) {
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
	}

	plua_pause_coverage(0);
	eventpool_gc();
	plua_gc();
	CuAssertIntEquals(tc, 0, xfree());
	testing_plua_reference_count = 0;
}

static void test_lua_reference_count_timer1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 1;

	test_lua_reference_count(tc, "timer1", 500);

	CuAssertIntEquals(tc, 4, run);
}

static void test_lua_reference_count_timer2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 2;

	test_lua_reference_count(tc, "timer2", 500);

	CuAssertIntEquals(tc, 7, run);
}

static void test_lua_reference_count_timer3(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 3;

	test_lua_reference_count(tc, "timer3", 500);

	CuAssertIntEquals(tc, 10, run);
}

static void test_lua_reference_count_timer4(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 4;

	test_lua_reference_count(tc, "timer4", 500);

	CuAssertIntEquals(tc, 11, run);
}

static void test_lua_reference_count_timer5(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 5;

	test_lua_reference_count(tc, "timer5", 1000);

	CuAssertIntEquals(tc, 6, run);
}

static void test_lua_reference_count_thread1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 10;

	test_lua_reference_count(tc, "thread1", 500);

	CuAssertIntEquals(tc, 4, run);
}

static void test_lua_reference_count_thread2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 11;

	test_lua_reference_count(tc, "thread2", 500);

	CuAssertIntEquals(tc, 6, run);
}

static void test_lua_reference_count_thread3(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 12;

	test_lua_reference_count(tc, "thread3", 500);

	CuAssertIntEquals(tc, 10, run);
}

static void test_lua_reference_count_thread4(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 13;

	test_lua_reference_count(tc, "thread4", 500);

	CuAssertIntEquals(tc, 10, run);
}

static void test_lua_reference_count_event1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 20;

	test_lua_reference_count(tc, "event1", 500);

	CuAssertIntEquals(tc, 4, run);
}

static void test_lua_reference_count_event2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 21;

	test_lua_reference_count(tc, "event2", 500);

	CuAssertIntEquals(tc, 7, run);
}

static void test_lua_reference_count_event3(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 22;

	test_lua_reference_count(tc, "event3", 500);

	CuAssertIntEquals(tc, 12, run);
}

static void test_lua_reference_count_event4(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 23;

	test_lua_reference_count(tc, "event4", 500);

	CuAssertIntEquals(tc, 12, run);
}

static void test_lua_reference_count_event5(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 24;

	test_lua_reference_count(tc, "event5", 500);

	CuAssertIntEquals(tc, 12, run);
}

static void test_lua_reference_count_event6(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 25;

	test_lua_reference_count(tc, "event6", 500);

	CuAssertIntEquals(tc, 5, run);
}

static void test_lua_reference_count_event7(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 26;

	test_lua_reference_count(tc, "event7", 500);

	CuAssertIntEquals(tc, 7, run);
}

static void test_lua_reference_count_event8(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 27;

	test_lua_reference_count(tc, "event8", 500);

	CuAssertIntEquals(tc, 7, run);
}

static void test_lua_reference_count_mqtt1(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 30;

	test_lua_reference_count(tc, "mqtt1", 500);

	CuAssertIntEquals(tc, 3, run);
}

static void test_lua_reference_count_mqtt2(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 31;

	mqtt_server(11883);

	test_lua_reference_count(tc, "mqtt2", 500);

	CuAssertIntEquals(tc, 5, run);
}

static void test_lua_reference_count_mqtt3(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 32;

	mqtt_server(11883);

	test_lua_reference_count(tc, "mqtt3", 500);

	CuAssertIntEquals(tc, 8, run);
}

static void test_lua_reference_count_mqtt4(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 33;

	mqtt_server(11883);

	test_lua_reference_count(tc, "mqtt4", 750);

	CuAssertIntEquals(tc, 9, run);
}

static void test_lua_reference_count_mqtt5(CuTest *tc) {
	printf("[ %-48s ]\n", __FUNCTION__);
	fflush(stdout);
	memtrack();

	test = 34;

	// mqtt_server(11883);

	test_lua_reference_count(tc, "mqtt5", 750);

	CuAssertIntEquals(tc, 5, run);
}

CuSuite *suite_lua_reference_count(void) {
	CuSuite *suite = CuSuiteNew();

	SUITE_ADD_TEST(suite, test_lua_reference_count_timer1);
	SUITE_ADD_TEST(suite, test_lua_reference_count_timer2);
	SUITE_ADD_TEST(suite, test_lua_reference_count_timer3);
	SUITE_ADD_TEST(suite, test_lua_reference_count_timer4);
	SUITE_ADD_TEST(suite, test_lua_reference_count_timer5);
	SUITE_ADD_TEST(suite, test_lua_reference_count_thread1);
	SUITE_ADD_TEST(suite, test_lua_reference_count_thread2);
	SUITE_ADD_TEST(suite, test_lua_reference_count_thread3);
	SUITE_ADD_TEST(suite, test_lua_reference_count_thread4);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event1);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event2);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event3);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event4);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event5);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event6);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event7);
	SUITE_ADD_TEST(suite, test_lua_reference_count_event8);
	SUITE_ADD_TEST(suite, test_lua_reference_count_mqtt1);
	SUITE_ADD_TEST(suite, test_lua_reference_count_mqtt2);
	SUITE_ADD_TEST(suite, test_lua_reference_count_mqtt3);
	SUITE_ADD_TEST(suite, test_lua_reference_count_mqtt4);
	SUITE_ADD_TEST(suite, test_lua_reference_count_mqtt5);

	return suite;
}
