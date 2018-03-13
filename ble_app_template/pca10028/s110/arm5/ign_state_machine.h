/*
 *  Ignition Controller State Machine
 */

#ifndef IGN_STATE_MACHINE_H__
#define IGN_STATE_MACHINE_H__

#define MAX_EVENTS 10

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include "logger.h"
#include "ble_boc.h"
#include "mt19937-64.h"
//#include "tinymt64.h"
#include "ble_hci.h"
#include "app_timer.h"
#include "boards.h"
#include "nordic_common.h"

// Forward declaration of the ble_boc_t type. 
typedef struct ble_boc_s ble_boc_t;

typedef enum {  ST_INVALID,
                ST_UNSEEDED,
                ST_UNSEEDED_CONNECTED,
                ST_IDLE, 
                ST_CONNECTED, 
                ST_LOCKED, 
                ST_UNLOCKED, 
                NUM_STATES
} STATE;

static char* st_str[] = {
                "ST_INVALID",
                "ST_UNSEEDED", 
                "ST_UNSEEDED_CONNECTED",
                "ST_IDLE", 
                "ST_CONNECTED", 
                "ST_LOCKED", 
                "ST_UNLOCKED"
};

typedef enum {  EVT_INVALID,
                EVT_BUTTON_PRESS,
                EVT_PASSCODE_SET,
                EVT_CONNECTED,
                EVT_DISCONNECTED,
                EVT_TIMED_OUT,
                EVT_PASSCODE_TIMED_OUT,
                EVT_OPERATION_SET,
                EVT_OPERAND_SET,
                NUM_EVENTS
} EVENT;

static char* evt_str[] = {
                "EVT_INVALID",
                "EVT_BUTTON_PRESS",
                "EVT_PASSCODE_SET",
                "EVT_CONNECTED",
                "EVT_DISCONNECTED",
                "EVT_TIMED_OUT",
                "EVT_PASSCODE_TIMED_OUT",
                "EVT_OPERATION_SET",
                "EVT_OPERAND_SET"
};

typedef enum {  OP_INVALID,
                OP_LOCK,
                OP_IGNITION,
                OP_STARTER,
                OP_PANIC,
								OP_GET_MILLIS,
								OP_SYNC_TIMER,
								OP_SYNC_TIMER_ADV,
                NUM_OPERATIONS
} OPERATION;

static char* op_str[] = {
                "OP_INVALID",
                "OP_LOCK",
                "OP_IGNITION",
                "OP_STARTER",
                "OP_PANIC",
								"OP_GET_MILLIS",
								"OP_SYNC_TIMER",
								"OP_SYNC_TIMER_ADV"
};

typedef struct queued_event_s {
        EVENT event;
        void* data;
        uint8_t size;
        struct queued_event_s* next;
} queued_event_t;                        

void state_machine_init(ble_boc_t boc);
void add_event(EVENT event, void* data, uint8_t size);
void process_event(void);
bool events_queued(void);
#endif
