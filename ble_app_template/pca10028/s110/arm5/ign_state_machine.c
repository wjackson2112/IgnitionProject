
#include "ign_state_machine.h"

#define CONNECTION_TIMEOUT_INTERVAL APP_TIMER_TICKS(60000, 0)
#define PASSCODE_ROTATE_INTERVAL APP_TIMER_TICKS(30000, 0)
#define STARTER_INTERVAL APP_TIMER_TICKS(80, 0)

#define ENDIAN_SWAP_32( x )  (\
              (( x & 0x000000FF ) << 24 ) \
            | (( x & 0x0000FF00 ) << 8  ) \
            | (( x & 0x00FF0000 ) >> 8  ) \
            | (( x & 0xFF000000 ) >> 24 ) \
            )

queued_event_t* head = NULL;
uint8_t queued_events = 0;
uint8_t current_state = ST_UNSEEDED;
uint64_t passcodes[3] = {0, 0, 0};
ble_boc_t m_boc;

static app_timer_id_t m_connection_timeout_timer_id;
static app_timer_id_t m_passcode_rotate_timer_id;
static app_timer_id_t m_starter_timer_id;

uint32_t passcode_rotate_timer_start_ticks;

void connection_timeout(void* p_context);
void passcode_timeout(void* p_context);
void starter_timeout(void* p_context);

uint32_t app_timer_ms(uint32_t ticks)
{
    float numerator = (1.0f) * 1000.0f;
    float denominator = (float)APP_TIMER_CLOCK_FREQ;
    float ms_per_tick = numerator / denominator;

    uint32_t ms = ms_per_tick * ticks;

    return ms;
}

void op_invalid(uint32_t toggle);
void op_lock(uint32_t toggle);
void op_ignition(uint32_t toggle);
void op_starter(uint32_t toggle);
void op_panic(uint32_t toggle);
void op_get_millis(uint32_t arg);
void op_sync_timer(uint32_t arg);
void op_sync_timer_adv(uint32_t arg);

void (*operations[NUM_OPERATIONS])(uint32_t arg) = { op_invalid, op_lock, op_ignition, op_starter, op_panic, op_get_millis, op_sync_timer, op_sync_timer_adv };

void state_machine_init(ble_boc_t boc){

    uint32_t err_code;
    err_code = app_timer_create(&m_connection_timeout_timer_id, APP_TIMER_MODE_SINGLE_SHOT, connection_timeout);
    APP_ERROR_CHECK(err_code);

    err_code = app_timer_create(&m_passcode_rotate_timer_id, APP_TIMER_MODE_REPEATED, passcode_timeout);
    APP_ERROR_CHECK(err_code);
	
	  err_code = app_timer_create(&m_starter_timer_id, APP_TIMER_MODE_SINGLE_SHOT, starter_timeout);
    APP_ERROR_CHECK(err_code);

    m_boc = boc;
}

void add_event(EVENT event, void* data, uint8_t size){

    LOG_INFO("Adding Event %s", evt_str[event]);

    queued_event_t* new_event = malloc(sizeof(queued_event_t));
    new_event->event = event;

    new_event->data = malloc(size);
    memcpy(new_event->data, data, size);
    
    new_event->next = NULL;
    
    queued_event_t* current = head;

    if(head == NULL){
        head = new_event;
    } else {
        while(current->next){
            current = current->next;
        }

        current->next = new_event;
    }

    queued_events++;

    LOG_DEBUG("%d Queued Events", queued_events);
}



