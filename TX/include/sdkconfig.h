// Wraps the Arduino core's prebuilt sdkconfig.h to override the NimBLE host
// settings (#41).
//
// Arduino-ESP32 3.x ships NimBLE enabled in its sdkconfig, with every role on,
// 3 connections, 3 bonds and INFO logging. NimBLE-Arduino includes sdkconfig.h,
// which redefines anything set with -D in platformio.ini, so the core's values
// silently win. The NimBLE host is compiled from source in NimBLE-Arduino, so
// overriding them here takes effect; the precompiled BLE controller allows up
// to CONFIG_BT_CTRL_BLE_MAX_ACT (6) links.
//
// Arduino-ESP32 2.x has no NimBLE settings in its sdkconfig, so there this only
// restates what the -D flags used to set.
//
// Only NimBLE host values belong here. Changing anything that the core's
// precompiled libraries were built with would mismatch them
#pragma once
#include_next <sdkconfig.h>

#ifdef TX_BLE
// the TX is only ever a peripheral
#undef CONFIG_BT_NIMBLE_ROLE_CENTRAL
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL 0
#undef CONFIG_BT_NIMBLE_ROLE_OBSERVER
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER 0
// and the legacy aliases, or NimBLE-Arduino's nimconfig_rename.h turns the
// roles back into empty defines
#undef CONFIG_NIMBLE_ROLE_CENTRAL
#undef CONFIG_NIMBLE_ROLE_OBSERVER

// two phones and a watch, plus one spare
#undef CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS 4
// operator phones and watches; the oldest bond is dropped when full
#undef CONFIG_BT_NIMBLE_MAX_BONDS
#define CONFIG_BT_NIMBLE_MAX_BONDS 8
#undef CONFIG_BT_NIMBLE_MAX_CCCDS
#define CONFIG_BT_NIMBLE_MAX_CCCDS 16

// no host logging, as on Arduino-ESP32 2.x (5 = none)
#undef CONFIG_BT_NIMBLE_LOG_LEVEL
#define CONFIG_BT_NIMBLE_LOG_LEVEL 5
#endif
