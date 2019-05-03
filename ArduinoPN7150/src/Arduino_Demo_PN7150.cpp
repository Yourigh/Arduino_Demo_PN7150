/*
 * TagDetect.ino
 *
 * Copyright (c) Thomas Buhot. All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/******************************************************************************
 *           Purpose of this sketch
 *
 * The purpose of this sketch is to provide an example of NFC tag detection
 * by using a simple but efficient NFC stack.
 * 
 * The NFC stack implements a tag API which drives an NFC controller
 * through the NCI (NFC Controller Interface) as defined by the NFC Forum.
 * The NFC stack
 * consists in:
 * - NfcTagsi   : high level tag API for detection, deactivation
 * - NfcNci     : NCI implementation, hardware independent
 * - NfcHw      : NFC hardaware interface
 *
 * The NFC stack configures the NFC Controller to detect tag of types 1,
 * 2 or 3 as per the NFC Forum specifications.
 *
 * This sketch:
 * 1. initializes the NFC controller
 * 2. configures the RF discovery parameters
 * 3. prints NFCID of the detected tag
 * 4. restart the tag detection back to step 2
 *
 * The HW configuration used to test that sketch is: Intel Arduino 101
 * with NXP PN7120 SBC kit.
 *****************************************************************************/
#include <Arduino.h>
#include "Nfc.h" //lib id 2888
#include <HardwareSerial.h>

/**********************************************
 *      NFC controller hardware configuration
 *
 * - NXP PN7120 NFC chipset
 * - Connected with I2C + IRQ + RESET
 *********************************************/

#define PN7120_IRQ          2  // pin 2 configured as input for IRQ
#define PN7120_RESET        4  // pin 4 configured as input for VEN (reset)
#define PN7120_I2C_ADDRESS  0x7C // 0x28

/**********************************************
 *          Sketch application class
 *
 * Implements the state machine and event
 * handler of the sketch. Besides, interfaces
 * with the NfcTags class which offers the
 * NFC API for tags detection and handling.
 * Each callback is called upon NFC controller
 * response or event received from Tags class.
 * The callbacks are used to:
 * - check the reponse / event status and data,
 * - change the current state accordingly.
 **********************************************/

// state definition
enum
{
    STATE_RESET = 0,
    STATE_RESET_RESPONSE,
    STATE_DISCOVER,
    STATE_DISCOVER_RESPONSE,
    STATE_DISCOVERING,
    STATE_DEACTIVATE,
    STATE_DEACTIVATE_RESPONSE,
    STATE_ERROR,
    STATE_END
};

const char *tagDetectStateToStr[] = {
    "STATE_RESET",
    "STATE_RESET_RESPONSE",
    "STATE_DISCOVER",
    "STATE_DISCOVER_RESPONSE",
    "STATE_DISCOVERING",
    "STATE_DEACTIVATE",
    "STATE_DEACTIVATE_RESPONSE",
    "STATE_ERROR",
    "STATE_END"
};

// Sketch application object to interface with NfcTags API
class NfcApps : public NfcTagsCb
{
    public:
        NfcApps(NfcLog& log, NfcTags& tags) : _state(STATE_RESET), _log(log), _tags(tags) {;}
        void init(void) {;}
        void handleEvent(void);
        void cbReset(uint8_t status, uint16_t id, void *data);
        void cbDiscover(uint8_t status, uint16_t id, void *data);
        void cbDiscoverNtf(uint8_t status, uint16_t id, void *data);
        void cbDeactivate(uint8_t status, uint16_t id, void *data);
        void cbDump(uint8_t status, uint16_t id, void *data);

    private:
        uint8_t _state;
        NfcLog& _log;
        NfcTags& _tags;
};

// State machine event handler
void NfcApps::handleEvent(void)
{
    uint8_t status = TAGS_STATUS_FAILED;

    _log.d("TagDetect: %s state = %s\n", __func__, tagDetectStateToStr[_state]);

    switch(_state) {
        case STATE_RESET:
            // reset NFC stack and hw
            status = _tags.cmdReset();
            _state = STATE_RESET_RESPONSE;
            break;
        case STATE_RESET_RESPONSE:
            // wait for reset
            status = TAGS_STATUS_OK;
            break;
        case STATE_DISCOVER:
            // find tags
            status = _tags.cmdDiscover();
            _state = STATE_DISCOVER_RESPONSE;
            break;
        case STATE_DISCOVER_RESPONSE:
            // wait for find tags response
            status = TAGS_STATUS_OK;
            break;
        case STATE_DISCOVERING:
            // waiting for a tag to be detected
            status = TAGS_STATUS_OK;
            break;
        case STATE_DEACTIVATE:
            // disconnect from tag and restart discovery loop
            status = _tags.cmdDeactivate();
            _state = STATE_DEACTIVATE_RESPONSE;
            break;
        case STATE_DEACTIVATE_RESPONSE:
            // wait for tag deactivation
            status = TAGS_STATUS_OK;
            break;
        case STATE_ERROR:
        case STATE_END:
        default:
            break;
    }

    // handle error
    if (status != TAGS_STATUS_OK) {
        _log.e("TagDetect error: %s status = %d state = %d\n", __func__, status, _state);
        _state = STATE_ERROR;
    }
}

