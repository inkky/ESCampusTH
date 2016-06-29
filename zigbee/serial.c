/*********************************************************************
 * INCLUDES
 */

#include <stdio.h>
#include <string.h>
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SerialApp.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"

#include "hal_drivers.h"
#include "hal_key.h"
#if defined ( LCD_SUPPORTED )
  #include "hal_lcd.h"
#endif
#include "hal_led.h"
#include "hal_uart.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

#if !defined( SERIAL_APP_PORT )
#define SERIAL_APP_PORT  0
#endif

#if !defined( SERIAL_APP_BAUD )
  //#define SERIAL_APP_BAUD  HAL_UART_BR_38400
  #define SERIAL_APP_BAUD  HAL_UART_BR_38400
#endif

// When the Rx buf space is less than this threshold, invoke the Rx callback.
#if !defined( SERIAL_APP_THRESH )
#define SERIAL_APP_THRESH  64
#endif

#if !defined( SERIAL_APP_RX_SZ )
#define SERIAL_APP_RX_SZ  128
#endif

#if !defined( SERIAL_APP_TX_SZ )
#define SERIAL_APP_TX_SZ  128
#endif

// Millisecs of idle time after a byte is received before invoking Rx callback.
#if !defined( SERIAL_APP_IDLE )
#define SERIAL_APP_IDLE  6
#endif

// Loopback Rx bytes to Tx for throughput testing.
#if !defined( SERIAL_APP_LOOPBACK )
#define SERIAL_APP_LOOPBACK  FALSE
#endif

// This is the max byte count per OTA message.
#if !defined( SERIAL_APP_TX_MAX )
#define SERIAL_APP_TX_MAX  80
#endif

#define SERIAL_APP_RSP_CNT  4

// This list should be filled with Application specific Cluster IDs.
const cId_t SerialApp_ClusterList[SERIALAPP_MAX_CLUSTERS] =
{
  SERIALAPP_CLUSTERID1,
  SERIALAPP_CLUSTERID2,
  SERIALAPP_CONNECTREQ_CLUSTER,            
  SERIALAPP_CONNECTRSP_CLUSTER             
};

const SimpleDescriptionFormat_t SerialApp_SimpleDesc =
{
#if ZDO_COORDINATOR
  SERIALAPP_COORPOINT,
#else
  SERIALAPP_ENDPOINT,              //  int   Endpoint;
#endif
  SERIALAPP_PROFID,                //  uint16 AppProfId[2];
  SERIALAPP_DEVICEID,              //  uint16 AppDeviceId[2];
  SERIALAPP_DEVICE_VERSION,        //  int   AppDevVer:4;
  SERIALAPP_FLAGS,                 //  int   AppFlags:4;
  SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumInClusters;
  (cId_t *)SerialApp_ClusterList,  //  byte *pAppInClusterList;
  SERIALAPP_MAX_CLUSTERS,          //  byte  AppNumOutClusters;
  (cId_t *)SerialApp_ClusterList   //  byte *pAppOutClusterList;
};

endPointDesc_t SerialApp_epDesc =
{
#if ZDO_COORDINATOR
  SERIALAPP_COORPOINT,
#else
  SERIALAPP_ENDPOINT,              //  int   Endpoint;
#endif
 &SerialApp_TaskID,
  (SimpleDescriptionFormat_t *)&SerialApp_SimpleDesc,
  noLatencyReqs
};

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
devStates_t SampleApp_NwkState;   
uint8 SerialApp_TaskID;           // Task ID for internal task/event processing.

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static uint8 SerialApp_MsgID;

static afAddrType_t SerialApp_TxAddr;
static uint8 SerialApp_TxSeq;
static uint8 SerialApp_TxBuf[SERIAL_APP_TX_MAX+1];
static uint8 SerialApp_TxLen;

static afAddrType_t SerialApp_RxAddr;
static uint8 SerialApp_RxSeq;
static uint8 SerialApp_RspBuf[SERIAL_APP_RSP_CNT];

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
static void SerialApp_Send(void);
static void SerialApp_Resp(void);
static void SerialApp_CallBack(uint8 port, uint8 event); 
static void SerialApp_DeviceConnect(void);              
static void SerialApp_DeviceConnectRsp(uint8*);         
static void SerialApp_ConnectReqProcess(uint8*, uint8);           

struct circleBuffer{
  uint8 buffer[10][40];
  uint8 eleSize[10];
  uint8 capacity;
  uint8 size;
  uint8 head;
  uint8 tail;
};

struct circleBuffer cB;

