/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

/* Attention! 
*  To maintain compliance with Nordic Semiconductor ASA’s Bluetooth profile 
*  qualification listings, this section of source code must not be modified.
*/

#include "ble_boc.h"
#include <string.h>
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "app_util.h"

/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_boc       Battery Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_boc_t * p_boc, ble_evt_t * p_ble_evt)
{
    p_boc->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_boc       Battery Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_boc_t * p_boc, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_boc->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_boc       Battery Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_boc_t * p_boc, ble_evt_t * p_ble_evt)
{
    if (p_ble_evt->evt.gatts_evt.params.write.handle == p_boc->passcode_handles.value_handle)
    {
        LOG_DEBUG("Passcode Written");

        add_event(EVT_PASSCODE_SET, 
                  p_ble_evt->evt.gatts_evt.params.write.data, 
                  p_ble_evt->evt.gatts_evt.params.write.len);
    }

    if (p_ble_evt->evt.gatts_evt.params.write.handle == p_boc->opcode_handles.value_handle)
    {
        LOG_DEBUG("Operation Written");

        add_event(EVT_OPERATION_SET,
                  p_ble_evt->evt.gatts_evt.params.write.data,
                  p_ble_evt->evt.gatts_evt.params.write.len);
    }

    if (p_ble_evt->evt.gatts_evt.params.write.handle == p_boc->operand_handles.value_handle)
    {
        LOG_DEBUG("Operand Written");

        add_event(EVT_OPERAND_SET,
                  p_ble_evt->evt.gatts_evt.params.write.data,
                  p_ble_evt->evt.gatts_evt.params.write.len);
    }

    if (p_boc->is_notification_supported)
    {
        ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

        if (
            (p_evt_write->handle == p_boc->passcode_handles.cccd_handle)
            &&
            (p_evt_write->len == 2)
           )
        {
            // CCCD written, call application event handler
            if (p_boc->evt_handler != NULL)
            {
                ble_boc_evt_t evt;

                if (ble_srv_is_notification_enabled(p_evt_write->data))
                { 
                    evt.evt_type = BLE_BOC_EVT_NOTIFICATION_ENABLED;
                }
                else
                {
                    evt.evt_type = BLE_BOC_EVT_NOTIFICATION_DISABLED;
                }

                p_boc->evt_handler(p_boc, &evt);
            }
        }
    }
}


void ble_boc_on_ble_evt(ble_boc_t * p_boc, ble_evt_t * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_boc, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_boc, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            LOG_DEBUG("Received BOC BLE Write Event");
            on_write(p_boc, p_ble_evt);
            break;

        default:
            // No implementation needed.
            LOG_DEBUG("Unsupported BOC BLE Event %d", p_ble_evt->header.evt_id);
            break;
    }
}


