/* 
 * Copyright (c) 2016 Nemik Consulting Inc
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
 
#include "mbed.h"
#include "ble/BLE.h"

#include <string>
#include <stdint.h>

using namespace std;

Ticker     ticker;
Serial pc(NC, NC);

DigitalIn enable (p26);
InterruptIn esp_reset (p29);

void advertisementCallback(const Gap::AdvertisementCallbackParams_t *params) 
{
    pc.printf("|%02x%02x%02x%02x%02x%02x|", 
        params->peerAddr[5], params->peerAddr[4], params->peerAddr[3], params->peerAddr[2], params->peerAddr[1], params->peerAddr[0]);
 
    pc.printf("%i|%i|%i|", 
        params->rssi, params->isScanResponse, params->type);
    
    //char data_tmp[(2*params->advertisingDataLen)];
    for (unsigned index = 0; index < params->advertisingDataLen; index++) 
    {
        //sprintf(data_tmp+2*index, "%02x", params->advertisingData[index]);
        pc.printf("%02x", params->advertisingData[index]);
    }
    pc.printf("|\n");
}

/**
 * This function is called when the ble initialization process has failed
 */
void onBleInitError(BLE &ble, ble_error_t error)
{
    /* Initialization error handling should go here */
}

/**
 * Callback triggered when the ble initialization process has finished
 */
void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE&        ble   = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        /* In case of error, forward the error handling to onBleInitError */
        onBleInitError(ble, error);
        return;
    }

    /* Ensure that it is the default instance of BLE */
    if(ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        return;
    }
 
    //ble.gap().setScanParams(100 /* scan interval */, 100 /* scan window */);
    ble.gap().setScanParams(50 /* scan interval */, 50 /* scan window */, 0 /* no timeout */, true /*active scanning*/);
    
    ble.gap().startScan(advertisementCallback);
}

bool serial_enabled = false;

void serial_init(void)
{
    // check if a default pulled-up pin from esp8266 is GPIO out down. 
    // if is, then this chip is enabled and should use serial
    
    if(serial_enabled && enable == 0)
    {
        return;
    }
    
    if(enable == 0 && !serial_enabled)
    {
        Serial pc(p9, p8); //prod
        pc.baud (115200);
        serial_enabled = true;
    }
    else
    {
        Serial pc(NC, NC);
        serial_enabled = false;
        DigitalIn d8(p8);
        DigitalIn d9(p9);
        d8.mode(PullNone);
        d9.mode(PullNone);
    }
}

int main(void)
{
    serial_init();
    
    ticker.attach(&serial_init, 1.0);
    
    BLE &ble = BLE::Instance();
    ble.init(bleInitComplete);

    while (true) 
    {
        ble.waitForEvent();
    }
}