static int circleBufferInit(void);
static int circleBufferHasElement(void);
static uint8* popElement(uint8* eleSize);
static int pushElement(uint8* eleAddr, uint8 eleSize);
static void send_uart(void);

int circleBufferInit(void){
  cB.capacity=10;
  cB.size=0;
  cB.head=0;
  cB.tail=0;
  return 0;
}

int circleBufferHasElement(void){
  if(cB.size>0){
    return 1;
  }
  else{
    return 0;
  }
}

uint8* popElement(uint8* eleSize){
  uint8* returnAddress;
  if(cB.size<=0){
    *eleSize=0;
    return NULL;
  }
  else{
    *eleSize=cB.eleSize[cB.head];
    returnAddress=(uint8*)&cB.buffer[cB.head];
    cB.head++;
    if(cB.head==cB.capacity){
      cB.head=0; 
    }
    cB.size--;
    return returnAddress;
  }
}

int pushElement(uint8* eleAddr, uint8 eleSize){
  if(cB.size==cB.capacity){
    return -1; 
  }
  else{
    osal_memcpy(cB.buffer[cB.tail],eleAddr,eleSize);
    cB.eleSize[cB.tail]=eleSize;
    cB.tail++;
    if(cB.tail==cB.capacity){
      cB.tail=0; 
    }
    cB.size++;
    osal_set_event( SerialApp_TaskID, SEND_UART );
    return 0;
  }
}

void send_uart(void){
  uint8* buffer;
  uint8 size;
  if(circleBufferHasElement()==1){
    buffer=popElement(&size);
    HalUARTWrite( SERIAL_APP_PORT, buffer, size);
  }
  else{
    return;
  }
}

/*********************************************************************
 * @fn      SerialApp_Init
 *
 * @brief   This is called during OSAL tasks' initialization.
 *
 * @param   task_id - the Task ID assigned by OSAL.
 *
 * @return  none
 */
void SerialApp_Init( uint8 task_id )
{
  halUARTCfg_t uartConfig;

  SerialApp_TaskID = task_id;
  SerialApp_RxSeq = 0xC3;
  SampleApp_NwkState = DEV_INIT;       
  
  afRegister( (endPointDesc_t *)&SerialApp_epDesc );

  RegisterForKeys( task_id );

  uartConfig.configured           = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.baudRate             = SERIAL_APP_BAUD;
  uartConfig.flowControl          = FALSE;
  uartConfig.flowControlThreshold = SERIAL_APP_THRESH; // 2x30 don't care - see uart driver.
  uartConfig.rx.maxBufSize        = SERIAL_APP_RX_SZ;  // 2x30 don't care - see uart driver.
  uartConfig.tx.maxBufSize        = SERIAL_APP_TX_SZ;  // 2x30 don't care - see uart driver.
  uartConfig.idleTimeout          = SERIAL_APP_IDLE;   // 2x30 don't care - see uart driver.
  uartConfig.intEnable            = TRUE;              // 2x30 don't care - see uart driver.
  uartConfig.callBackFunc         = SerialApp_CallBack;
  HalUARTOpen (SERIAL_APP_PORT, &uartConfig);

#if defined ( LCD_SUPPORTED )
  HalLcdWriteString( "SerialApp", HAL_LCD_LINE_2 );
#endif
  
  ZDO_RegisterForZDOMsg( SerialApp_TaskID, End_Device_Bind_rsp );
  ZDO_RegisterForZDOMsg( SerialApp_TaskID, Match_Desc_rsp );
  circleBufferInit();
}

/*********************************************************************
 * @fn      SerialApp_ProcessEvent
 *
 * @brief   Generic Application Task event processor.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events   - Bit map of events to process.
 *
 * @return  Event flags of all unprocessed events.
 */
UINT16 SerialApp_ProcessEvent( uint8 task_id, UINT16 events )
{
  (void)task_id;  // Intentionally unreferenced parameter
  
  if ( events & SYS_EVENT_MSG )
  {
    afIncomingMSGPacket_t *MSGpkt;

    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( SerialApp_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
      case AF_INCOMING_MSG_CMD:
        SerialApp_ProcessMSGCmd( MSGpkt );
        break;
        
      case ZDO_STATE_CHANGE:
        SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
        if ( (SampleApp_NwkState == DEV_ZB_COORD)
            || (SampleApp_NwkState == DEV_ROUTER)
            || (SampleApp_NwkState == DEV_END_DEVICE) )
        {
            // Start sending the periodic message in a regular interval.
            HalLedSet(HAL_LED_1, HAL_LED_MODE_ON);
            
            if(SampleApp_NwkState != DEV_ZB_COORD)
              SerialApp_DeviceConnect();              
        }
        else
        {
          // Device is no longer in the network
        }
        break;

      default:
        break;
      }

      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    return ( events ^ SYS_EVENT_MSG );
  }

  if ( events & SERIALAPP_SEND_EVT )
  {
    SerialApp_Send();
    return ( events ^ SERIALAPP_SEND_EVT );
  }

  if ( events & SERIALAPP_RESP_EVT )
  {
    SerialApp_Resp();
    return ( events ^ SERIALAPP_RESP_EVT );
  }
  if (events & SEND_UART){
    send_uart();
    return ( events );
  }
  return ( 0 );  // Discard unknown events.
}