/**@brief Function for adding the Battery Level characteristic.
 *
 * @param[in]   p_boc        Battery Service structure.
 * @param[in]   p_boc_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t passcode_char_add(ble_boc_t * p_boc, const ble_boc_init_t * p_boc_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_passcode[8];
    uint8_t             encoded_report_ref[BLE_SRV_ENCODED_REPORT_REF_LEN];
    uint8_t             init_len;
      int                 i;

    // Add Battery Level characteristic
    if (p_boc->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));

        // According to boc_SPEC_V10, the read operation on cccd should be possible without
        // authentication.
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_boc_init->passcode_char_attr_md.cccd_write_perm;
        cccd_md.vloc       = BLE_GATTS_VLOC_STACK;
    }

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 0;
    char_md.char_props.write  = 1;
    char_md.char_props.notify = 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = (p_boc->is_notification_supported) ? &cccd_md : NULL;
    char_md.p_sccd_md         = NULL;

    //BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_passcode_CHAR);

        ble_uuid.type = BLE_UUID_TYPE_BLE;
        ble_uuid.uuid = 0x82ad;
        
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_boc_init->passcode_char_attr_md.read_perm;
    attr_md.write_perm = p_boc_init->passcode_char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;

    for(i = 0; i < 8; i++){
        initial_passcode[i] = p_boc_init->initial_passcode[i];
    }


    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 8;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = 8;
    attr_char_value.p_value   = initial_passcode;

    err_code = sd_ble_gatts_characteristic_add(p_boc->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_boc->passcode_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_boc_init->p_report_ref != NULL)
    {
        // Add Report Reference descriptor
        BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_REPORT_REF_DESCR);

        memset(&attr_md, 0, sizeof(attr_md));

        attr_md.read_perm = p_boc_init->passcode_report_read_perm;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

        attr_md.vloc    = BLE_GATTS_VLOC_STACK;
        attr_md.rd_auth = 0;
        attr_md.wr_auth = 0;
        attr_md.vlen    = 0;
        
        init_len = ble_srv_report_ref_encode(encoded_report_ref, p_boc_init->p_report_ref);
        
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        attr_char_value.p_uuid    = &ble_uuid;
        attr_char_value.p_attr_md = &attr_md;
        attr_char_value.init_len  = init_len;
        attr_char_value.init_offs = 0;
        attr_char_value.max_len   = attr_char_value.init_len;
        attr_char_value.p_value   = encoded_report_ref;

        err_code = sd_ble_gatts_descriptor_add(p_boc->passcode_handles.value_handle,
                                               &attr_char_value,
                                               &p_boc->report_ref_handle);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        p_boc->report_ref_handle = BLE_GATT_HANDLE_INVALID;
    }

    return NRF_SUCCESS;
}

/**@brief Function for adding the Battery Level characteristic.
 *
 * @param[in]   p_boc        Battery Service structure.
 * @param[in]   p_boc_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t opcode_char_add(ble_boc_t * p_boc, const ble_boc_init_t * p_boc_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t       ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_opcode;
    uint8_t             encoded_report_ref[BLE_SRV_ENCODED_REPORT_REF_LEN];
    uint8_t             init_len;

    // Add Battery Level characteristic
    if (p_boc->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));

        // According to boc_SPEC_V10, the read operation on cccd should be possible without
        // authentication.
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_boc_init->opcode_char_attr_md.cccd_write_perm;
        cccd_md.vloc       = BLE_GATTS_VLOC_STACK;
    }

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 0;
	char_md.char_props.write  = 1;
    char_md.char_props.notify = 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = (p_boc->is_notification_supported) ? &cccd_md : NULL;
    char_md.p_sccd_md         = NULL;

    //BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_passcode_CHAR);

		ble_uuid.type = BLE_UUID_TYPE_BLE;
		ble_uuid.uuid = 0x82ae;
		
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_boc_init->opcode_char_attr_md.read_perm;
    attr_md.write_perm = p_boc_init->opcode_char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 0;

    initial_opcode = p_boc_init->initial_opcode;

    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint8_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = sizeof(uint8_t);
    attr_char_value.p_value   = &initial_opcode;

    err_code = sd_ble_gatts_characteristic_add(p_boc->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_boc->opcode_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_boc_init->p_report_ref != NULL)
    {
        // Add Report Reference descriptor
        BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_REPORT_REF_DESCR);

        memset(&attr_md, 0, sizeof(attr_md));

        attr_md.read_perm = p_boc_init->opcode_report_read_perm;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

        attr_md.vloc    = BLE_GATTS_VLOC_STACK;
        attr_md.rd_auth = 0;
        attr_md.wr_auth = 0;
        attr_md.vlen    = 0;
        
        init_len = ble_srv_report_ref_encode(encoded_report_ref, p_boc_init->p_report_ref);
        
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        attr_char_value.p_uuid    = &ble_uuid;
        attr_char_value.p_attr_md = &attr_md;
        attr_char_value.init_len  = init_len;
        attr_char_value.init_offs = 0;
        attr_char_value.max_len   = attr_char_value.init_len;
        attr_char_value.p_value   = encoded_report_ref;

        err_code = sd_ble_gatts_descriptor_add(p_boc->opcode_handles.value_handle,
                                               &attr_char_value,
                                               &p_boc->report_ref_handle);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        p_boc->report_ref_handle = BLE_GATT_HANDLE_INVALID;
    }

    return NRF_SUCCESS;
}

/**@brief Function for adding the Battery Level characteristic.
 *
 * @param[in]   p_boc        Battery Service structure.
 * @param[in]   p_boc_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t operand_char_add(ble_boc_t * p_boc, const ble_boc_init_t * p_boc_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t       ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_operand[20];
    uint8_t             encoded_report_ref[BLE_SRV_ENCODED_REPORT_REF_LEN];
    uint8_t             init_len;
	  int                 i;

    // Add Battery Level characteristic
    if (p_boc->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));

        // According to boc_SPEC_V10, the read operation on cccd should be possible without
        // authentication.
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_boc_init->operand_char_attr_md.cccd_write_perm;
        cccd_md.vloc       = BLE_GATTS_VLOC_STACK;
    }

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 0;
	char_md.char_props.write  = 1;
    char_md.char_props.notify = 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = (p_boc->is_notification_supported) ? &cccd_md : NULL;
    char_md.p_sccd_md         = NULL;

    //BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_passcode_CHAR);

		ble_uuid.type = BLE_UUID_TYPE_BLE;
		ble_uuid.uuid = 0x82af;
		
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_boc_init->operand_char_attr_md.read_perm;
    attr_md.write_perm = p_boc_init->operand_char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;

	for(i = 0; i < 20; i++){
		initial_operand[i] = p_boc_init->initial_operand[i];
	}


    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = 20;
    attr_char_value.p_value   = initial_operand;

    err_code = sd_ble_gatts_characteristic_add(p_boc->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_boc->operand_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_boc_init->p_report_ref != NULL)
    {
        // Add Report Reference descriptor
        BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_REPORT_REF_DESCR);

        memset(&attr_md, 0, sizeof(attr_md));

        attr_md.read_perm = p_boc_init->operand_report_read_perm;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

        attr_md.vloc    = BLE_GATTS_VLOC_STACK;
        attr_md.rd_auth = 0;
        attr_md.wr_auth = 0;
        attr_md.vlen    = 0;
        
        init_len = ble_srv_report_ref_encode(encoded_report_ref, p_boc_init->p_report_ref);
        
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        attr_char_value.p_uuid    = &ble_uuid;
        attr_char_value.p_attr_md = &attr_md;
        attr_char_value.init_len  = init_len;
        attr_char_value.init_offs = 0;
        attr_char_value.max_len   = attr_char_value.init_len;
        attr_char_value.p_value   = encoded_report_ref;

        err_code = sd_ble_gatts_descriptor_add(p_boc->operand_handles.value_handle,
                                               &attr_char_value,
                                               &p_boc->report_ref_handle);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        p_boc->report_ref_handle = BLE_GATT_HANDLE_INVALID;
    }

    return NRF_SUCCESS;
}

/**@brief Function for adding the Battery Level characteristic.
 *
 * @param[in]   p_boc        Battery Service structure.
 * @param[in]   p_boc_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t response_char_add(ble_boc_t * p_boc, const ble_boc_init_t * p_boc_init)
{
    uint32_t            err_code;
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t       ble_uuid;
    ble_gatts_attr_md_t attr_md;
    uint8_t             initial_response[20];
    uint8_t             encoded_report_ref[BLE_SRV_ENCODED_REPORT_REF_LEN];
    uint8_t             init_len;
		int 								i;
	
    // Add Battery Level characteristic
    if (p_boc->is_notification_supported)
    {
        memset(&cccd_md, 0, sizeof(cccd_md));

        // According to boc_SPEC_V10, the read operation on cccd should be possible without
        // authentication.
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        cccd_md.write_perm = p_boc_init->response_char_attr_md.cccd_write_perm;
        cccd_md.vloc       = BLE_GATTS_VLOC_STACK;
    }

    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read   = 1;
    char_md.char_props.notify = (p_boc->is_notification_supported) ? 1 : 0;
    char_md.p_char_user_desc  = NULL;
    char_md.p_char_pf         = NULL;
    char_md.p_user_desc_md    = NULL;
    char_md.p_cccd_md         = (p_boc->is_notification_supported) ? &cccd_md : NULL;
    char_md.p_sccd_md         = NULL;

    //BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_passcode_CHAR);

		ble_uuid.type = BLE_UUID_TYPE_BLE;
		ble_uuid.uuid = 0x82b0;
		
    memset(&attr_md, 0, sizeof(attr_md));

    attr_md.read_perm  = p_boc_init->response_char_attr_md.read_perm;
    attr_md.write_perm = p_boc_init->response_char_attr_md.write_perm;
    attr_md.vloc       = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth    = 0;
    attr_md.wr_auth    = 0;
    attr_md.vlen       = 1;

	  for(i = 0; i < 20; i++){
			initial_response[i] = p_boc_init->initial_response[i];
		}
			
    memset(&attr_char_value, 0, sizeof(attr_char_value));

    attr_char_value.p_uuid    = &ble_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = 1;
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = 20;
    attr_char_value.p_value   = initial_response;

    err_code = sd_ble_gatts_characteristic_add(p_boc->service_handle, &char_md,
                                               &attr_char_value,
                                               &p_boc->response_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    if (p_boc_init->p_report_ref != NULL)
    {
        // Add Report Reference descriptor
        BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_REPORT_REF_DESCR);

        memset(&attr_md, 0, sizeof(attr_md));

        attr_md.read_perm = p_boc_init->response_report_read_perm;
        BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);

        attr_md.vloc    = BLE_GATTS_VLOC_STACK;
        attr_md.rd_auth = 0;
        attr_md.wr_auth = 0;
        attr_md.vlen    = 0;
        
        init_len = ble_srv_report_ref_encode(encoded_report_ref, p_boc_init->p_report_ref);
        
        memset(&attr_char_value, 0, sizeof(attr_char_value));

        attr_char_value.p_uuid    = &ble_uuid;
        attr_char_value.p_attr_md = &attr_md;
        attr_char_value.init_len  = init_len;
        attr_char_value.init_offs = 0;
        attr_char_value.max_len   = attr_char_value.init_len;
        attr_char_value.p_value   = encoded_report_ref;

        err_code = sd_ble_gatts_descriptor_add(p_boc->response_handles.value_handle,
                                               &attr_char_value,
                                               &p_boc->report_ref_handle);
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }
    else
    {
        p_boc->report_ref_handle = BLE_GATT_HANDLE_INVALID;
    }

    return NRF_SUCCESS;
}

uint32_t ble_boc_init(ble_boc_t * p_boc, const ble_boc_init_t * p_boc_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;

		ble_uuid128_t ble_uuid128 = { 0x72, 0xfd, 0x22, 0x7d, 0x53, 0x2d, 0x3d, 0xb6, 0xe3, 0x42, 0x5e, 0x09, 0xac, 0x82, 0xc9, 0x2f };
		uint8_t ble_custom_type;
	
		err_code = sd_ble_uuid_vs_add(&ble_uuid128, &ble_custom_type);      
		if (err_code != NRF_SUCCESS)
		{
				return err_code;
		}
	
    // Initialize service structure
    p_boc->evt_handler               = p_boc_init->evt_handler;
    p_boc->conn_handle               = BLE_CONN_HANDLE_INVALID;
    p_boc->is_notification_supported = p_boc_init->support_notification;

    // Add service
    //BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_BATTERY_SERVICE);

		ble_uuid.type = BLE_UUID_TYPE_VENDOR_BEGIN;
		ble_uuid.uuid = 0x82ac;
	
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_boc->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add battery level characteristic
		passcode_char_add(p_boc, p_boc_init);
		opcode_char_add(p_boc, p_boc_init);
		operand_char_add(p_boc, p_boc_init);
		response_char_add(p_boc, p_boc_init);
    return 0;
}


uint32_t ble_boc_passcode_update(ble_boc_t * p_boc, uint8_t passcode)
{
    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

        // Initialize value struct.
        memset(&gatts_value, 0, sizeof(gatts_value));

        gatts_value.len     = sizeof(uint8_t);
        gatts_value.offset  = 0;
        gatts_value.p_value = &passcode;

        // Update databoce.
        err_code = sd_ble_gatts_value_set(p_boc->conn_handle,
                                                                            p_boc->passcode_handles.value_handle,
                                                                            &gatts_value);
        if (err_code != NRF_SUCCESS)
        {
                return err_code;
        }

        // Send value if connected and notifying.
        if ((p_boc->conn_handle != BLE_CONN_HANDLE_INVALID) && p_boc->is_notification_supported)
        {
                ble_gatts_hvx_params_t hvx_params;

                memset(&hvx_params, 0, sizeof(hvx_params));

                hvx_params.handle = p_boc->passcode_handles.value_handle;
                hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
                hvx_params.offset = gatts_value.offset;
                hvx_params.p_len  = &gatts_value.len;
                hvx_params.p_data = gatts_value.p_value;

                err_code = sd_ble_gatts_hvx(p_boc->conn_handle, &hvx_params);
        }
        else
        {
                err_code = NRF_ERROR_INVALID_STATE;
        }

    return err_code;
}

uint32_t ble_boc_opcode_update(ble_boc_t * p_boc, uint8_t opcode)
{
    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

		// Initialize value struct.
		memset(&gatts_value, 0, sizeof(gatts_value));

		gatts_value.len     = sizeof(uint8_t);
		gatts_value.offset  = 0;
		gatts_value.p_value = &opcode;

		// Update databoce.
		err_code = sd_ble_gatts_value_set(p_boc->conn_handle,
																			p_boc->opcode_handles.value_handle,
																			&gatts_value);
		if (err_code != NRF_SUCCESS)
		{
				return err_code;
		}

		// Send value if connected and notifying.
		if ((p_boc->conn_handle != BLE_CONN_HANDLE_INVALID) && p_boc->is_notification_supported)
		{
				ble_gatts_hvx_params_t hvx_params;

				memset(&hvx_params, 0, sizeof(hvx_params));

				hvx_params.handle = p_boc->opcode_handles.value_handle;
				hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
				hvx_params.offset = gatts_value.offset;
				hvx_params.p_len  = &gatts_value.len;
				hvx_params.p_data = gatts_value.p_value;

				err_code = sd_ble_gatts_hvx(p_boc->conn_handle, &hvx_params);
		}
		else
		{
				err_code = NRF_ERROR_INVALID_STATE;
		}

    return err_code;
}

uint32_t ble_boc_operand_update(ble_boc_t * p_boc, uint8_t operand)
{
    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

		// Initialize value struct.
		memset(&gatts_value, 0, sizeof(gatts_value));

		gatts_value.len     = sizeof(uint8_t);
		gatts_value.offset  = 0;
		gatts_value.p_value = &operand;

		// Update databoce.
		err_code = sd_ble_gatts_value_set(p_boc->conn_handle,
																			p_boc->operand_handles.value_handle,
																			&gatts_value);
		if (err_code != NRF_SUCCESS)
		{
				return err_code;
		}

		// Send value if connected and notifying.
		if ((p_boc->conn_handle != BLE_CONN_HANDLE_INVALID) && p_boc->is_notification_supported)
		{
				ble_gatts_hvx_params_t hvx_params;

				memset(&hvx_params, 0, sizeof(hvx_params));

				hvx_params.handle = p_boc->operand_handles.value_handle;
				hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
				hvx_params.offset = gatts_value.offset;
				hvx_params.p_len  = &gatts_value.len;
				hvx_params.p_data = gatts_value.p_value;

				err_code = sd_ble_gatts_hvx(p_boc->conn_handle, &hvx_params);
		}
		else
		{
				err_code = NRF_ERROR_INVALID_STATE;
		}

    return err_code;
}

uint32_t ble_boc_response_update(ble_boc_t * p_boc, void * response, uint8_t len)
{
    uint32_t err_code = NRF_SUCCESS;
    ble_gatts_value_t gatts_value;

    LOG_INFO("Sending Response %02X", response);

	// Initialize value struct.
	memset(&gatts_value, 0, sizeof(gatts_value));

	gatts_value.len     = len;
	gatts_value.offset  = 0;
	gatts_value.p_value = response;

	// Update databoce.
	err_code = sd_ble_gatts_value_set(p_boc->conn_handle,
									  p_boc->response_handles.value_handle,
									  &gatts_value);
	if (err_code != NRF_SUCCESS)
	{
			return err_code;
	}

	// Send value if connected and notifying.
	if ((p_boc->conn_handle != BLE_CONN_HANDLE_INVALID) && p_boc->is_notification_supported)
	{
			ble_gatts_hvx_params_t hvx_params;

			memset(&hvx_params, 0, sizeof(hvx_params));

			hvx_params.handle = p_boc->response_handles.value_handle;
			hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
			hvx_params.offset = gatts_value.offset;
			hvx_params.p_len  = &gatts_value.len;
			hvx_params.p_data = gatts_value.p_value;

			err_code = sd_ble_gatts_hvx(p_boc->conn_handle, &hvx_params);
	}
	else
	{
			err_code = NRF_ERROR_INVALID_STATE;
	}

    return err_code;
}
