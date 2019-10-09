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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY Kidx, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE

#include <modbus.h>
#include "modbus-binding.h"
#include <ctl-config.h>

typedef struct modbusRegistryS {
   char *uid;
   struct modbusRegistryS *next;
   ModbusFormatCbT *formats;
} modbusRegistryT;


// registry holds a linked list of core+pugins encoders
static modbusRegistryT *registryHead = NULL;

// add a new plugin encoder to the registry
void mbEncoderRegister (char *uid, ModbusFormatCbT *encoderCB) {
    modbusRegistryT *registryIdx, *registryEntry;

    // create holding hat for encoder/decoder CB
    registryEntry= (modbusRegistryT*) calloc (1, sizeof(modbusRegistryT));
    registryEntry->uid = uid;
    registryEntry->formats = encoderCB;


    // if not 1st encoder insert at the end of the chain
    if (!registryHead) {
        registryHead = registryEntry;
    } else {
        for (registryIdx= registryHead; registryIdx->next; registryIdx=registryIdx->next);
        registryIdx->next = registryEntry;
    }   
}

// find on format encoder/decoder within one plugin
ModbusFormatCbT *mvOneFormatFind (ModbusFormatCbT *format, const char *uid) {
    int idx;
    assert (uid);
    assert(format);

    for (idx=0; format[idx].uid; idx++) {
        if (!strcasecmp (format[idx].uid, uid)) break;
    }

    return (&format[idx]);
}

static void PluginParseURI (const char* uri, char **plugin, char **format) {
    char *chaine;
    int idx;
    static int prefixlen = sizeof(PLUGIN_ACTION_PREFIX)-1;

    // as today only TCP modbus is supported
    if (strncasecmp(uri, PLUGIN_ACTION_PREFIX, prefixlen)) goto NoPluginExit;

    // break URI in substring ignoring leading tcp:
    chaine= strdup(uri);   
    for (idx=prefixlen; idx < strlen(chaine); idx ++) {
        if (chaine[idx] == 0) break;
        if (chaine[idx] == '#') chaine[idx] = 0;
    }

    // extract IP addr and port
    *plugin = &chaine[prefixlen];
    *format= &chaine[idx];
    return;

NoPluginExit:
    return;
}


// search for a plugin encoders/decoders CB list
ModbusFormatCbT *mbEncoderFind (afb_api_t api, const char *uri) {
    char *pluginuid = NULL, *formatuid = NULL;
    modbusRegistryT *registryIdx;
    ModbusFormatCbT *format;

    PluginParseURI (uri, &pluginuid, &formatuid);

    // if no plugin search for core use directly format uri
    if (!pluginuid) {
        for (registryIdx= registryHead; registryIdx->uid && registryIdx->next; registryIdx=registryIdx->next);
        if (registryIdx->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: (Internal error) fail find core encoders");
            goto OnErrorExit;
        }

        format= mvOneFormatFind (registryIdx->formats, uri);
        if (!format || !format->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail find format='%s' within default core encoders", uri);
            goto OnErrorExit;
        } 

    }  else {
        for (registryIdx= registryHead; registryIdx->next; registryIdx=registryIdx->next) {
            if (registryIdx->uid && !strcasecmp (registryIdx->uid, uri)) break;
        }
        if (!registryIdx) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail find plugin='%s' format encoder ", formatuid);
            goto OnErrorExit;
        }

        format= mvOneFormatFind (registryIdx->formats, formatuid);
        if (!format || !format->uid) {
            AFB_API_ERROR(api, "mbEncoderFind: Fail find plugin='%s' format='%s' encoder", pluginuid, formatuid);
            goto OnErrorExit;
        }  
    }

    return (format);

OnErrorExit:
    return NULL;
}

static int mbDecodeFloatABCD (afb_api_t api, ModbusFormatCbT *format, uint16_t *source, uint index, json_object **responseJ) {
    float value= modbus_get_float_abcd (&source [index*format->nbreg]);
    *responseJ = json_object_new_double (value);
    return 0;
}

