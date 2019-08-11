/***************************************************************************
**********************************History***********************************
*     when              who                      what, where, why
* ------------    ---------------    ---------------------------------------
*  2017/02/23
*
****************************************************************************/

#include "jan_oem.h"
#include "jan_oem_common.h"
#include "jan_oem_indication.h"
#include "jan_input_dev_listen.h"
#include "jan_thread/jan_thread.h"
#include <linux/input.h>

#include "jan_voice.h"
#include "jan_voice_core.h"

#define JRD_VOICE_EMERGENCY_BUFF_LEN 64

/* Private data about voice module. */
typedef struct jan_voice_pridata_struct {
    char emergency_number[JRD_VOICE_EMERGENCY_BUFF_LEN];
} jan_voice_pridata_t;
static jan_voice_pridata_t g_voice_config_data = {0};


static os_signal_type           jan_voice_os_signal;
static q_type                   jan_voice_q;
static jan_thread_info_type jan_voice_thread_info = {
  MODULE_VOICE,
  &jan_voice_os_signal,
  &jan_voice_q,
  NULL,
  NULL
};


static jan_ind_list_type *jan_voice_ind_list[E_JRD_VOICE_IND_MAX] = {0};
static jan_ind_list_info_type jan_voice_ind_info = {
    jan_voice_ind_list,
    MODULE_VOICE,
    E_JRD_VOICE_IND_MAX,
};

static jan_param_key_info_t jan_voice_config_data_table[]=
{
    {&(g_voice_config_data.emergency_number), "EmergencyNumber", E_PARAM_STR, sizeof(g_voice_config_data.emergency_number), E_JRD_VOICE_CONFIG_ID_EMERGENCY_NUMBER},
    {NULL,NULL,0,0,0},
};
static jan_param_table_info_t jan_voice_config_data_table_info = 
{
  "voice_config",
  jan_voice_config_data_table,
};


/*===========================================================================
  Function:  jan_voice_get_emergency_number
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/

int jan_voice_get_emergency_number (char *number_string)
{
    JRD_STRNCPY (number_string, g_voice_config_data.emergency_number, JRD_STRLEN (g_voice_config_data.emergency_number));
    return 0;
}

/*===========================================================================
  FUNCTION  jan_voice_msg_process
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_msg_process(e_jan_voice_event_t event, void *msg_data, int msg_data_len, void *cb_param)
{
    jan_ind_data_type ind_info = {0};

    JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW,"Info, voice module callback event:%d.\n", event);
    switch (event)
    {
        case E_JRD_VOICE_EVENT_CALL_STATUS_CHANGE:
        {
            ind_info.ind_id = E_JRD_VOICE_IND_SELF_CALL_STATUS_CHANGE;
            ind_info.ind_data = (jan_voice_call_info_t *)msg_data;
            ind_info.data_size = msg_data_len;
            break;
        }
        default:
        {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW,"Info, unknown voice msg event: %d.\n", event);
            return JRD_FAIL;
        }
    }

    /* Indication self, to process msg event. */
    ind_info.module_id = MODULE_VOICE;
    jan_oem_ind(&ind_info);
    return JRD_NO_ERR;
}


/*===========================================================================
  Function:  jan_voice_msg_handle
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
static int jan_voice_msg_handle(jan_ind_data_type *indication)
{
    if (!indication || !indication->ind_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }
    
    if (indication->module_id != MODULE_VOICE) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW,"Info, not voice self indication message.\n");
        return JRD_FAIL;
    }

    /* Handle msg notify. */
    switch (indication->ind_id)
    {
        case E_JRD_VOICE_IND_SELF_CALL_STATUS_CHANGE:
        {
            jan_voice_core_network_event_handle((jan_voice_call_info_t *)indication->ind_data);
            break;
        }
        default:
        {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, unknown voice indication self id:%d.\n", indication->ind_id);
            return JRD_FAIL;
        }
    }
    return JRD_NO_ERR;
}


/*===========================================================================
  FUNCTION  jan_misc_input_ind_handler
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_input_ind_handler(jan_ind_data_type *indication)
{
    struct input_event *input_data = NULL;
    int rc = JRD_NO_ERR;

    if (!indication || !indication->ind_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }

    if(indication->ind_data) /* key event handle */
    {
        input_data = (struct input_event *)indication->ind_data;

        if (input_data->type != EV_SND) {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, input event type:0x%x.\n", (int)input_data->type);
            return JRD_FAIL;
        }

        jan_voice_core_device_event_handle();

        /*Input dev listen malloc the ind_data, so here is free the memory*/
        JRD_FREE(indication->ind_data);
    }
    else
    {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Invalid input event pointer\n");
        return JRD_FAIL;
    }

    return JRD_NO_ERR;
}


