
#include "AF.h"
#include "OSAL.h"
#include "OSAL_Clock.h"
#include "OSAL_PwrMgr.h"
#include "ZComDef.h"
#include "ZDApp.h"
#include "ZDNwkMgr.h"
#include "ZDObject.h"
#include "math.h"

#include "nwk_util.h"
#include "zcl.h"
#include "zcl_app.h"
#include "zcl_diagnostic.h"
#include "zcl_general.h"
#include "zcl_lighting.h"
#include "zcl_ms.h"

#include "bdb.h"
#include "bdb_interface.h"
#include "gp_interface.h"

#include "Debug.h"

#include "OnBoard.h"

/* HAL */
#include "hal_adc.h"
#include "hal_drivers.h"
#include "hal_key.h"
#include "hal_led.h"

#include "battery.h"
#include "commissioning.h"
#include "factory_reset.h"
#include "utils.h"
#include "version.h"

/*********************************************************************
 * MACROS
 */
#define HAL_KEY_P0_EDGE_BITS HAL_KEY_BIT0

#define HAL_KEY_P1_EDGE_BITS (HAL_KEY_BIT1 | HAL_KEY_BIT2)

#define HAL_KEY_CODE_RELEASE_KEY HAL_KEY_CODE_NOKEY

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

extern bool requestNewTrustCenterLinkKey;
byte zclApp_TaskID;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static uint8 currentSensorsReadingPhase = 0;

afAddrType_t inderect_DstAddr = {.addrMode = (afAddrMode_t)AddrNotPresent, .endPoint = 0, .addr.shortAddr = 0};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void zclApp_HandleKeys(byte shift, byte keys);
static void zclApp_Report(void);

static void zclApp_BasicResetCB(void);
static void zclApp_RestoreAttributesFromNV(void);
static void zclApp_SaveAttributesToNV(void);
static void zclApp_StopReloadTimer(void);
static void zclApp_StartReloadTimer(void);

static ZStatus_t zclApp_ReadWriteAuthCB(afAddrType_t *srcAddr, zclAttrRec_t *pAttr, uint8 oper);

static void zclApp_ReadSensors(void);
static void zclApp_StateTextReport(void);

static void zclApp_InitUart1(halUARTCBack_t pf);
static void zclApp_Uart1RxCb(uint8 port, uint8 event);
static void zclApp_WriteUart1(void);

void DelayMs(unsigned int delaytime);

#ifdef PWM_LED_PORT0
static void InitLedPWM(uint8 level);
uint8 zclApp_st = 1;
bool zclApp_st_dnup = 1;
#endif
/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t zclApp_CmdCallbacks = {
    zclApp_BasicResetCB, // Basic Cluster Reset command
    NULL, // Identify Trigger Effect command
    NULL, // On/Off cluster commands
    NULL, // On/Off cluster enhanced command Off with Effect
    NULL, // On/Off cluster enhanced command On with Recall Global Scene
    NULL, // On/Off cluster enhanced command On with Timed Off
    NULL, // RSSI Location command
    NULL  // RSSI Location Response command
};

void zclApp_Init(byte task_id) {
    zclApp_RestoreAttributesFromNV();
#ifdef DO_UART_1    
    zclApp_InitUart1(zclApp_Uart1RxCb);
#endif          
    // this is important to allow connects throught routers
    // to make this work, coordinator should be compiled with this flag #define TP2_LEGACY_ZC
    requestNewTrustCenterLinkKey = FALSE;

    zclApp_TaskID = task_id;

    zclGeneral_RegisterCmdCallbacks(1, &zclApp_CmdCallbacks);
    zcl_registerAttrList(zclApp_FirstEP.EndPoint, zclApp_AttrsFirstEPCount, zclApp_AttrsFirstEP);
    bdb_RegisterSimpleDescriptor(&zclApp_FirstEP);
    zcl_registerReadWriteCB(zclApp_FirstEP.EndPoint, NULL, zclApp_ReadWriteAuthCB);

    zcl_registerForMsg(zclApp_TaskID);

    // Register for all key events - This app will handle all key events
    RegisterForKeys(zclApp_TaskID);
    LREP("Started build %s \r\n", zclApp_DateCodeNT);
   
    zclApp_StartReloadTimer();
}

