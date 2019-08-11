/***************************************************************************
* Description: 
*
**********************************History***********************************
*     when              who                      what, where, why
* ------------    ---------------    ---------------------------------------
*  2017/04/27
*
****************************************************************************/

#include "jan_oem.h"
#include "jan_oem_common.h"

#include "jan_db_common.h"
#include "jan_voice_core.h"
#include "jan_voice_datastore.h"


#define JRD_VOICE_DATASTORE_SQL_COMMAND_MAX 256
#define JRD_VOICE_DEBUG

/*===========================================================================
  Function:  jan_voice_datastore_insert
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
int jan_voice_datastore_insert(jan_voice_call_record_t *record, int limit_num)
{
    char sql_command[JRD_VOICE_DATASTORE_SQL_COMMAND_MAX] = {0};
    int rc = JRD_NO_ERR;

    if (!record) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Pointer is NULL.\n");
        return JRD_FAIL;
    }

    JRD_SPRINTF(sql_command, "INSERT INTO CallRecord(ID, Contacts, ContactNumber, Date, Duration, Type) VALUES (%d, '%s', '%s', %d, %d, %d)", 
        record->id, record->contacts, record->contact_number, record->call_date, record->call_duration, record->call_type);
#ifdef JRD_VOICE_DEBUG
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info, sql_command = %s.\n", sql_command);
#endif

    jan_voice_datastore_limit(record, limit_num);

	return  jan_db_exec_cmd(sql_command, NULL, NULL, 0);
}


/*===========================================================================
  Function:  jan_voice_datastore_limit
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
int jan_voice_datastore_limit(jan_voice_call_record_t *record, int limit_num)
{
    int del_id = 0;
    int index = 0;

    if (!record) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Pointer is NULL.\n");
        return JRD_FAIL;
    }

    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info[yuan.jiang], record->id = %d.\n", record->id);
    /* id > 100, delete (id - 100) record data, make sure records are new 100 records. */
    if (record->id > limit_num) {
        del_id = record->id - limit_num;
        for (index = 1; index <= del_id; index++) {
            jan_voice_datastore_delete(index);
        }
    }
    return JRD_NO_ERR;
}


/*===========================================================================
  Function:  jan_voice_datastore_delete
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
int jan_voice_datastore_delete(int id)
{
    char sql_command[JRD_VOICE_DATASTORE_SQL_COMMAND_MAX] = {0};
    int rc = JRD_NO_ERR;

    JRD_SPRINTF(sql_command, "DELETE FROM CallRecord WHERE ID = %d", id);
#ifdef JRD_VOICE_DEBUG
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info[yuan.jiang], sql_command = %s.\n", sql_command);
#endif

	return  jan_db_exec_cmd(sql_command, NULL, NULL, 0);
}


/*===========================================================================
  Function:  jan_voice_datastore_clear
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
int jan_voice_datastore_clear(void)
{
    char sql_command[JRD_VOICE_DATASTORE_SQL_COMMAND_MAX] = {0};
    int rc = JRD_NO_ERR;

    JRD_SPRINTF(sql_command, "DELETE FROM CallRecord");
#ifdef JRD_VOICE_DEBUG
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info, sql_command = %s.\n", sql_command);
#endif

	return  jan_db_exec_cmd(sql_command, NULL, NULL, 0);
}


/*===========================================================================
  Function:  jan_voice_datastore_callback
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
static int jan_voice_datastore_callback(void *ptr_param, int n_count, char** ptr_value,char** ptr_name)
{
    //jan_voice_datastore_cb call_back = (jan_voice_datastore_cb )ptr_param;

    if (!ptr_param) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Pointer is NULL.\n");
        return JRD_FAIL;        
    }

    #ifdef JRD_VOICE_DEBUG
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info, n_count = %d.\n", n_count);
    #endif

    ((jan_voice_datastore_cb )ptr_param)(n_count, ptr_value, ptr_name);
    return JRD_NO_ERR;
    //return ((jan_voice_datastore_cb )ptr_param)(n_count, ptr_value, ptr_name);
    //call_back(n_count, ptr_value, ptr_name);
}


/*===========================================================================
  Function:  jan_voice_datastore_init
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
int jan_voice_datastore_init(jan_voice_datastore_cb ptr_cb)
{
    char sql_command[JRD_VOICE_DATASTORE_SQL_COMMAND_MAX] = {0};
    int rc = JRD_NO_ERR;

    if (!ptr_cb) {
        JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Error, Pointer is NULL.\n");
        return JRD_FAIL;
    }

    JRD_SPRINTF(sql_command, "SELECT * FROM CallRecord");
#ifdef JRD_VOICE_DEBUG
    JRD_OEM_LOG_INFO(JRD_OEM_LOG_ERROR,"Info, sql_command = %s.\n", sql_command);
#endif

	return  jan_db_exec_cmd(sql_command, jan_voice_datastore_callback, (void *)ptr_cb, 0);
}