/*********************************************************************
 * @fn      SerialApp_ProcessMSGCmd
 *
 * @brief   Data message processor callback. This function processes
 *          any incoming data - probably from other devices. Based
 *          on the cluster ID, perform the intended action.
 *
 * @param   pkt - pointer to the incoming message packet
 *
 * @return  TRUE if the 'pkt' parameter is being used and will be freed later,
 *          FALSE otherwise.
 */
void SerialApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
  char buf[30]={0};
  uint8 stat;
  uint8 seqnb;
  uint8 delay;

  switch ( pkt->clusterId )
  {
  case SERIALAPP_CLUSTERID1:
    osal_memcpy(&SerialApp_RxAddr, &(pkt->srcAddr), sizeof( afAddrType_t ));

    seqnb = pkt->cmd.Data[0];
    pushElement(pkt->cmd.Data+1, (pkt->cmd.DataLength-1));
    break;

  case SERIALAPP_CLUSTERID2:
    if ((pkt->cmd.Data[1] == SerialApp_TxSeq) &&
       ((pkt->cmd.Data[0] == OTA_SUCCESS) || (pkt->cmd.Data[0] == OTA_DUP_MSG)))
    {
      SerialApp_TxLen = 0;
      osal_stop_timerEx(SerialApp_TaskID, SERIALAPP_SEND_EVT);
    }
    else
    {
      delay = BUILD_UINT16( pkt->cmd.Data[2], pkt->cmd.Data[3] );
      osal_start_timerEx( SerialApp_TaskID, SERIALAPP_SEND_EVT, delay );
    }
    HalLedSet(HAL_LED_2, HAL_LED_MODE_ON);
    break;

    case SERIALAPP_CONNECTREQ_CLUSTER:
      SerialApp_ConnectReqProcess((uint8*)pkt->cmd.Data, pkt->srcAddr.endPoint);
      
    case SERIALAPP_CONNECTRSP_CLUSTER:
      SerialApp_DeviceConnectRsp((uint8*)pkt->cmd.Data);
      
    default:
      break;
  }
}

/*********************************************************************
 * @fn      SerialApp_Send
 *
 * @brief   Send data OTA.
 *
 * @param   none
 *
 * @return  none
 */
static void SerialApp_Send(void)
{
#if SERIAL_APP_LOOPBACK
    if (SerialApp_TxLen < SERIAL_APP_TX_MAX)
    {
        SerialApp_TxLen += HalUARTRead(SERIAL_APP_PORT, SerialApp_TxBuf+SerialApp_TxLen+1,
                                                      SERIAL_APP_TX_MAX-SerialApp_TxLen);
    }
  
    if (SerialApp_TxLen)
    {
      (void)SerialApp_TxAddr;
      if (HalUARTWrite(SERIAL_APP_PORT, SerialApp_TxBuf+1, SerialApp_TxLen))
      {
        SerialApp_TxLen = 0;
      }
      else
      {
        osal_set_event(SerialApp_TaskID, SERIALAPP_SEND_EVT);
      }
    }
#else
    if (SerialApp_TxLen = HalUARTRead(SERIAL_APP_PORT, SerialApp_TxBuf+1, SERIAL_APP_TX_MAX))
    {
      SerialApp_TxBuf[0] = ++SerialApp_TxSeq;
    }
    if (SerialApp_TxLen)
    {
      if (afStatus_SUCCESS != AF_DataRequest(&SerialApp_TxAddr,
                                             (endPointDesc_t *)&SerialApp_epDesc,
                                              SERIALAPP_CLUSTERID1,
                                              SerialApp_TxLen+1, SerialApp_TxBuf,
                                              &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS))
      {
        osal_set_event(SerialApp_TaskID, SERIALAPP_SEND_EVT);
      }
    }
    HalLedSet(HAL_LED_2, HAL_LED_MODE_OFF);
#endif
}

/*********************************************************************
 * @fn      SerialApp_Resp
 *
 * @brief   Send data OTA.
 *
 * @param   none
 *
 * @return  none
 */