uint16 zclApp_event_loop(uint8 task_id, uint16 events) {
    afIncomingMSGPacket_t *MSGpkt;
    devStates_t zclApp_NwkState; //---
    
    (void)task_id; // Intentionally unreferenced parameter
    if (events & SYS_EVENT_MSG) {
        while ((MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(zclApp_TaskID))) {
            switch (MSGpkt->hdr.event) {
            case KEY_CHANGE:
                zclApp_HandleKeys(((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys);
                
                break;
            case ZDO_STATE_CHANGE:
                zclApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
                LREP("NwkState=%d\r\n", zclApp_NwkState);
                if (zclApp_NwkState == DEV_END_DEVICE) {

                } 
                break;
            case ZCL_INCOMING_MSG:
                if (((zclIncomingMsg_t *)MSGpkt)->attrCmd) {
                    osal_mem_free(((zclIncomingMsg_t *)MSGpkt)->attrCmd);
                }
                break;

            default:
                break;
            }
            // Release the memory
            osal_msg_deallocate((uint8 *)MSGpkt);
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }
    
    if (events & APP_REPORT_BATTERY_EVT) {
        LREPMaster("APP_REPORT_BATTERY_EVT\r\n");
        zclBattery_Report();

        return (events ^ APP_REPORT_BATTERY_EVT);
    }
#ifdef PWM_LED_PORT0    
    if (events & APP_LED_PWM_EVT) {
        LREPMaster("APP_LED_PWM_EVT\r\n");
        if (zclApp_st < 200 && zclApp_st_dnup){
          InitLedPWM(zclApp_st);
          zclApp_st++;
        } else {
          zclApp_st_dnup = 0;
        }
        if (zclApp_st !=1 && !zclApp_st_dnup){
          InitLedPWM(zclApp_st);
          zclApp_st--;
        } else {
          zclApp_st_dnup = 1;
        }

        return (events ^ APP_LED_PWM_EVT);
    }
#endif    
    if (events & APP_REPORT_EVT) {
        LREPMaster("APP_REPORT_EVT\r\n");
        zclApp_Report();
        
        return (events ^ APP_REPORT_EVT);
    }

    if (events & APP_READ_SENSORS_EVT) {
        LREPMaster("APP_READ_SENSORS_EVT\r\n");
        zclApp_ReadSensors();
        return (events ^ APP_READ_SENSORS_EVT);
    }    
    if (events & APP_SAVE_ATTRS_EVT) {
        LREPMaster("APP_SAVE_ATTRS_EVT\r\n");
        zclApp_SaveAttributesToNV();
        
        return (events ^ APP_SAVE_ATTRS_EVT);
    }

    // Discard unknown events
    return 0;
}

static void zclApp_HandleKeys(byte portAndAction, byte keyCode) {
    LREP("zclApp_HandleKeys portAndAction=0x%X keyCode=0x%X\r\n", portAndAction, keyCode);
    zclFactoryResetter_HandleKeys(portAndAction, keyCode);
    zclCommissioning_HandleKeys(portAndAction, keyCode);
    if (portAndAction & HAL_KEY_PRESS) {
        LREPMaster("Key press\r\n");
    }

    bool contact = portAndAction & HAL_KEY_PRESS ? TRUE : FALSE;
    uint8 endPoint = 0;
    if (portAndAction & HAL_KEY_PORT0) {
        LREPMaster("Key press PORT0\r\n");
#ifdef POWER_SAVING         
        osal_pwrmgr_task_state(zclApp_TaskID, PWRMGR_HOLD);
#endif 
//        DelayMs(300); //test WDT
    } else if (portAndAction & HAL_KEY_PORT1) {     
        LREPMaster("Key press PORT1\r\n");

     } else if (portAndAction & HAL_KEY_PORT2) {
       LREPMaster("Key press PORT2\r\n");
       if (contact) {
          HalLedSet(HAL_LED_1, HAL_LED_MODE_BLINK);
          osal_start_timerEx(zclApp_TaskID, APP_REPORT_EVT, 200);
       }
     }
     LREP("contact=%d endpoint=%d\r\n", contact, endPoint);
     uint16 alarmStatus = 0;
     if (!contact) {
        alarmStatus |= BV(0);
     } 
}

static void zclApp_ReadSensors(void) {
    LREP("currentSensorsReadingPhase %d\r\n", currentSensorsReadingPhase);
//    uint8 response[10];
//    uint8 i = 0;
    /**
     * FYI: split reading sensors into phases, so single call wouldn't block processor
     * for extensive ammount of time
     * */
    switch (currentSensorsReadingPhase++) {
    case 0:
//        HalLedSet(HAL_LED_1, HAL_LED_MODE_BLINK);    
      zclBattery_Report();      
        break;
    default:
        osal_stop_timerEx(zclApp_TaskID, APP_READ_SENSORS_EVT);
        osal_clear_event(zclApp_TaskID, APP_READ_SENSORS_EVT);
        currentSensorsReadingPhase = 0;
        break;
    }

}

static void zclApp_WriteUart1(void) {
      for (uint8 i = 1; i <= zclApp_StateText[0]; i++) {
        LREP("[%d]= %d\r\n", i, zclApp_StateText[i]);
        HalUARTWrite(HAL_UART_PORT_1, &zclApp_StateText[i], 1);
      }
}

static void zclApp_InitUart1(halUARTCBack_t pf) {
    halUARTCfg_t halUARTConfig;
    halUARTConfig.configured = TRUE;
    halUARTConfig.baudRate = HAL_UART_BR_115200;
    halUARTConfig.flowControl = FALSE;
    halUARTConfig.flowControlThreshold = 48; // this parameter indicates number of bytes left before Rx Buffer
                                             // reaches maxRxBufSize
    halUARTConfig.idleTimeout = 10;          // this parameter indicates rx timeout period in millisecond
    halUARTConfig.rx.maxBufSize = 128;
    halUARTConfig.tx.maxBufSize = 128;
    halUARTConfig.intEnable = TRUE;
    halUARTConfig.callBackFunc = pf;
    HalUARTInit();
    if (HalUARTOpen(HAL_UART_PORT_1, &halUARTConfig) == HAL_UART_SUCCESS) {
        LREPMaster("Initialized UART1 \r\n");
    }
}

static void zclApp_Uart1RxCb(uint8 port, uint8 event)
{
  uint8  ch;
  zclApp_StateText[0] = Hal_UART_RxBufLen(port);
  while (Hal_UART_RxBufLen(port))
  {
    // Read one byte from UART to ch
    uint8 l = Hal_UART_RxBufLen(port);
    HalUARTRead (port, &ch, 1);
    zclApp_StateText[zclApp_StateText[0] - l + 1] = ch;
    LREP("%d %d\r\n", zclApp_StateText[0] - l + 1, ch);
  }
  zclApp_StateTextReport();
#ifdef POWER_SAVING  
  osal_pwrmgr_task_state(zclApp_TaskID, PWRMGR_CONSERVE);
#endif
}

static void zclApp_Report(void) { osal_start_reload_timer(zclApp_TaskID, APP_READ_SENSORS_EVT, 100); }

static void zclApp_BasicResetCB(void) {
    LREPMaster("BasicResetCB\r\n");
    zclApp_ResetAttributesToDefaultValues();
    zclApp_SaveAttributesToNV();
}

static ZStatus_t zclApp_ReadWriteAuthCB(afAddrType_t *srcAddr, zclAttrRec_t *pAttr, uint8 oper) {
    LREPMaster("AUTH CB called\r\n");

    osal_start_timerEx(zclApp_TaskID, APP_SAVE_ATTRS_EVT, 100);
    return ZSuccess;
}

static void zclApp_SaveAttributesToNV(void) {
//    uint8 writeStatus = osal_nv_write(NW_APP_CONFIG, 0, sizeof(application_config_t), &zclApp_Config);
//    LREP("Saving attributes to NV write=%d\r\n", writeStatus);
    zclApp_WriteUart1();
    zclApp_StopReloadTimer();
    zclApp_StartReloadTimer();
}

static void zclApp_StopReloadTimer(void) {
    osal_stop_timerEx(zclApp_TaskID, APP_REPORT_BATTERY_EVT);
    osal_clear_event(zclApp_TaskID, APP_REPORT_BATTERY_EVT);    
}

static void zclApp_StartReloadTimer(void) {
    osal_start_reload_timer(zclApp_TaskID, APP_REPORT_BATTERY_EVT, (uint32)zclApp_Config.CfgBatteryPeriod * 60000);
#ifdef PWM_LED_PORT0
    osal_start_reload_timer(zclApp_TaskID, APP_LED_PWM_EVT, 15);
#endif
}

static void zclApp_RestoreAttributesFromNV(void) {
    uint8 status = osal_nv_item_init(NW_APP_CONFIG, sizeof(application_config_t), NULL);
    LREP("Restoring attributes from NV  status=%d \r\n", status);
    if (status == NV_ITEM_UNINIT) {
        uint8 writeStatus = osal_nv_write(NW_APP_CONFIG, 0, sizeof(application_config_t), &zclApp_Config);
        LREP("NV was empty, writing %d\r\n", writeStatus);
    }
    if (status == ZSUCCESS) {
        LREPMaster("Reading from NV\r\n");
        osal_nv_read(NW_APP_CONFIG, 0, sizeof(application_config_t), &zclApp_Config);
    }
}

static void zclApp_StateTextReport(void) {
    const uint8 NUM_ATTRIBUTES = 1;
    zclReportCmd_t *pReportCmd;
    pReportCmd = osal_mem_alloc(sizeof(zclReportCmd_t) + (NUM_ATTRIBUTES * sizeof(zclReport_t)));
    if (pReportCmd != NULL) {
        pReportCmd->numAttr = NUM_ATTRIBUTES;

        pReportCmd->attrList[0].attrID = ATTRID_IOV_BASIC_STATE_TEXT;
        pReportCmd->attrList[0].dataType = ZCL_CHAR_STR;
        pReportCmd->attrList[0].attrData = (void *)(&zclApp_StateText);

        afAddrType_t inderect_DstAddr = {.addrMode = (afAddrMode_t)AddrNotPresent, .endPoint = 0, .addr.shortAddr = 0};
        zcl_SendReportCmd(1, &inderect_DstAddr, MS_VALUE_BASIC, pReportCmd, ZCL_FRAME_CLIENT_SERVER_DIR, TRUE, bdb_getZCLFrameCounter());
    }
    osal_mem_free(pReportCmd);
}

#ifdef PWM_LED_PORT0
static void InitLedPWM(uint8 level){
//  P0DIR |= 0x08; // p0.3 output
  PERCFG &= ~0x40; //select of alternative 1 for timer 1
  P2DIR = (P2DIR & ~0xC0) | 0x80; // priority timer 1 channels 0–1
  P0SEL |= 0x08; // p0.3 periferal
  
  T1CC1H = 0x00;
  T1CC1L = level; //PWM Duty Cycle 
  
  T1CC0H = 0x00;
  T1CC0L = 0xff; //PWM signal period
  
  T1CCTL1 = 0x1c; //00: No capture
                  //1: Compare mode
                  //011: Set output on compare-up, clear on compare-down in up-and-down mode. Otherwise set output on compare, clear on 0.
  T1CTL = 0x02; //11: Up-and-down, repeatedly count from 0x0000 to T1CC0 and from T1CC0 down to 0x0000.
                //00: Tick frequency / 1
}
#endif

void DelayMs(unsigned int delaytime) {
  while(delaytime--)
  {
    uint16 microSecs = 1000;
    while(microSecs--)
    {
      asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop");
    }
  }
}
/****************************************************************************
****************************************************************************/