/*===========================================================================
  FUNCTION  jan_voice_timezone_handle
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_timezone_handle(jan_ind_data_type *indication)
{
    if (!indication || !indication->ind_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }
    
    if (indication->module_id != MODULE_VOICE) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Info, not voice self indication message.\n");
        return JRD_FAIL;
    }

    /* Handle msg notify. */
    switch (indication->ind_id)
    {
        case E_JRD_VOICE_IND_TIMEZONE_STATUS_CHANGE:
        {
            jan_voice_core_timezone_handle((jan_voice_timezone_data_t *)indication->ind_data);
            break;
        }
        default:
        {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, unknown voice indication self id:%d.\n", indication->ind_id);
            return JRD_FAIL;
        }
    }
    return JRD_NO_ERR;
}

/*===========================================================================
  FUNCTION  jan_voice_get_call_record_list
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_get_call_record_list(jan_cmd_data_type *command_data, void *param)
{
    ordered_list_link_type *list_node_link = NULL;
    jan_voice_node_list_t *this_record_node = NULL;
    jan_voice_call_record_t *record = NULL;
    json_object *record_list_array = NULL;
    json_object *record_object = NULL;
    ordered_list_type *record_list = NULL;
    char record_date[32] = {0};
    int list_type = 0;
    int page = 0;
    int total_page_num = 0;
    int total_record_num = 0;
    int index = 0;
    int count = 0;
    int rc = JRD_NO_ERR;

    if (!command_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }

    rc = jan_oem_get_param_from_req_object(command_data,
                                            MODULE_VOICE,
                                            E_JRD_VOICE_PARAM_LIST_TYPE,
                                            NULL,
                                            &list_type,
                                            0);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, get list_type param.\n");
        return JRD_FAIL;
    }

    rc = jan_oem_get_param_from_req_object(command_data,
                                            MODULE_VOICE,
                                            E_JRD_VOICE_PARAM_PAGE,
                                            NULL,
                                            &page,
                                            0);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, get page_num param.\n");
        return JRD_FAIL;
    }

#if 1  /* 0,ALL  1,INCOMING  2,OUTGOING  3,MISSED */
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, list_type = %d.\n", list_type);
#endif

    record_list = jan_voice_core_get_record_list(list_type);
    if (!record_list) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, record list, page type:%d.\n", list_type);
        return JRD_FAIL;
    }

    total_record_num = ordered_list_size(record_list);

#if 1
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, page = %d.\n", page);
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, total_record_num = %d.\n", total_record_num);
#endif

    if (total_record_num > 0) {
        list_node_link = ordered_list_peek_back(record_list);
        /* 
         * One page display 10 record:
         * 
         *   [n/10] + [(n - [n/10] * 10) / 10]
         *   [(n + 9) / 10]
         *
        */
        //total_page_num = (total_record_num / 10) + ((total_record_num - ((total_record_num / 10) * 10)) / 10);
        total_page_num = ((total_record_num + 9) / 10);
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, total_page_num = %d.\n", total_page_num);
    } else if (total_record_num == 0) {
        /* No Record. */
        return JRD_NO_ERR;
    } else {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, not have record in list, type:%d.\n", list_type);
        return JRD_FAIL;
    }

    record_list_array = json_object_new_array();
    if (!record_list_array) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, new record list array.\n");
        return JRD_FAIL;
    }

    if (page == 0) {  /* Response all call record. */
        while (list_node_link != NULL) {
            record_object = json_object_new_object();
            if (!record_object) {
                JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, new record object.\n");
                return JRD_FAIL;
            }

            if (list_type == E_JRD_VOICE_LIST_TYPE_ALL) {
                this_record_node = (jan_voice_node_list_t *)list_node_link;
            } else {
                this_record_node = (jan_voice_node_list_t *)(((jan_voice_node_list_t *)list_node_link)->data);
            }

            if (this_record_node->data) {
                record = (jan_voice_call_record_t *)(this_record_node->data);

                change_time_to_string_for_general(record_date, record->call_date);
                json_object_object_add(record_object, "Id",          json_object_new_int(record->id));
                json_object_object_add(record_object, "TelNumber",   json_object_new_string(record->contact_number));
                json_object_object_add(record_object, "Date",        json_object_new_string(record_date));
                json_object_object_add(record_object, "Duration",    json_object_new_int(record->call_duration));
                json_object_object_add(record_object, "CallLogType", json_object_new_int(record->call_type));
                json_object_array_add(record_list_array, record_object);
            }
            list_node_link =  (ordered_list_link_type *) ordered_list_peek_prev(record_list, list_node_link);
        }/* end while (this_msg_node...) */
    } else {
        if (total_page_num < page) {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, page count too max.\n");
            return JRD_FAIL;
        }

        for (index = 0; index < ((page - 1) * 10); index++) {
            list_node_link =  (ordered_list_link_type *) ordered_list_peek_prev(record_list, list_node_link);
        }
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Info, index = %d.\n", index);

        while (list_node_link != NULL) {
            if (count < 10) {
                count++;
            } else {
                break;
            }

            record_object = json_object_new_object();
            if (!record_object) {
                JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, new record object.\n");
                return JRD_FAIL;
            }

            if (list_type == E_JRD_VOICE_LIST_TYPE_ALL) {
                this_record_node = (jan_voice_node_list_t *)list_node_link;
            } else {
                this_record_node = (jan_voice_node_list_t *)(((jan_voice_node_list_t *)list_node_link)->data);
            }

            if (this_record_node->data) {
                record = (jan_voice_call_record_t *)(this_record_node->data);

                change_time_to_string_for_general(record_date, record->call_date);
                json_object_object_add(record_object, "Id",          json_object_new_int(record->id));
                json_object_object_add(record_object, "TelNumber",   json_object_new_string(record->contact_number));
                json_object_object_add(record_object, "Date",        json_object_new_string(record_date));
                json_object_object_add(record_object, "Duration",    json_object_new_int(record->call_duration));
                json_object_object_add(record_object, "CallLogType", json_object_new_int(record->call_type));
                json_object_array_add(record_list_array, record_object);
            }
            list_node_link =  (ordered_list_link_type *) ordered_list_peek_prev(record_list, list_node_link);
        }/* end while (this_msg_node...) */
    }


    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_RECORD_LIST,
                                        record_list_array);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_PAGE,
                                        &page);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_TOTAL_PAGE_COUNT,
                                        &total_page_num);
    return JRD_NO_ERR;
}


