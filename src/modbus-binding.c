/*
 * Copyright (C) 2015-2020 IoT.bzh Company
 * Author "Fulup Ar Foll"
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
*/

// Le contexte de sensor loader au moment de l'API n'est retrouvé avec le request ****

#define _GNU_SOURCE


#include "modbus-binding.h"

#include <ctl-config.h>
#include <filescan-utils.h>

#ifndef MB_DEFAULT_POLLING_FEQ
#define MB_DEFAULT_POLLING_FEQ 10
#endif

static int ModbusConfig(afb_api_t api, CtlSectionT *section, json_object *rtusJ);

// Config Section definition (note: controls section index should match handle
// retrieval in HalConfigExec)
static CtlSectionT ctrlSections[] = {
    { .key = "plugins",.loadCB = PluginConfig, .handle= mbEncoderRegister},
    { .key = "onload", .loadCB = OnloadConfig },
    { .key = "modbus", .loadCB = ModbusConfig },
    { .key = NULL }
};

static void PingTest (afb_req_t request) {
    static int count=0;
    char response[32];
    json_object *queryJ =  afb_req_json(request);

    snprintf (response, sizeof(response), "Pong=%d", count++);
    AFB_API_NOTICE (request->api, "Modbus:ping count=%d query=%s", count, json_object_get_string(queryJ));
    afb_req_success_f(request,json_object_new_string(response), NULL);

    return;
}

static void InfoRtu (afb_req_t request) {
    json_object *elemJ;
    int err, idx, status;
    int verbose=0;
    ModbusRtuT *rtus = (ModbusRtuT*) afb_req_get_vcbdata(request);
    json_object *queryJ =  afb_req_json(request);
    json_object *responseJ= json_object_new_array();

    if (json_object_is_type(queryJ, json_type_object)) {
        err= wrap_json_unpack(queryJ, "{s?i !}", "verbose", &verbose);
        if (err) {
            afb_req_fail_f (request, "ModbusRtuAdmin", "ListRtu: invalid 'json query' query=%s", json_object_get_string(queryJ));
            goto OnErrorExit;
        }
    }

    // loop on every defined RTU
    if (verbose) {
        for (idx=0; rtus[idx].uid; idx++) {
        switch (verbose) {
            case 1:
            default:
                wrap_json_pack (&elemJ, "{ss ss ss}", "uid", rtus[idx].uid, "uri",rtus[idx].uri, "info", rtus[idx].info);
                break;
            case 2:
                status= ModbusRtuIsConnected (request->api, &rtus[idx]);
                if (status <0) {
                    wrap_json_pack (&elemJ, "{ss ss ss}", "uid", rtus[idx].uid, "uri",rtus[idx].uri, "info", rtus[idx].info);;
                } else {
                    wrap_json_pack (&elemJ, "{ss ss ss sb}", "uid", rtus[idx].uid, "uri",rtus[idx].uri, "info", rtus[idx].info, "status", err);;
                }
                break;
        }
        json_object_array_add(responseJ, elemJ);
        }   
    } else {
        // build global info page for developper dynamic HTML5 page
        json_object *globalJ, *rtuJ, *rtusJ, *statusJ, *sensorsJ, *admincmdJ, *usageJ, *actionsJ;
        CtlConfigT* ctlConfig = (CtlConfigT*)afb_api_get_userdata(afb_req_get_api(request));
        err= wrap_json_pack (&globalJ, "{ss ss* ss* ss*}", "uid", ctlConfig->uid, "info",ctlConfig->info, "version", ctlConfig->version, "author", ctlConfig->author);
        if (err) {
            AFB_DEBUG ("InfoRtu: Fail to wrap json binding metadata");
            goto OnErrorExit;
        }

        rtusJ= json_object_new_array();
        for (idx=0; rtus[idx].uid; idx++) {
            status= ModbusRtuIsConnected (request->api, &rtus[idx]);
            err= wrap_json_pack (&statusJ, "{ss si sb}", "uri",rtus[idx].uri, "slaveid", rtus[idx].slaveid, "status", status>=0);

            // prepare array to hold every sensor verbs
            sensorsJ=  json_object_new_array();
            err += wrap_json_pack (&actionsJ, "[s s s]", "info", "connect", "disconnect");
            err += wrap_json_pack (&usageJ, "{so, si}", "action", actionsJ, "verbose", 3);
            err += wrap_json_pack (&admincmdJ, "{ss ss ss so}", "uid", "admin", "info","RTU admin cmd", "verb", rtus[idx].adminapi, "usage", usageJ);
            json_object_array_add (sensorsJ, admincmdJ); 

            // create group object with rtu_info and rtu-sensors
            ModbusRtuSensorsId (&rtus[idx], 3, sensorsJ);
            err += wrap_json_pack (&rtuJ, "{ss ss* so* so}", "uid", rtus[idx].uid, "info", rtus[idx].info, "status", statusJ, "verbs", sensorsJ);
            if (err) {
                AFB_DEBUG ("InfoRtu: Fail to wrap json sensors info rtu=%s", rtus[idx].uid);
                goto OnErrorExit;
            }
            json_object_array_add(rtusJ, rtuJ);      
        }

        err= wrap_json_pack (&responseJ, "{so so}", "metadata", globalJ, "groups", rtusJ); 
        if (err) {
            AFB_DEBUG ("InfoRtu: Fail to wrap json binding global response");
            goto OnErrorExit;
        }
    }
    afb_req_success(request, responseJ, NULL);
    return;

OnErrorExit:
    return;
}