void process_event(){

    LOG_DEBUG("Processing Next Event");
    LOG_DEBUG("Old state is %s", st_str[current_state]);

    queued_event_t* event_to_process = head;

    if(head->event >= NUM_EVENTS){
        LOG_ERROR("Undefined Event %d Received", head->event);
    } else {
        LOG_DEBUG("Processing %s", evt_str[head->event]);   

        static OPERATION selected_operation = OP_INVALID;

        //Run the event
        switch(head->event){
            case EVT_INVALID:
						{
                LOG_ERROR("Tried to process invalid event");
								int8_t response = -1;
								ble_boc_response_update(&m_boc, &response, 1);
                break;
						}
            case EVT_BUTTON_PRESS:
				switch(current_state){
					case ST_CONNECTED: case ST_UNLOCKED: case ST_UNSEEDED_CONNECTED: case ST_LOCKED:
					{
						uint32_t err_code = sd_ble_gap_disconnect(0, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
						APP_ERROR_CHECK(err_code);
					}
					default:
						app_timer_stop(m_passcode_rotate_timer_id);
						LOG_DEBUG("Passcode Rotation Timer stopped due to Seed Reset");
						current_state = ST_UNSEEDED;
						break;
				}
            case EVT_PASSCODE_SET:
                switch(current_state){
                    case ST_UNSEEDED_CONNECTED:
                    {
                        static uint64_t seed[4] = {0LL, 0LL, 0LL, 0LL};
                        static uint8_t number_of_seed_values = 0;
                        uint64_t seed_value = 0LL;
                        for(int i = 0; i < 8; i++){
                            uint64_t temp = ((uint8_t *) head->data)[i];
                            temp = temp << ((7 - i) * 8);
                            seed_value += temp;
                        }
                        
                        seed[number_of_seed_values] = seed_value;

                        LOG_DEBUG("Seed Value (MSB) - %08X", seed_value >> 32); 
                        LOG_DEBUG("Seed Value (LSB) - %08X", seed_value);

                        number_of_seed_values++;

                        LOG_DEBUG("Seed Values Received - %d", number_of_seed_values);

                        //Seed Completed
                        if(number_of_seed_values == 4){
                            
                            LOG_DEBUG("Final seed is ");
                            for(int i = 0; i < 4; i++){
                                LOG_DEBUG("%08X", seed[i] >> 32);
                                LOG_DEBUG("%08X", seed[i]);
                            }

                            init_by_array64(seed, 4);

                            //Reset Seed and Counter
                            for(int i = 0; i < 4; i++){
                                seed [i] = 0LL;
                            }
                            number_of_seed_values = 0;

                            //Generate and record passcode
                            passcodes[0] = genrand64_int64();
														passcodes[1] = genrand64_int64();
														passcodes[2] = genrand64_int64();
                            LOG_INFO("Passcode Prev (MSB) - %08X", passcodes[0] >> 32);
                            LOG_INFO("Passcode Prev (LSB) - %08X", passcodes[0]);
                            LOG_INFO("Passcode Curr (MSB) - %08X", passcodes[1] >> 32);
                            LOG_INFO("Passcode Curr (LSB) - %08X", passcodes[1]);
                            LOG_INFO("Passcode Next (MSB) - %08X", passcodes[2] >> 32);
                            LOG_INFO("Passcode Next (LSB) - %08X", passcodes[2]);

                            uint32_t err_code;
                            EVENT event = EVT_PASSCODE_TIMED_OUT;
                            err_code = app_timer_start(m_passcode_rotate_timer_id, PASSCODE_ROTATE_INTERVAL, &event);
                            APP_ERROR_CHECK(err_code);
														
														app_timer_cnt_get(&passcode_rotate_timer_start_ticks);

                            //Send successful response
														uint8_t response = 2;
                            ble_boc_response_update(&m_boc, &response, 1);
                            current_state = ST_CONNECTED;                       
                        } else {
														uint8_t response = 1;
														ble_boc_response_update(&m_boc, &response, 1);
												}
                        break;
                    }
                    case ST_CONNECTED: case ST_LOCKED:
                    {
                        uint64_t guess = 0LL;
                        for(int i = 0; i < 8; i++){
                            uint64_t temp = ((uint8_t *) head->data)[i];
                            temp = temp << ((7 - i) * 8);
                            guess += temp;
                        }

                        if(guess == passcodes[0]){
                            app_timer_stop(m_connection_timeout_timer_id);
                            LOG_DEBUG("Connection Timeout Timer stopped due to Correct Passcode");
                            current_state = ST_UNLOCKED;
														uint8_t response = 6;
                            ble_boc_response_update(&m_boc, &response, 1);
												} else if (guess == passcodes[1]){
														app_timer_stop(m_connection_timeout_timer_id);
                            LOG_DEBUG("Connection Timeout Timer stopped due to Correct Passcode");
                            current_state = ST_UNLOCKED;
														uint8_t response = 3;
                            ble_boc_response_update(&m_boc, &response, 1);
												} else if (guess == passcodes[2]){
														app_timer_stop(m_connection_timeout_timer_id);
                            LOG_DEBUG("Connection Timeout Timer stopped due to Correct Passcode");
                            current_state = ST_UNLOCKED;
														uint8_t response = 7;
                            ble_boc_response_update(&m_boc, &response, 1);													
												} else {
                            static uint8_t incorrect_attempts = 0;
                            incorrect_attempts++;
                            LOG_DEBUG("Incorrect passcode attempt");
                            if(incorrect_attempts >= 5){
                                uint32_t err_code = sd_ble_gap_disconnect(0, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                                APP_ERROR_CHECK(err_code);
                                incorrect_attempts = 0;
                                LOG_DEBUG("Disconnecting from too many incorrect passcode attempts");
																int8_t response = -2;
                                ble_boc_response_update(&m_boc, &response, 1);
                            }
														int8_t response = -3;
                            ble_boc_response_update(&m_boc, &response, 1);
                        }
                        break;
                    }
                    default:
										{		
												int8_t response = -1;
                        ble_boc_response_update(&m_boc, &response, 1);
                        LOG_WARN("Logged unsupported %s event from %s", evt_str[head->event], st_str[current_state]);
                        break;
										}
                }
                break;
            case EVT_CONNECTED:
                switch(current_state){
                    case ST_IDLE:
                    {
                        uint32_t err_code;
                        EVENT event = EVT_TIMED_OUT;
                        err_code = app_timer_start(m_connection_timeout_timer_id, CONNECTION_TIMEOUT_INTERVAL, &event);
                        APP_ERROR_CHECK(err_code);
                        current_state = ST_CONNECTED;
                        selected_operation = OP_INVALID;
                        break;
                    }
                    case ST_UNSEEDED:
                    {
                        uint32_t err_code;
                        EVENT event = EVT_TIMED_OUT;
                        err_code = app_timer_start(m_connection_timeout_timer_id, CONNECTION_TIMEOUT_INTERVAL, &event);
                        APP_ERROR_CHECK(err_code);
                        current_state = ST_UNSEEDED_CONNECTED;
                        selected_operation = OP_INVALID;
                        break;
                    }
                    default:
                        LOG_WARN("Logged unsupported %s event from %s", evt_str[head->event], st_str[current_state]);
                        break;
                }
                break;
            case EVT_DISCONNECTED:
                switch(current_state){
                    case ST_CONNECTED: 
                        app_timer_stop(m_connection_timeout_timer_id);
                        LOG_DEBUG("Connection Timeout Timer stopped due to Manual Disconnect");
                    case ST_UNLOCKED: case ST_LOCKED:
                        current_state = ST_IDLE;
                        break;
                    case ST_UNSEEDED_CONNECTED:
                        app_timer_stop(m_connection_timeout_timer_id);
                        LOG_DEBUG("Connection Timeout Timer stopped due to Manual Disconnect");
                        current_state = ST_UNSEEDED;
                        break;
                    default:
                        LOG_WARN("Logged unsupported %s event from %s", evt_str[head->event], st_str[current_state]);
                        break;
                }
                break;
            case EVT_TIMED_OUT:
                switch(current_state){
                    case ST_CONNECTED: case ST_UNSEEDED_CONNECTED:
                    {
                        uint32_t err_code = sd_ble_gap_disconnect(0, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
                        APP_ERROR_CHECK(err_code);
                        break;
                    }
                    default:
                        LOG_WARN("Logged unsupported %s event from %s", evt_str[head->event], st_str[current_state]);
                        break;
                }
                break;
            case EVT_PASSCODE_TIMED_OUT:
                switch(current_state){
                    case ST_UNLOCKED:
                        current_state = ST_LOCKED;
                        selected_operation = OP_INVALID;
                    case ST_IDLE: case ST_LOCKED: case ST_CONNECTED:
												passcodes[0] = passcodes[1];
										    passcodes[1] = passcodes[2];
                        passcodes[2] = genrand64_int64();
												LOG_INFO("Passcode Prev (MSB) - %08X", passcodes[0] >> 32);
												LOG_INFO("Passcode Prev (LSB) - %08X", passcodes[0]);
												LOG_INFO("Passcode Curr (MSB) - %08X", passcodes[1] >> 32);
												LOG_INFO("Passcode Curr (LSB) - %08X", passcodes[1]);
												LOG_INFO("Passcode Next (MSB) - %08X", passcodes[2] >> 32);
												LOG_INFO("Passcode Next (LSB) - %08X", passcodes[2]);
												app_timer_cnt_get(&passcode_rotate_timer_start_ticks);
                        break;
                    default:
                        LOG_WARN("Logged unsupported %s event from %s", evt_str[head->event], st_str[current_state]);
                        break;
                }
                break;
            case EVT_OPERATION_SET:
                switch(current_state){
                    case ST_UNLOCKED:
										{
                        if(*((OPERATION *) head->data) >= NUM_OPERATIONS){
                            selected_operation = OP_INVALID;
														int8_t response = -4;
                            ble_boc_response_update(&m_boc, &response, 1);
                            break;
                        }
                        selected_operation = *((OPERATION *) head->data);
												LOG_DEBUG("Selected operation %s", op_str[selected_operation]);
												if(selected_operation == OP_GET_MILLIS ||
													 selected_operation == OP_SYNC_TIMER ||
												   selected_operation == OP_SYNC_TIMER_ADV){
														LOG_DEBUG("Running operation %s", op_str[selected_operation]);
														(*operations[selected_operation])(0);
												} else {                       
														int8_t response = 4;
														ble_boc_response_update(&m_boc, &response, 1);
												}
                        break;
										}
										case ST_LOCKED: case ST_CONNECTED: case ST_UNSEEDED_CONNECTED:
										{
												int8_t response = -5;
												ble_boc_response_update(&m_boc, &response, 1);					
												break;
										}
										default:
										{
												int8_t response = -1;
												ble_boc_response_update(&m_boc, &response, 1);
												break;
										}
								}
                break;
            case EVT_OPERAND_SET:
                switch(current_state){
                    case ST_UNLOCKED:
										{
                        if(selected_operation == OP_INVALID){
														int8_t response = -4;
                            ble_boc_response_update(&m_boc, &response, 1);
                            break;     
                        }
                        (*operations[selected_operation])(*((uint8_t *) head->data));
												int8_t response = 5;
                        ble_boc_response_update(&m_boc, &response, 1);
                        break;
										}
										case ST_LOCKED: case ST_CONNECTED: case ST_UNSEEDED_CONNECTED:
										{
												int8_t response = -6;
												ble_boc_response_update(&m_boc, &response, 1);
												break;
										}
										default:
										{
												int8_t response = -1;
												ble_boc_response_update(&m_boc, &response, 1);
												break;
										}											
								}
								break;
						default:
                LOG_ERROR("Logged unsupported event %s", evt_str[head->event]);
                break;
        }
    }

    head = head->next;

    LOG_INFO("Current State - %s", st_str[current_state]);

    free(event_to_process->data);
    free(event_to_process);
    queued_events--;
}

bool events_queued(){
    if(queued_events){
        return true;
    }
    return false;
}

void connection_timeout(void * p_context){
    add_event(EVT_TIMED_OUT, NULL, 0);
}

void passcode_timeout(void * p_context){
    add_event(EVT_PASSCODE_TIMED_OUT, NULL, 0);
}

void starter_timeout(void * p_context){
		op_starter(0);
}

void op_invalid(uint32_t toggle){
    LOG_INFO("Setting Operation Invalid to %d", toggle);
		LOG_ERROR("Attempt to run Invalid Operation");
}

void op_lock(uint32_t toggle){
    LOG_INFO("Setting Operation Lock to %d", toggle);
		if(toggle){
			nrf_gpio_pin_set(LED_1);
		} else {
			nrf_gpio_pin_clear(LED_1);
		}
}

void op_ignition(uint32_t toggle){
    LOG_INFO("Setting Operation Ignition to %d", toggle);
		if(toggle){
			nrf_gpio_pin_set(LED_2);
		} else {
			nrf_gpio_pin_clear(LED_2);
		}
}

void op_starter(uint32_t toggle){
    LOG_INFO("Setting Operation Starter to %d", toggle);
		if(toggle){
			app_timer_stop(m_starter_timer_id);
			app_timer_start(m_starter_timer_id, STARTER_INTERVAL, 0);
			
			nrf_gpio_pin_set(LED_3);
		} else {
			nrf_gpio_pin_clear(LED_3);
		}
}

void op_panic(uint32_t toggle){
    LOG_INFO("Setting Operation Panic to %d", toggle);
		if(toggle){
			nrf_gpio_pin_set(LED_4);
			nrf_gpio_pin_set(11);
		} else {
			nrf_gpio_pin_clear(LED_4);
			nrf_gpio_pin_clear(11);
		}
}

void op_get_millis(uint32_t arg){
	
		UNUSED_PARAMETER(arg);
	
		uint32_t millis;
	
		app_timer_cnt_get(&millis);
		app_timer_cnt_diff_compute(millis,
                               passcode_rotate_timer_start_ticks,
                               &millis);
	
		millis = ENDIAN_SWAP_32(app_timer_ms(millis));
	
		ble_boc_response_update(&m_boc, &millis, 4);
}

void op_sync_timer(uint32_t arg){
		
		app_timer_stop(m_passcode_rotate_timer_id);
		app_timer_start(m_passcode_rotate_timer_id, PASSCODE_ROTATE_INTERVAL, 0);
		app_timer_cnt_get(&passcode_rotate_timer_start_ticks);

}

void op_sync_timer_adv(uint32_t arg){
		app_timer_stop(m_passcode_rotate_timer_id);
		app_timer_start(m_passcode_rotate_timer_id, PASSCODE_ROTATE_INTERVAL, 0);
		passcodes[0] = passcodes[1];
		passcodes[1] = passcodes[2];
		passcodes[2] = genrand64_int64();
		LOG_INFO("Passcode Prev (MSB) - %08X", passcodes[0] >> 32);
		LOG_INFO("Passcode Prev (LSB) - %08X", passcodes[0]);
		LOG_INFO("Passcode Curr (MSB) - %08X", passcodes[1] >> 32);
		LOG_INFO("Passcode Curr (LSB) - %08X", passcodes[1]);
		LOG_INFO("Passcode Next (MSB) - %08X", passcodes[2] >> 32);
		LOG_INFO("Passcode Next (LSB) - %08X", passcodes[2]);
		app_timer_cnt_get(&passcode_rotate_timer_start_ticks);
		add_event(EVT_PASSCODE_TIMED_OUT, NULL, 0);
}
