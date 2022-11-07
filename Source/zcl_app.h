#ifndef ZCL_APP_H
#define ZCL_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * INCLUDES
 */
#include "version.h"
#include "zcl.h"


/*********************************************************************
 * CONSTANTS
 */

// Application Events
#define APP_REPORT_EVT                  0x0001
#define APP_READ_SENSORS_EVT            0x0002
#define APP_SAVE_ATTRS_EVT              0x0080
#define APP_REPORT_BATTERY_EVT          0x4000

#define APP_REPORT_DELAY ((uint32) 1800000) //30 minutes

/*********************************************************************
 * MACROS
 */
#define NW_APP_CONFIG 0x0401

#define R           ACCESS_CONTROL_READ
#define RR          (R | ACCESS_REPORTABLE)
#define RW          (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE | ACCESS_CONTROL_AUTH_WRITE)
#define RRW          (ACCESS_CONTROL_READ| ACCESS_REPORTABLE | ACCESS_CONTROL_WRITE | ACCESS_CONTROL_AUTH_WRITE)

#define BASIC                ZCL_CLUSTER_ID_GEN_BASIC
#define ONOFF                ZCL_CLUSTER_ID_GEN_ON_OFF
#define POWER_CFG            ZCL_CLUSTER_ID_GEN_POWER_CFG
#define MS_VALUE_BASIC       ZCL_CLUSTER_ID_GEN_MULTISTATE_VALUE_BASIC

#define ZCL_BOOLEAN   ZCL_DATATYPE_BOOLEAN
#define ZCL_UINT8     ZCL_DATATYPE_UINT8
#define ZCL_UINT16    ZCL_DATATYPE_UINT16
#define ZCL_UINT32    ZCL_DATATYPE_UINT32
#define ZCL_INT16     ZCL_DATATYPE_INT16
#define ZCL_INT8      ZCL_DATATYPE_INT8
#define ZCL_BITMAP8   ZCL_DATATYPE_BITMAP8
#define ZCL_ENUM8     ZCL_DATATYPE_ENUM8
#define ZCL_UNKNOWN   ZCL_DATATYPE_UNKNOWN
#define ZCL_OCTET_STR ZCL_DATATYPE_OCTET_STR
#define ZCL_CHAR_STR  ZCL_DATATYPE_CHAR_STR
#define ZCL_ARRAY     ZCL_DATATYPE_ARRAY


/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * VARIABLES
 */

extern SimpleDescriptionFormat_t zclApp_FirstEP;

//extern uint8 zclApp_BatteryVoltage;
//extern uint8 zclApp_BatteryPercentageRemainig; 
extern uint8 zclApp_StateText[];

typedef struct
{
    uint16 CfgBatteryPeriod;
}  application_config_t;

extern application_config_t zclApp_Config;

// attribute list
extern CONST zclAttrRec_t zclApp_AttrsFirstEP[];

extern CONST uint8 zclApp_AttrsFirstEPCount;

extern const uint8 zclApp_ManufacturerName[];
extern const uint8 zclApp_ModelId[];
extern const uint8 zclApp_PowerSource;

// APP_TODO: Declare application specific attributes here

/*********************************************************************
 * FUNCTIONS
 */

/*
 * Initialization for the task
 */
extern void zclApp_Init(byte task_id);

/*
 *  Event Process for the task
 */
extern UINT16 zclApp_event_loop(byte task_id, UINT16 events);

//void user_delay_ms(uint32_t period);

extern void zclApp_ResetAttributesToDefaultValues(void);

#ifdef __cplusplus
}
#endif

#endif /* ZCL_APP_H */