/*===========================================================================
  FUNCTION  jan_voice_delete_call_record
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_delete_call_record(jan_cmd_data_type *command_data, void *param)
{
    int list_type = 0;
    int record_id = 0;
    int count_record_id = 0;
    int total_record_num = 0;
    boolean is_record_id = FALSE;
    ordered_list_type *record_list = NULL;
    ordered_list_link_type *list_node_link = NULL;
    int rc = JRD_NO_ERR;

    if (!command_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }

    rc = jan_oem_get_param_from_req_object(command_data,
                                            MODULE_VOICE,
                                            E_JRD_VOICE_PARAM_LIST_TYPE,
                                            NULL,
                                            &list_type,
                                            0);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, get list_type param.\n");
        return JRD_FAIL;
    }

    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"(Fan.Xi)Info, list_type:%d.\n", list_type);

    record_list = jan_voice_core_get_record_list(list_type);
    if (!record_list) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, record list, page type:%d.\n", list_type);
        return JRD_FAIL;
    }

    total_record_num = ordered_list_size(record_list);
    if (total_record_num > 0) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW,"Info, total_record_num:%d.\n", total_record_num);
    } else {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, not have record in list, type:%d.\n", list_type);
        return JRD_FAIL;
    }

    while (0 <= jan_oem_get_param_from_req_object(command_data,
                                                    MODULE_VOICE,
                                                    E_JRD_VOICE_PARAM_RECORD_ID,
                                                    NULL,
                                                    &record_id,
                                                    count_record_id)) {
        count_record_id++;
        rc = jan_voice_core_delete_record_id(list_type, record_id);
        if (rc == JRD_NO_ERR) {
            is_record_id = TRUE;
        }
    }

    if (count_record_id == 0) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Cann't get call record id.\n");
        command_data->error_code = 0;
        return JRD_NO_ERR;
    }

    if (is_record_id == FALSE) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Cann't find call record id:%s.\n", record_id);
        command_data->error_code = 0;
        return JRD_NO_ERR;
    }
    return JRD_NO_ERR;
}


/*===========================================================================
  FUNCTION  jan_voice_clear_call_record_list
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_clear_call_record_list(jan_cmd_data_type *command_data, void *param)
{
    ordered_list_type *record_list = NULL;
    int list_type = 0;
    int rc = JRD_NO_ERR;

    if (!command_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }

    rc = jan_oem_get_param_from_req_object(command_data,
                                            MODULE_VOICE,
                                            E_JRD_VOICE_PARAM_LIST_TYPE,
                                            NULL,
                                            &list_type,
                                            0);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, get list_type param.\n");
        return JRD_FAIL;
    }

    return JRD_NO_ERR;
}


/*===========================================================================
  FUNCTION  jan_voice_get_call_record_count_info
===========================================================================*/
/*!
@brief


@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_get_call_record_count_info(jan_cmd_data_type *command_data, void *param)
{
    jan_voice_call_record_count_info_t *count_info = NULL;
    int rc = JRD_NO_ERR;

    if (!command_data) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, param pointer is NULL.\n");
        return JRD_FAIL;
    }

    count_info = jan_voice_core_get_record_count_info();
    if (!count_info) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Get record count info.\n");
        return JRD_FAIL;
    }

    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_TOTAL_COUNT_MAX,
                                        &count_info->max_count);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_INCOMING_COUNT,
                                        &count_info->incoming_count);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_OUTGOING_COUNT,
                                        &count_info->outgoing_count);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_MISSED_COUNT,
                                        &count_info->missed_count);
    jan_oem_add_param_to_resp_object(command_data,
                                        MODULE_VOICE,
                                        E_JRD_VOICE_PARAM_TOTAL_COUNT,
                                        &count_info->total_count);
    return JRD_NO_ERR;
}


typedef int (*jan_oem_act_func)(jan_cmd_data_type *, void *);
static const jan_oem_act_func jan_voice_act_table[E_JRD_VOICE_ACT_ID_MAX] = {
    jan_voice_get_call_record_list,  /*0*/
    jan_voice_delete_call_record,
    jan_voice_clear_call_record_list,
    jan_voice_get_call_record_count_info,
};


