/*
 * Copyright (C) 2018 "IoT.bzh"
 * Author "Fulup Ar Foll" <fulup@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include "modbus-binding.h"
#include <ctl-plugin.h>

CTLP_CAPI_REGISTER("king_pigeon");

static int decodePigeonInfo (afb_api_t api, ModbusFormatCbT *format, uint16_t *data, uint index, json_object **responseJ) {

    *responseJ = json_object_new_object();
    json_object_object_add (*responseJ, "product", json_object_new_int(data[0]));
    json_object_object_add (*responseJ, "lot", json_object_new_int(data[1]));
    json_object_object_add (*responseJ, "serial", json_object_new_int(data[2]));
    json_object_object_add (*responseJ, "online", json_object_new_int(data[3]));
    json_object_object_add (*responseJ, "hardware", json_object_new_int(data[4]));
    json_object_object_add (*responseJ, "firmware", json_object_new_int(data[5]));

    return 0;
}


static int encodePigeonInfo(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   
   if (!json_object_is_type (sourceJ, json_type_int))  goto OnErrorExit;
   int16_t value = (int16_t)json_object_get_int (sourceJ);
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "encodePigeonInfo: [%s] not an interger", json_object_get_string(sourceJ));
    return 1;
}

// encode/decode callbacks
static ModbusFormatCbT pigeonEncoders[] = {
    {.uid="devinfo", .info="return KingPigeon Device Info as an array", .nbreg=6, .decodeCB=decodePigeonInfo, .encodeCB=encodePigeonInfo},
    {.uid=NULL} // must be NULL terminated
};

CTLP_ONLOAD(plugin, registryCB) {
    registerCbT callback = (registerCbT)registryCB;
    assert (callback);
    (*callback) (plugin->uid, pigeonEncoders);
    return 0;
}
