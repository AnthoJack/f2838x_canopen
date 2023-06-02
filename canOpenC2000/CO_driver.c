/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "301/CO_driver.h"
#include "driverlib_cm.h"
#include "cm.h"

CO_CANmodule_t *canModForIsr;

void CO_CANinterrupt(CO_CANmodule_t *CANmodule);

// Driver ISR prototype requires access to the CO_CANmodule_t object so 
// this intermediate ISR is defined to pass the object to the driver ISR
void /*__interrupt*/ canIsr(void)
{
    CO_CANinterrupt(canModForIsr);
}


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANptr){
    /* Put CAN module in configuration mode */
    CAN_disableController(CANptr);
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    /* Put CAN module in normal mode */
    CAN_startModule(CANmodule->CANptr);

    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANptr,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    canModForIsr = CANmodule;

    /* Configure object variables */
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize + txSize <= 32U) ? true : false;/* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for(i=0U; i<txSize; i++){
        txArray[i].bufferFull = false;
    }


    /* Configure CAN module registers */
    CAN_initModule(CANptr);

    /* Configure CAN timing */
    CAN_setBitRate(CANptr, CM_CLK_FREQ, CANbitRate, 20);


    /* Configure CAN module hardware filters */
    if(CANmodule->useCANrxFilters){
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
        CAN_disableAllMessageObjects(CANmodule->CANptr);
    }
    else{
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
    }


    /* configure CAN interrupt registers */
    CAN_enableInterrupt(CANB_BASE, CAN_INT_IE0 | CAN_INT_ERROR |
                        CAN_INT_STATUS);
    // register ISR, enable interrupt
    Interrupt_registerHandler(INT_CANA0, canIsr);
    Interrupt_enable(INT_CANA0);
    CAN_enableGlobalInterrupt(CANmodule->CANptr, CAN_GLOBAL_INT_CANINT0);

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule) {
    if (CANmodule != NULL) {
        /* turn off the module */
        CAN_disableController(CANmodule->CANptr);
    }
}


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*CANrx_callback)(void *object, void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if((CANmodule!=NULL) && (object!=NULL) && (CANrx_callback!=NULL) && (index < CANmodule->rxSize)){
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
        buffer->ident = ident & 0x07FFU;
        if(rtr){
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        /* Set CAN hardware module filter and mask. */
        if(CANmodule->useCANrxFilters){
            CAN_setupMessageObject(CANmodule->CANptr,
                index,
                ident,
                CAN_MSG_FRAME_STD,
                rtr ? CAN_MSG_OBJ_TYPE_RXTX_REMOTE : CAN_MSG_OBJ_TYPE_RX,
                0,
                CAN_MSG_OBJ_RX_INT_ENABLE,
                8);
        }
    }
    else{
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer.
         * Microcontroller specific. */
        buffer->ident = ((uint32_t)ident & 0x07FFU)
                      | ((uint32_t)(((uint32_t)noOfBytes & 0xFU) << 12U))
                      | ((uint32_t)(rtr ? 0x8000U : 0U));

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;

        CAN_setupMessageObject(CANmodule->CANptr,
            index + CANmodule->rxSize,
            ident,
            CAN_MSG_FRAME_STD,
            rtr ? CAN_MSG_OBJ_TYPE_TX_REMOTE : CAN_MSG_OBJ_TYPE_TX,
            0,
            CAN_MSG_OBJ_TX_INT_ENABLE,
            noOfBytes);
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;

    CO_LOCK_CAN_SEND(CANmodule);
    /* if CAN TX buffer is free, copy message to it */
    if(!(CAN_getTxRequests(CANmodule->CANptr) & (1 << buffer->msgObj))){
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        /* copy message and txRequest */
        CAN_sendMessage(CANmodule->CANptr, buffer->msgObj, buffer->DLC, buffer->data);
    }
    /* if no buffer is free, message will be sent by interrupt */
    else{
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }
        return CO_ERROR_TX_OVERFLOW;
    }
    buffer->bufferFull = true;
    CO_UNLOCK_CAN_SEND(CANmodule);

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
    uint32_t tpdoDeleted = 0U;

    CO_LOCK_CAN_SEND(CANmodule);

    /* Delete pending synchronous TPDOs in TX msg objects */
    uint16_t i;
    CO_CANtx_t *buffer = &CANmodule->txArray[0];
    for(i = CANmodule->txSize; i > 0U; i--){
        if(buffer->bufferFull && buffer->syncFlag){
            buffer->bufferFull = false;
            tpdoDeleted++;
            //WARNING: It is unclear if the next function has the desired effect
            // The stack seems to require the message transmission to be cancelled
            // but the message object still needs to function for next message and
            // the driverlib documentation is a bit shady about this function's
            // behaviour. More testing is required if this function is required
            CAN_clearMessage(CANmodule->CANptr, buffer->msgObj);
        }
        buffer++;
    }
    CO_UNLOCK_CAN_SEND(CANmodule);


    if(tpdoDeleted != 0U){
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}


/******************************************************************************/
/* Get error counters from the module. If necessary, function may use
    * different way to determine errors. */
static uint16_t rxErrors=0, txErrors=0, overflow=0;

void CO_CANmodule_process(CO_CANmodule_t *CANmodule) {
    uint32_t err;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

    if (CANmodule->errOld != err) {
        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (txErrors >= 256U) {
            /* bus off */
            status |= CO_CAN_ERRTX_BUS_OFF;
        }
        else {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF |
                                CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE |
                                CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE);

            /* rx bus warning or passive */
            if (rxErrors >= 128) {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE;
            } else if (rxErrors >= 96) {
                status |= CO_CAN_ERRRX_WARNING;
            }

            /* tx bus warning or passive */
            if (txErrors >= 128) {
                status |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE;
            } else if (rxErrors >= 96) {
                status |= CO_CAN_ERRTX_WARNING;
            }

            /* if not tx passive clear also overflow */
            if ((status & CO_CAN_ERRTX_PASSIVE) == 0) {
                status &= 0xFFFF ^ CO_CAN_ERRTX_OVERFLOW;
            }
        }

        if (overflow != 0) {
            /* CAN RX bus overflow */
            status |= CO_CAN_ERRRX_OVERFLOW;
        }

        CANmodule->CANerrorStatus = status;
    }
}


/******************************************************************************/


void CO_CANinterrupt(CO_CANmodule_t *CANmodule){

    uint32_t cause = CAN_getInterruptCause(CANmodule->CANptr);
    // Assuming that:
    //      -No Interrupt is pending on INT1
    //      -Interrupt IS pending on INT0
    /* receive interrupt */
    if(cause < CANmodule->rxSize){
        CO_CANrxMsg_t rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;             /* index of received message */
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */

        index = cause;
        buffer = &CANmodule->rxArray[index];
        // Functions exist to recover the data from a message object but they 
        // don't allow the DLC to be recovered too so a more "manual" data 
        // recovery is used
        CAN_transferMessage(CANmodule->CANptr, 1, cause, false, false);
        rcvMsg.ident = HWREG(CANmodule->CANptr + CAN_O_IF1ARB);
        rcvMsg.DLC = HWREG(CANmodule->CANptr + CAN_O_IF1MCTL);
        rcvMsg.ident >>= 18;
        rcvMsg.ident &= 0x7ff;
        rcvMsg.DLC &= 0xf;


        /* Call specific function, which will process the message */
        if((buffer != NULL) && (buffer->CANrx_callback != NULL)){
            buffer->CANrx_callback(buffer->object, (void*) &rcvMsg);
        }

        /* Clear interrupt flag */
        CAN_clearInterruptStatus(CANmodule->CANptr, cause);        
    }

    /* transmit interrupt */
    else if(cause < 32){
        /* Clear interrupt flag */

        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        CANmodule->txArray[cause - CANmodule->rxSize].bufferFull = false;
    }
    else{
        /* some other interrupt reason */
    }

    CAN_clearGlobalInterruptStatus(CANmodule->CANptr, CAN_GLOBAL_INT_CANINT0);
}