static  jan_ind_proc_info_type jan_voice_inds_cb_table[] = {
    {MODULE_INPUT_LISTEN, JRD_INPUT_DEV_EVENT_IND, jan_voice_input_ind_handler},
    {MODULE_VOICE, E_JRD_VOICE_IND_SELF_CALL_STATUS_CHANGE, jan_voice_msg_handle},
    {MODULE_VOICE, E_JRD_VOICE_IND_TIMEZONE_STATUS_CHANGE, jan_voice_timezone_handle},
};


/*===========================================================================
  Function:  jan_voice_ind_cb_init
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
static int jan_voice_ind_cb_init(void)
{
    int count = sizeof(jan_voice_inds_cb_table) / sizeof(jan_voice_inds_cb_table[0]);
    jan_ind_proc_info_type *inds = jan_voice_inds_cb_table;
    int index = 0;
    int rc = JRD_NO_ERR;

    for(index = 0; index < count; index++)
    {
        rc = jan_oem_register_ind_cb(&inds[index], MODULE_VOICE);
        if (rc != JRD_NO_ERR) {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "failed to register ind, rc=%d\n", rc);
        }
    }
    return JRD_NO_ERR;
}


/*===========================================================================
  Function:  jan_voice_command_handle
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
static int jan_voice_command_handle(jan_cmd_q_type *command)
{
    jan_ind_data_type *indication = NULL;
    jan_cmd_data_type *command_data = NULL;
    int index = 0;
    int rc = JRD_NO_ERR;

    if (!command) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, Command pointer is invalid.\n");
        return JRD_FAIL;
    }

    switch (command->cmd_hdr.type)
    {
        case CMD_WEB_REQ:
        case CMD_RSP_WEB:
        case CMD_UI_REQ:
        case CMD_RSP_UI:
        case CMD_NORMAL_REQ:
        case CMD_RSP_NORMAL:
        {
            /* Change the pointer type. */
            command_data = (jan_cmd_data_type *)command->cmd_data;

            JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW, "sub_mask: %d %d %d %d\n", command_data->sub_mask[0], 
                command_data->sub_mask[1], command_data->sub_mask[2], command_data->sub_mask[3]);
            jan_voice_act_table[command->cmd_hdr.act_id](command_data, NULL);

            command->cmd_hdr.type = CMD_RSP_WEB;
            if (command_data->jan_cmd_cb) {
                command_data->jan_cmd_cb(command, command_data->user_data);
            }
            break;
        }
        case CMD_IND:
        {
            /* Change the pointer type. */
            indication = (jan_ind_data_type *)command->cmd_data;

            for (index = 0; index < sizeof(jan_voice_inds_cb_table)/sizeof(jan_voice_inds_cb_table[0]); index++) {
                if (jan_voice_inds_cb_table[index].module_id == indication->module_id
                    && jan_voice_inds_cb_table[index].ind_id == indication->ind_id) {

                    jan_voice_inds_cb_table[index].jan_ind_proc_cb(indication);
                }
            }

            jan_free_cmd_q_data(command);
            break;
        }
        default:
        {
            JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, Iinvalid CMD type: %d.\n", command->cmd_hdr.type);
            break;
        }
    }
    return rc;
}