// Static verb not depending on Modbus json config file
static afb_verb_t CtrlApiVerbs[] = {
    /* VERB'S NAME         FUNCTION TO CALL         SHORT DESCRIPTION */
    { .verb = "ping",     .callback = PingTest    , .info = "Modbus API ping test"},
    { .verb = "info",     .callback = InfoRtu     , .info = "Modbus List RTUs"},
    { .verb = NULL} /* marker for end of the array */
};

static int CtrlLoadStaticVerbs (afb_api_t api, afb_verb_t *verbs, void *vcbdata) {
    int errcount=0;

    for (int idx=0; verbs[idx].verb; idx++) {
        errcount+= afb_api_add_verb(api, CtrlApiVerbs[idx].verb, CtrlApiVerbs[idx].info, CtrlApiVerbs[idx].callback, vcbdata, 0, 0,0);
    }

    return errcount;
};
static void RtuDynRequest(afb_req_t request) {

    // retrieve action handle from request and execute the request
    json_object *queryJ = afb_req_json(request);
    ModbusRtuT* rtu= (ModbusRtuT*) afb_req_get_vcbdata(request);
    ModbusRtuRequest (request, rtu, queryJ);
}

static void SensorDynRequest(afb_req_t request) {

    // retrieve action handle from request and execute the request
    json_object *queryJ = afb_req_json(request);
    ModbusSensorT* sensor = (ModbusSensorT*) afb_req_get_vcbdata(request);
    ModbusSensorRequest (request, sensor, queryJ);
}

static int SensorLoadOne(afb_api_t api, ModbusRtuT *rtu, ModbusSensorT *sensor, json_object *sensorJ) {
    int err = 0;
    const char *type=NULL;
    const char *format=NULL;
    const char *privilege=NULL;
    afb_auth_t *authent=NULL;
    json_object *argsJ=NULL;
    ModbusSourceT source;

    // should already be allocated
    assert (sensorJ);

    // set default values
    memset(sensor, 0, sizeof (ModbusSensorT));
    sensor->rtu   = rtu;
    sensor->hertz = rtu->hertz;
    sensor->iddle = rtu->iddle;
    sensor->count = 1;

    err = wrap_json_unpack(sensorJ, "{ss,ss,si,s?s,s?s,s?s,s?i,s?i,s?i,s?o,s?o,s?o !}",
                "uid", &sensor->uid,
                "type", &type,
                "register", &sensor->registry,
                "info", &sensor->info,
                "privilege", &privilege,
                "format", &format,
                "hertz", &sensor->hertz,
                "iddle", &sensor->iddle,
                "count", &sensor->count,
                "usage", &sensor->usage,
                "sample", &sensor->sample,
                "args", &argsJ);
    if (err) goto ParsingErrorExit;

    // keep sample and usage as object when defined
    if (sensor->usage) json_object_get (sensor->usage);
    if (sensor->sample) json_object_get (sensor->sample);

    // find modbus register type/function callback
    sensor->function = mbFunctionFind (api, type);
    if (!sensor->function) goto FunctionErrorExit;

    // find encode/decode callback
    sensor->format = mbEncoderFind (api, format);
    if (!sensor->format) goto TypeErrorExit;

    // Fulup should insert global auth here
    sensor->api = api;
    if (privilege) {
       authent= (afb_auth_t*) calloc(1, sizeof (afb_auth_t));
       authent->type = afb_auth_Permission;
       authent->text = privilege;
    }

    asprintf ((char**) &sensor->apiverb, "%s/%s", rtu->prefix, sensor->uid);
    
    // if defined call format init callback
    if (sensor->format->initCB) {
        source.sensor = sensor->uid;
        source.api = api;
        source.context=NULL;
        err = sensor->format->initCB (&source, argsJ);
        if (err) {
            AFB_API_ERROR(api, "SensorLoadOne: fail to init format verb=%s", sensor->apiverb);
            goto OnErrorExit;
        }
        // remember context for further encode/decode callback 
        sensor->context = source.context;
    }

    err = afb_api_add_verb(api, sensor->apiverb, sensor->info, SensorDynRequest, sensor, authent, 0, 0);
    if (err) {
        AFB_API_ERROR(api, "SensorLoadOne: fail to register API verb=%s", sensor->apiverb);
        goto OnErrorExit;
    }

    return 0;

ParsingErrorExit:
    AFB_API_ERROR(api, "SensorLoadOne: Fail to parse sensor: %s", json_object_to_json_string(sensorJ));
    return -1;

TypeErrorExit:
    AFB_API_ERROR(api, "SensorLoadOne: missing/invalid Format JSON=%s", json_object_to_json_string(sensorJ));
    return -1;
FunctionErrorExit:
    AFB_API_ERROR(api, "SensorLoadOne: missing/invalid Modus Type=%s JSON=%s", type, json_object_to_json_string(sensorJ));
    return -1;
OnErrorExit:
    return -1;    
}

