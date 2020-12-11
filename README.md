# Modbus Binding

Modbus binding support TCP Modbus with format conversion for multi-register type as int32, Float, ...

## Install from RPM/APT:

 * Declare redpesk repository: [(see doc)](http://docs.redpesk.bzh/docs/en/master/developer-guides/host-configuration/docs/2-download-packages.html)

 * Redpesk: sudo dnf install modbus-binding afb-ui-devtools 
 * Fedora: sudo dnf install modbus-binding afb-ui-devtools 
 * OpenSuse: sudo dnf install modbus-binding afb-ui-devtools 
 * Ubuntu: sudo apt-get install modbus-binding afb-ui-devtools 

## Rebuilding from source

### Modbus Dependencies:
* Declare redpesk repository: [(see doc)](http://docs.redpesk.bzh/docs/en/master/developer-guides/host-configuration/docs/2-download-packages.html)

* From redpesk-core 
    * application framework 'afb-binder' & 'afb-binding-devel'
    * binding controller 'afb-libcontroller-devel'
    * binding helpers 'afb-libhelpers-devel'
    * cmake template 'afb-cmake-modules'
    * ui-devel html5 'afb-ui-devtools'
 * From your preferred Linux distribution
    * Libmodbus 3.1.6
    * Lua

 
 * Install LibModbus 
    + WARNING: Fedora-33 and many distro ship the old 3.0. This bind use the 3.1 !!! 
    + download from https://libmodbus.org/download/
    + wget https://libmodbus.org/releases/libmodbus-3.1.6.tar.gz
    + tar -xzf libmodbus-3.1.6.tar.gz && cd libmodbus-3.1.6/
    + ./configure --libdir=/usr/local/lib64
    + make && sudo make install-strip
    + Update conf.d/00-????-config.cmake with choose installation directory. ex: set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/libmodbus-3.1.6/lib64/pkgconfig")

 ### Modbus Binding build
    * mkdir build && cd build
    * cmake ..
    * make

## Running/Testing

By default Kingpigeon devices use a fix IP addr (192.168.1.110). You may need to add this network to your own desktop config before starting your test.
```
sudo ip a add  192.168.1.1/24 dev eth0 # eth0, enp0s20f0u4u1 or whatever is your ethernet card name
ping 192.168.1.110 # check you can ping your TCP modbus device
check default config with browser at http://192.168.1.110
```

### Start Sample Binder
```
afb-binder --name=afb-kingpigeon --port=1234  --ldpaths=src --workdir=. --verbose
open binding UI with browser at http://localhost:1234/devtools
```

### Test Binder in CLI
```
afb-client -H ws://localhost:1234/api
 # modbus ping
 # modbus info
 # modbus RTU0/D01_SWITCH {"action":"write","data":1}
 # modbus RTU0/D01_SWITCH {"action":"read"}
 # modbus RTU0/D01_SWITCH {"action":"write","data":0}
 # modbus RTU0/D01_SWITCH {"action":"read"}
```

### Adding your own config

Json config file is selected from *afb-binder --name=afb-midlename-xxx* option. This allows you to switch from one json config to the other without editing any file. *'middlename'* is use to select a specific config. As example *--name='afb-myrtu@lorient-modbus'* will select *modbus-myrtu@lorient-config.json*.

You may also choose to force your config file by exporting CONTROL_CONFIG_PATH environement variable. For further information, check binding controller documentation [here](http://docs.redpesk.bzh/docs/en/master/developer-guides/controllerConfig.html)

**Warning:** some TCP Modbus device as KingPigeon check SalveID even for buildin I/O. Generic config make the assumption that your slaveID is set to *'1'*. 

 
```
export CONTROL_CONFIG_PATH="$HOME/my-modbus-config-directory"
afb-binder --name=afb-myconfig --port=1234  --ldpaths=src --workdir=. --verbose

# connect with your browser on http://localhost:1234/devtools/index.html
```


## Modbus binding support a set of default encoder for values store within multiple registries. 

    * int16, bool => 1 register 
    * int32 => 2 registers
    * int64 => 4 registers
    * float, floatabcd, floatdabc, ...

Nevertheless user may also add its own encoding/decoding format to handle device specific representation (ex: device info string),or custom application encoding (ex: float to uint16 for an analog output). Custom encoder/decoder are store within user plugin (see sample at src/plugins/kingpigeon).

## API usage:

Modbus binding create one api/verb by sensor. By default each sensor api/verb is prefixed by the RTU uid. With following config mak
```
    "modbus": [
      {
        "uid": "King-Pigeon-myrtu",
        "info": "King Pigeon TCP I/O Module",
        "uri" : "tcp://192.168.1.110:502",
        "privilege": "global RTU required privilege",
        "autostart" : 1,  // connect to RTU at binder start
        "prefix": "myrtu  // api verb prefix 
        "timeout": xxxx // optional response timeout in ms
        "debug": 0-3 // option libmodbus debug level
        "hertz": 10  // default pooling for event subscription 
        "iddle": 0   // force event even when value does not change every hertz*iddle count
        "sensors": [
          {
            "uid": "PRODUCT_INFO",
            "info" : "Array with Product Number, Lot, Serial, OnlineTime, Hardware, Firmware",
            "function": "Input_Register",
            "format" : "plugin://king-pigeon/devinfo",
            "register" : 26,
            "privilege": "optional sensor required privilege"
          },
          {
            "uid": "DIN01_switch",
            "function": "Coil_input",
            "format" : "BOOL",
            "register" : 1,
            "privilege": "optional sensor required privilege"
            "herz": xxx // special pooling rate for this sensor 
            "iddle": xxx // special iddle force event when value does not change 
          },
          {
            "uid": "DIN01_counter",
            "function": "Register_Holding",
            "format" : "UINT32",
            "register" : 6,
            "privilege": "optional sensor required privilege"
            "herz": xxx // special pooling rate for this sensor 
          },
    ...      
```

## Modbus controller exposed

### Two builtin api/verb 

  * api://modbus/ping // check if binder is alive
  * api://modbus/info // return registered MTU

### One introspection api/verb per declared RTU
    * api://modbus/myrtu/info

### On action api/verb per declared Sensor    

    * api://modbus/myrtu/din01_switch
    * api://modbus/myrtu/din01_counter
    * etc ...

### For each sensors the API accept 3 actions

    * action=read (return register(s) value after format decoding)
    * action=write (push value on register(s) after format encoding)
    * action=subscribe (subscribe to sensors value changes, frequency is defined by sensor or globally at RTU level)

### Format Converter

The Modbus binding supports both builtin format converter and optional custom converter provided by user through plugins.

  * Standard converter include the traditional INT16, UINT16, INT32, UINT32, FLOATABCD, ... Depending on the format one or more register is read
  * Custom converter are provided through optional plugins. Custom converter should declare a static structure and register it at plugin loadtime(CTLP_ONLOAD). 
    * uid is the formatter name as declare inside JSON config file.
    * decode/encore callback are respectively called for read/write action
    * init callback is call at format registration time and might be used to process a special value for a given sensor (e.g; deviation for a wind sensor). Each sensor attaches a void* context. Developer may declare a private context for each sensor (e.g. to store a previous value, a min/max, ...). The init callback receive sensor source to store context and optionally the ARGS json object when present within sensor json config.
  
  * WARNING: do not confuse format count and nbreg. NBreg is the number of 16bits registers use for a given formatter (e.g. 4 for a 64bits float). Count is the number of value you want to read in one operation (e.g. you may want to read all your digital input in one operation and receive them as an array of boolean)

```
// Sample of custom formatter (king-pigeon-encore.c)
// -------------------------------------------------
static ModbusFormatCbT pigeonEncoders[] = {
    {
      .uid="devinfo",
      .info="return KingPigeon Device Info as an array",
      .nbreg=6, .decodeCB=decodePigeonInfo, 
      .encodeCB=encodePigeonInfo
    },{
      .uid="rcount",
      .info="Return Relative Count from Uint32", 
      .nbreg=2, .decodeCB=decodeRCount, .encodeCB=NULL, 
      .initCB=initRCount
    },{.uid=NULL} // must be NULL terminated
};
```