/*===========================================================================
  Function:  jan_voice_thread
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
void* jan_voice_thread(void *args)
{
    jan_thread_base_type *thread_info = NULL;
    jan_cmd_q_type *command = NULL;
    uint32 req_signal = 0;
    int rc = JRD_NO_ERR;

    thread_info = jan_thread_base_module_get(MODULE_VOICE);
    if (!thread_info) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_MEDIAM,"Warn, Invalid parameter.\n");
        return NULL;
    }

    /* Init Sound-Card PCM. */
    rc = jan_voice_soundcard_init();
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Failed to init sound-card pcm.\n");
        return NULL;
    }

    rc = jan_voice_core_init();
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Failed to init slic device.\n");
        return NULL;
    }

    /* Register voice message event callback. */
    rc = jan_voice_core_event_register(jan_voice_msg_process, NULL);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, register msg event callback.\n");
        return NULL;
    }


    /* Enable soft dog monitor. */
    jan_soft_dog_register_monitor(thread_info);

    while(1) {
        req_signal = jan_os_signal_handle(&jan_voice_os_signal);
        if (JRD_SOFT_DOG_SIGNAL == (JRD_SOFT_DOG_SIGNAL & req_signal)) {
            jan_soft_dog_response(thread_info);
        }

        if (JRD_Q_CMD_SIGNAL == (JRD_Q_CMD_SIGNAL & req_signal)) {
            while (NULL != (command = q_get(&jan_voice_q))) {
                jan_voice_command_handle(command);
            }
        }
        req_signal = 0;
    }

    q_destroy(&jan_voice_q);
    JRD_OS_SIGNAL_DEINIT(&jan_voice_os_signal);
    return;
}


/*===========================================================================
  FUNCTION  jan_voice_config_data_init
===========================================================================*/
/*!
@brief
  Get voice module data form database and save to the cache.

@return
  JRD_NO_ERR if success, negative value(JRD_FAIL) if not.

*/
/*=========================================================================*/
static int jan_voice_config_data_init(void)
{
    int rc = JRD_NO_ERR;

    /* Init voice-module config data from DataBase. */
    rc = jan_db_query_db_init_struct(&jan_voice_config_data_table_info);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, update voice config data table.\n");

        strncpy (g_voice_config_data.emergency_number, "110,112,911", sizeof (g_voice_config_data.emergency_number));
    }

    return JRD_NO_ERR;
}


/*===========================================================================
  Function:  jan_voice_init
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
int jan_voice_init(void)
{
    jan_thread_data_t voice_thread = {0};
    int rc = JRD_NO_ERR;

    if (jan_thread_judge_disable(MODULE_VOICE) == TRUE) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW, "Info, MODULE_VOICE Thread is not init.\n");
        goto error;
    }

    /* Init or register voice thread info to indication table. */
    q_init(&jan_voice_q);
    JRD_OS_SIGNAL_INIT(&jan_voice_os_signal);
    if (jan_oem_register_thread_info(&jan_voice_thread_info)) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, register voice thread info.\n");
        goto error;
    }
    jan_voice_ind_cb_init();

    /* Voice config data init -- init database. */
    rc = jan_voice_config_data_init();
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR, "Error, init voice config data table failed.\n");
        goto error;
    }

    /* Init and creat voice module thread. */
    voice_thread.stacksize = JRD_OEM_THREAD_STACK_SIZE;
    voice_thread.user_func = jan_voice_thread;
    voice_thread.user_data = NULL;
    rc = jan_thread_create(&voice_thread);
    if (rc != JRD_NO_ERR) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, voice module init thread.\n");
        goto error;
    }
    return JRD_NO_ERR;

error:
    return JRD_FAIL;
}


/*===========================================================================
  Function:  jan_voice_thread
===========================================================================*/
/*!
@brief
  None.

@parameters
  None.

@return
  None.
*/
/*=========================================================================*/
int jan_voice_early_init(void)
{
    int rc = JRD_NO_ERR;

    if (jan_thread_judge_disable(MODULE_VOICE) == TRUE) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW, "Info, MODULE_VOICE Thread is not init.\n");
        return JRD_FAIL;
    }

    JRD_OEM_LOG_INFO(JRD_OEM_LOG_LOW, "start %d!\n", jan_voice_ind_info.module_id);
    /* Init or register VOICE MODULE to indication. */
    jan_oem_register_ind(&jan_voice_ind_info);
    return JRD_NO_ERR;
}