static int ModbusLoadOne(afb_api_t api, ModbusRtuT *rtu, json_object *rtuJ) {
    int err = 0;
    json_object *sensorsJ;
    afb_auth_t *authent=NULL;

    // should already be allocated
    assert (rtuJ); 
    assert (api);

    memset(rtu, 0, sizeof (ModbusRtuT)); // default is empty
    err = wrap_json_unpack(rtuJ, "{ss,s?s,s?s,s?s,s?i,s?s,s?i,s?i,s?i,s?i,s?i,s?i,so}",
            "uid", &rtu->uid,
            "info", &rtu->info,
            "uri", &rtu->uri,
            "privileges", &rtu->privileges,
            "autostart", &rtu->autostart,
            "prefix", &rtu->prefix,
            "slaveid", &rtu->slaveid,
            "debug", &rtu->debug,
            "timeout", &rtu->timeout,
            "idlen", &rtu->idlen,
            "hertz", &rtu->hertz,
            "iddle", &rtu->iddle,
            "sensors", &sensorsJ);
    if (err) {
        AFB_API_ERROR(api, "Fail to parse rtu JSON : (%s)", json_object_to_json_string(rtuJ));
        goto OnErrorExit;
    }  

    // create an admin command for RTU
    if (rtu->privileges) {
       authent= (afb_auth_t*) calloc(1, sizeof (afb_auth_t));
       authent->type = afb_auth_Permission;
       authent->text = rtu->privileges;
    }

    // if not API prefix let's use RTU uid
    if (!rtu->prefix) rtu->prefix= rtu->uid;

    // set default pooling frequency
    if (!rtu->hertz) rtu->hertz=MB_DEFAULT_POLLING_FEQ;

    err=asprintf ((char**)&rtu->adminapi, "%s/%s", rtu->prefix, "admin");
    err= afb_api_add_verb(api, rtu->adminapi, rtu->info, RtuDynRequest, rtu, authent, 0, 0);
    if (err) {
        AFB_API_ERROR(api, "ModbusLoadOne: fail to register API uid=%s verb=%s info=%s", rtu->uid, rtu->adminapi, rtu->info);
        goto OnErrorExit;
    }

    // if uri is provided let's try to connect now
    if (rtu->uri && rtu->autostart) {
        err = ModbusRtuConnect (api, rtu);
        if (err) {
            AFB_API_ERROR(api, "ModbusLoadOne: fail to connect TCP/RTU uid=%s uri=%s", rtu->uid, rtu->uid);
            if (rtu->autostart > 1) goto OnErrorExit;
        }
    }

    // loop on sensors
    if (json_object_is_type(sensorsJ, json_type_array)) {
        int count = (int)json_object_array_length(sensorsJ);
        rtu->sensors= (ModbusSensorT*)calloc(count + 1, sizeof (ModbusSensorT));

        for (int idx = 0; idx < count; idx++) {
            json_object *sensorJ = json_object_array_get_idx(sensorsJ, idx);
            err = SensorLoadOne(api, rtu, &rtu->sensors[idx], sensorJ);
            if (err) goto OnErrorExit;
        }

    } else {
        rtu->sensors= (ModbusSensorT*) calloc(2, sizeof(ModbusSensorT));
        err= SensorLoadOne(api, rtu, &rtu->sensors[0], sensorsJ);
        if (err) goto OnErrorExit;
    }

    return 0;

OnErrorExit:
    return -1;    
}

