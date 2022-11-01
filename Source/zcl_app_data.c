#include "AF.h"
#include "OSAL.h"
#include "ZComDef.h"
#include "ZDConfig.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ms.h"
#include "zcl_ha.h"

#include "zcl_app.h"

#include "battery.h"
#include "version.h"
/*********************************************************************
 * CONSTANTS
 */

#define APP_DEVICE_VERSION 2
#define APP_FLAGS 0

#define APP_HWVERSION 1
#define APP_ZCLVERSION 1

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// Global attributes
const uint16 zclApp_clusterRevision_all = 0x0001;

//uint8 zclApp_StateText[] = {7, '0', '1', '2', '3', '4', '5', '6'};
uint8 zclApp_StateText[128];
uint16 zclApp_presentValue = 0;

#define DEFAULT_CfgBatteryPeriod 30 // min

application_config_t zclApp_Config = {
                                      .CfgBatteryPeriod = DEFAULT_CfgBatteryPeriod
};

// Basic Cluster
const uint8 zclApp_HWRevision = APP_HWVERSION;
const uint8 zclApp_ZCLVersion = APP_ZCLVERSION;
const uint8 zclApp_ApplicationVersion = 3;
const uint8 zclApp_StackVersion = 4;

//{lenght, 'd', 'a', 't', 'a'}
const uint8 zclApp_ManufacturerName[] = {7, 'V', 'O', 'R', 'O', 'N', 'E', 'G'};
const uint8 zclApp_ModelId[] = {12, 'V', 'O', 'R', 'O', 'N', 'E', 'G', '_', 'U', 'A', 'R', 'T'};
const uint8 zclApp_PowerSource = POWER_SOURCE_BATTERY;

/*********************************************************************
 * ATTRIBUTE DEFINITIONS - Uses REAL cluster IDs
 */

CONST zclAttrRec_t zclApp_AttrsFirstEP[] = {
    {BASIC, {ATTRID_BASIC_ZCL_VERSION, ZCL_UINT8, R, (void *)&zclApp_ZCLVersion}},
    {BASIC, {ATTRID_BASIC_APPL_VERSION, ZCL_UINT8, R, (void *)&zclApp_ApplicationVersion}},
    {BASIC, {ATTRID_BASIC_STACK_VERSION, ZCL_UINT8, R, (void *)&zclApp_StackVersion}},
    {BASIC, {ATTRID_BASIC_HW_VERSION, ZCL_UINT8, R, (void *)&zclApp_HWRevision}},   
    {BASIC, {ATTRID_BASIC_MANUFACTURER_NAME, ZCL_DATATYPE_CHAR_STR, R, (void *)zclApp_ManufacturerName}},
    {BASIC, {ATTRID_BASIC_MODEL_ID, ZCL_DATATYPE_CHAR_STR, R, (void *)zclApp_ModelId}},
    {BASIC, {ATTRID_BASIC_DATE_CODE, ZCL_DATATYPE_CHAR_STR, R, (void *)zclApp_DateCode}},
    {BASIC, {ATTRID_BASIC_POWER_SOURCE, ZCL_DATATYPE_ENUM8, R, (void *)&zclApp_PowerSource}},
    {BASIC, {ATTRID_BASIC_SW_BUILD_ID, ZCL_DATATYPE_CHAR_STR, R, (void *)zclApp_DateCode}},
    {BASIC, {ATTRID_CLUSTER_REVISION, ZCL_DATATYPE_UINT16, R, (void *)&zclApp_clusterRevision_all}},   
    
    {POWER_CFG, {ATTRID_POWER_CFG_BATTERY_VOLTAGE, ZCL_UINT8, RR, (void *)&zclBattery_Voltage}},
/**
 * FYI: calculating battery percentage can be tricky, since this device can be powered from 2xAA or 1xCR2032 batteries
 * */
    {POWER_CFG, {ATTRID_POWER_CFG_BATTERY_PERCENTAGE_REMAINING, ZCL_UINT8, RR, (void *)&zclBattery_PercentageRemainig}},
    {POWER_CFG, {ATTRID_POWER_CFG_BATTERY_PERIOD, ZCL_UINT16, RRW, (void *)&zclApp_Config.CfgBatteryPeriod}},
//    {POWER_CFG, {ATTRID_POWER_CFG_BAT_MANU, ZCL_CHAR_STR, RW, (void *)&zclApp_StateText}}

    {MS_VALUE_BASIC, {ATTRID_IOV_BASIC_STATE_TEXT, ZCL_CHAR_STR, RW, (void *)&zclApp_StateText}},
    {MS_VALUE_BASIC, {ATTRID_IOV_BASIC_PRESENT_VALUE, ZCL_UINT16, RRW, (void *)&zclApp_presentValue}}
};

uint8 CONST zclApp_AttrsFirstEPCount = (sizeof(zclApp_AttrsFirstEP) / sizeof(zclApp_AttrsFirstEP[0]));

const cId_t zclApp_InClusterList[] = {BASIC};

#define APP_MAX_INCLUSTERS (sizeof(zclApp_InClusterList) / sizeof(zclApp_InClusterList[0]))


const cId_t zclApp_OutClusterListFirstEP[] = {POWER_CFG, MS_VALUE_BASIC};

#define APP_MAX_OUTCLUSTERS_FIRST_EP (sizeof(zclApp_OutClusterListFirstEP) / sizeof(zclApp_OutClusterListFirstEP[0]))



SimpleDescriptionFormat_t zclApp_FirstEP = {
    1,                                                  //  int Endpoint;
    ZCL_HA_PROFILE_ID,                                  //  uint16 AppProfId[2];
    ZCL_HA_DEVICEID_SIMPLE_SENSOR,                      //  uint16 AppDeviceId[2];
    APP_DEVICE_VERSION,                          //  int   AppDevVer:4;
    APP_FLAGS,                                   //  int   AppFlags:4;
    APP_MAX_INCLUSTERS,                          //  byte  AppNumInClusters;
    (cId_t *)zclApp_InClusterList,                //  byte *pAppInClusterList;
    APP_MAX_OUTCLUSTERS_FIRST_EP,                //  byte  AppNumInClusters;
    (cId_t *)zclApp_OutClusterListFirstEP         //  byte *pAppInClusterList;
};

void zclApp_ResetAttributesToDefaultValues(void) {
    zclApp_Config.CfgBatteryPeriod = DEFAULT_CfgBatteryPeriod;    
}