static void SerialApp_Resp(void)
{
  if (afStatus_SUCCESS != AF_DataRequest(&SerialApp_RxAddr,
                                         (endPointDesc_t *)&SerialApp_epDesc,
                                          SERIALAPP_CLUSTERID2,
                                          SERIAL_APP_RSP_CNT, SerialApp_RspBuf,
                                         &SerialApp_MsgID, 0, AF_DEFAULT_RADIUS))
  {
    osal_set_event(SerialApp_TaskID, SERIALAPP_RESP_EVT);
  }
}

/*********************************************************************
 * @fn      SerialApp_CallBack
 *
 * @brief   Send data OTA.
 *
 * @param   port - UART port.
 * @param   event - the UART port event flag.
 *
 * @return  none
 */
static void SerialApp_CallBack(uint8 port, uint8 event)
{
  (void)port;

  if ((event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT)) &&
#if SERIAL_APP_LOOPBACK
      (SerialApp_TxLen < SERIAL_APP_TX_MAX))
#else
      1)
#endif
  {
    SerialApp_Send();
  }
}

/*********************************************************************
*********************************************************************/
void  SerialApp_DeviceConnect()              
{
#if ZDO_COORDINATOR
  
#else
  
  uint16 nwkAddr;
  uint16 parentNwkAddr;
  char buff[30] = {0};
  
  HalLedBlink( HAL_LED_2, 3, 50, (1000 / 4) );
  
  nwkAddr = NLME_GetShortAddr();
  parentNwkAddr = NLME_GetCoordShortAddr();
  sprintf(buff, "p:%d   s:%d\r\n", parentNwkAddr, nwkAddr);
  HalUARTWrite ( 0, (uint8*)buff, strlen(buff));
  
  SerialApp_TxAddr.addrMode = (afAddrMode_t)Addr16Bit;
  SerialApp_TxAddr.endPoint = SERIALAPP_COORPOINT;
  SerialApp_TxAddr.addr.shortAddr = parentNwkAddr;
  
  buff[0] = HI_UINT16( nwkAddr );
  buff[1] = LO_UINT16( nwkAddr );
  
  if ( AF_DataRequest( &SerialApp_TxAddr, &SerialApp_epDesc,
                       SERIALAPP_CONNECTREQ_CLUSTER,
                       2,
                       (uint8*)buff,
                       &SerialApp_MsgID, 
                       0, 
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
  
#endif    //ZDO_COORDINATOR
}

void SerialApp_DeviceConnectRsp(uint8 *buf)
{
#if ZDO_COORDINATOR
  
#else
  SerialApp_TxAddr.addrMode = (afAddrMode_t)Addr16Bit;
  SerialApp_TxAddr.endPoint = SERIALAPP_COORPOINT;
  SerialApp_TxAddr.addr.shortAddr = BUILD_UINT16(buf[1], buf[0]);
  
  HalLedSet(HAL_LED_2, HAL_LED_MODE_ON);
  HalUARTWrite ( 0, "connect success, endpoint\n", 23);
#endif
}

void SerialApp_ConnectReqProcess(uint8 *buf, uint8 endPoint)
{
  uint16 nwkAddr;
  char buff[30] = {0};
  
  SerialApp_TxAddr.addrMode = (afAddrMode_t)Addr16Bit;
  SerialApp_TxAddr.endPoint = endPoint;
  SerialApp_TxAddr.addr.shortAddr = BUILD_UINT16(buf[1], buf[0]);
  nwkAddr = NLME_GetShortAddr();
  
  sprintf(buff, "s:%d  c:%d  endpoint: %d\r\n", nwkAddr, SerialApp_TxAddr.addr.shortAddr, endPoint);
  HalUARTWrite ( 0, (uint8*)buff, strlen(buff));
  
  buff[0] = HI_UINT16( nwkAddr );
  buff[1] = LO_UINT16( nwkAddr );
  
  if ( AF_DataRequest( &SerialApp_TxAddr, &SerialApp_epDesc,
                       SERIALAPP_CONNECTRSP_CLUSTER,
                       2,
                       (uint8*)buff,
                       &SerialApp_MsgID, 
                       0, 
                       AF_DEFAULT_RADIUS ) == afStatus_SUCCESS )
  {
  }
  else
  {
    // Error occurred in request to send.
  }
  SerialApp_TxAddr.addrMode = afAddrBroadcast;
  SerialApp_TxAddr.addr.shortAddr = 0xFFFF;
  HalLedSet(HAL_LED_2, HAL_LED_MODE_ON);
  HalUARTWrite ( 0, "connect success, coor\n", 23);
}