// Hardware reset callback
void NfcApps::cbReset(uint8_t status, uint16_t id, void *data)
{
    _log.d("TagDetect: %s status = %d id = %d\n", __func__, status, id);

    if (status != TAGS_STATUS_OK || id != TAGS_ID_RESET) {
        _state = STATE_ERROR;
    }
    else {
        _log.i("TagDetect: NFC stack and HW reseted\n");
        _state = STATE_DISCOVER;
    }
}

// Discover target callback
void NfcApps::cbDiscover(uint8_t status, uint16_t id, void *data)
{
    _log.d("TagDetect: %s status = %d id = %d\n", __func__, status, id);

    if (status != TAGS_STATUS_OK || id != TAGS_ID_DISCOVER) {
        _state = STATE_ERROR;
    }
    else {
        _log.i("TagDetect: NFC stack discovering tags...\n");
        _state = STATE_DISCOVERING;
    }
}

// Discover notification on tag detected callback
void NfcApps::cbDiscoverNtf(uint8_t status, uint16_t id, void *data)
{
    NfcTagsIntf *pTag;
    uint8_t len, type;
    uint8_t *buf;

    _log.d("TagDetect: %s status = %d id = %d\n", __func__, status, id);

    if (status != TAGS_STATUS_OK || id != TAGS_ID_DISCOVER_ACTIVATED) {
        _state = STATE_ERROR;
    }
    else {
        pTag = _tags.getInterface();
        if (pTag != NULL) {
            type = pTag->getType();
            _log.i("TagDetect: tag type %d detected\n", type);
            len = pTag->getNfcidLen();
            buf = pTag->getNfcidBuf();
            _log.bi("TagDetect: tag NFCID = ", buf, len);
        }
        else {
            _log.i("TagDetect: unknown tag type detected\n");
        }
    }
    _state = STATE_DEACTIVATE;
}

// Tag dump callback, unused in that sketch which ony detects tags
void NfcApps::cbDump(uint8_t status, uint16_t id, void *data)
{
}

// Tag deactivation callback
void NfcApps::cbDeactivate(uint8_t status, uint16_t id, void *data)
{
    _log.d("TagDetect: %s status = %d id = %d\n", __func__, status, id);

    if (status != TAGS_STATUS_OK || id != TAGS_ID_DEACTIVATE) {
        _state = STATE_ERROR;
    }
    else {
        _state = STATE_DISCOVERING;
    }
}

/**********************************************
 *           Sketch runtime
 *
 * _log: logger (serial)
 * _pn7120: NXP PN7120 NFC chipset
 * _nci: NFC Connection Interface (NFC Forum)
 * _tags: tag API wrapper to drive NCI chipset
 * _app: sketch implementation
 **********************************************/
#define NFC_LOG_LEVEL_INFO 0
NfcLog _log(NFC_LOG_LEVEL_INFO);
NfcHw_pn7120 _pn7120(_log, PN7120_IRQ, PN7120_RESET, PN7120_I2C_ADDRESS);
NfcNci _nci(_log, _pn7120);
NfcTags _tags(_log, _nci);
NfcApps _app(_log, _tags);

// the setup function runs once when you press reset or power the board
void setup(void)
{
    // add a delay for the serial bus to be mounted
    delay(2000);

    // init all layers from bottom to top
    // logger, hw, nci, tags, and state machine
    _log.init(115200);
    _pn7120.init();
    _nci.init(&_tags);
    _tags.init(&_app);
    _app.init();

    Serial.print("AAAAAAA\n");
}

// the loop function runs over and over again forever
void loop(void)
{
    static int a=0;
    a++;
    Serial.print(a);
    Serial.print(" AAAAAAb\n");
    delay(20);
    // handle sketch events (state machine based)
    _app.handleEvent();

    // handle tags class events (state machine based)
    _tags.handleEvent();

    // handle NCI events (state machine based),
    // it may block waiting for NFC controller
    // response or event
    _nci.handleEvent();
}