static int ModbusConfig(afb_api_t api, CtlSectionT *section, json_object *rtusJ) {
    ModbusRtuT *rtus;
    int err;

    // everything is done during initial config call
    if (!rtusJ) return 0;

    // modbus array is close with a nullvalue;
    if (json_object_is_type(rtusJ, json_type_array)) {
        int count = (int)json_object_array_length(rtusJ);
        rtus =  (ModbusRtuT*) calloc(count + 1, sizeof (ModbusRtuT));

        for (int idx = 0; idx < count; idx++) {
            json_object *rtuJ = json_object_array_get_idx(rtusJ, idx);
            err = ModbusLoadOne(api, &rtus[idx], rtuJ);
            if (err) goto OnErrorExit;
        }

    } else {
        rtus = (ModbusRtuT*)calloc(2, sizeof (ModbusRtuT));
        err = ModbusLoadOne(api, &rtus[0], rtusJ);
        if (err) goto OnErrorExit;
    }


    // add static controls verbs
    err = CtrlLoadStaticVerbs (api, CtrlApiVerbs, (void*) rtus);
    if (err) {
        AFB_API_ERROR(api, "CtrlLoadOneApi fail to Registry static API verbs");
        goto OnErrorExit;
    }
    
    return 0;

OnErrorExit:
    AFB_API_ERROR (api, "Fail to initialise Modbus check Json Config");
    return -1;    
}


static int CtrlInitOneApi(afb_api_t api) {
    int err = 0;

    // retrieve section config from api handle
    CtlConfigT* ctlConfig = (CtlConfigT*)afb_api_get_userdata(api);

    err = CtlConfigExec(api, ctlConfig);
    if (err) {
        AFB_API_ERROR(api, "Error at CtlConfigExec step");
        return err;
    }

    return err;
}

static int CtrlLoadOneApi(void* vcbdata, afb_api_t api) {
    CtlConfigT* ctlConfig = (CtlConfigT*)vcbdata;

    // save closure as api's data context
    afb_api_set_userdata(api, ctlConfig);

    // load section for corresponding API
    int error = CtlLoadSections(api, ctlConfig, ctrlSections);

    // init and seal API function
    afb_api_on_init(api, CtrlInitOneApi);
    afb_api_seal(api);

    return error;    
}

int afbBindingEntry(afb_api_t api) {
    int status = 0;
    char *searchPath, *envConfig;
    afb_api_t handle;

    AFB_API_NOTICE(api, "Controller in afbBindingEntry");

    // register builtin encoders before plugin get loaded
    mbRegisterCoreEncoders();

    envConfig= getenv("CONTROL_CONFIG_PATH");
    if (!envConfig) envConfig = CONTROL_CONFIG_PATH;

    status=asprintf (&searchPath,"%s:%s/etc", envConfig, GetBindingDirPath(api));
    AFB_API_NOTICE(api, "Json config directory : %s", searchPath);

    const char* prefix = "control";
    const char* configPath = CtlConfigSearch(api, searchPath, prefix);
    if (!configPath) {
        AFB_API_ERROR(api, "afbBindingEntry: No %s-%s* config found in %s ", prefix, GetBinderName(), searchPath);
        status = ERROR;
        goto _exit_afbBindingEntry;
    }

    // load config file and create API
    CtlConfigT* ctlConfig = CtlLoadMetaData(api, configPath);
    if (!ctlConfig) {
        AFB_API_ERROR(api, "afbBindingEntry No valid control config file in:\n-- %s", configPath);
        status = ERROR;
        goto _exit_afbBindingEntry;
    }

    AFB_API_NOTICE(api, "Controller API='%s' info='%s'", ctlConfig->api, ctlConfig->info);

    // create one API per config file (Pre-V3 return code ToBeChanged)
    handle = afb_api_new_api(api, ctlConfig->api, ctlConfig->info, 1, CtrlLoadOneApi, ctlConfig);
    status = (handle) ? 0 : -1;

_exit_afbBindingEntry:
    free(searchPath);
    return status;
}