static int mbEncodeFloatABCD(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   float value;

   if (!json_object_is_type (sourceJ, json_type_double)) goto OnErrorExit;
   value= (float)json_object_get_double (sourceJ);
   modbus_set_float_abcd (value, *&response[index*format->nbreg]);
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "mbDecodeFloatABCD: [%s] not a double/float", json_object_get_string (sourceJ));
    return 1;
}

static int mbDecodeInt64 (afb_api_t api, ModbusFormatCbT *format, uint16_t *source, uint index, json_object **responseJ) {
    uint64_t value= MODBUS_GET_INT64_FROM_INT16(source, index*format->nbreg);
    *responseJ = json_object_new_int64 (value);
    return 0;
}

static int mbEncodeInt64(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   
   if (!json_object_is_type (sourceJ, json_type_int)) goto OnErrorExit;
   uint64_t val= (int64_t)json_object_get_int (sourceJ);
   MODBUS_SET_INT64_TO_INT16 (*response, index*format->nbreg, val);
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "mbDecodeInt64: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}


static int mbDecodeInt32 (afb_api_t api, ModbusFormatCbT *format, uint16_t *source, uint index, json_object **responseJ) {
    int32_t value= MODBUS_GET_INT32_FROM_INT16(source, index*format->nbreg);
    *responseJ = json_object_new_int (value);
    return 0;
}

static int mbEncodeInt32(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   
   if (!json_object_is_type (sourceJ, json_type_int)) goto OnErrorExit;
   int32_t value= (int32_t)json_object_get_int (sourceJ);
   MODBUS_SET_INT32_TO_INT16 (*response, index*format->nbreg, value);
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "mbDecodeInt16: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}


int mbDecodeInt16 (afb_api_t api, ModbusFormatCbT *format, uint16_t *source, uint index, json_object **responseJ) {

    *responseJ = json_object_new_int (source[index*format->nbreg]);
    return 0;
}

static int mbEncodeInt16(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   
   if (!json_object_is_type (sourceJ, json_type_int))  goto OnErrorExit;
   int16_t value = (int16_t)json_object_get_int (sourceJ);

   *response[index*format->nbreg] = value;
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "mbDecodeInt16: [%s] not an interger", json_object_get_string (sourceJ));
    return 1;
}

int mbDecodeBoolean (afb_api_t api, ModbusFormatCbT *format, uint16_t *source, uint index, json_object **responseJ) {

    *responseJ = json_object_new_boolean (source[index*format->nbreg]);
    return 0;
}

static int mbEncodeBoolean(afb_api_t api, ModbusFormatCbT *format, json_object *sourceJ, uint16_t **response, uint index) {
   
   if (!json_object_is_type (sourceJ, json_type_boolean))  goto OnErrorExit;
   int16_t value = (int16_t)json_object_get_boolean (sourceJ);

   *response[index*format->nbreg] = value;
   return 0;

OnErrorExit:
    AFB_API_ERROR(api, "mbEncodeBoolean: [%s] not a boolean", json_object_get_string (sourceJ));
    return 1;
}



static ModbusFormatCbT coreEncodersCB[] = {
  {.uid="BOOL"      , .nbreg=1, .decodeCB=mbDecodeBoolean  , .encodeCB=mbEncodeBoolean},
  {.uid="INT16"     , .nbreg=1, .decodeCB=mbDecodeInt16    , .encodeCB=mbEncodeInt16},
  {.uid="INT32"     , .nbreg=2, .decodeCB=mbDecodeInt32    , .encodeCB=mbEncodeInt32},
  {.uid="INT64"     , .nbreg=2, .decodeCB=mbDecodeInt64    , .encodeCB=mbEncodeInt64},
  {.uid="FLOAT_ABCD", .nbreg=4, .decodeCB=mbDecodeFloatABCD, .encodeCB=mbEncodeFloatABCD},

  {.uid= NULL} // must be null terminated
};

// register callback and use it to register core encoders
void mbRegisterCoreEncoders (void) {
  
  // Builtin Encoder don't have UID
  mbEncoderRegister (NULL, coreEncodersCB);

}
