# Mosbus Binding

Modbus binding support TCP Modbus with format convertion for multi-register type as int32, Float, ...

## Dependencies:
 * AGL application framework 'agl-app-framework-binder-devel'
 * AGL controller 'agl-libappcontroller-devel'
 * AGL helpers 'agl-libafb-helpers-devel'
 * AGL cmake template 'agl-cmake-apps-module'
 * Libmodbus
 * Lua

## AGL dependencies:
 * Declare AGL repository: [(see doc)](https://docs.automotivelinux.org/docs/en/guppy/devguides/reference/2-download-packages.html#install-the-repository)
 * Install AGL controller: [(see doc)](https://docs.automotivelinux.org/docs/en/guppy/devguides/reference/ctrler/controller.html)
 * Install LibModbus

 ```
  # if you did not logout, don't forget to source AGL environement
  source /etc/profile.d/agl-app-framework-binder.sh
 ```

    + download from https://libmodbus.org/download/
    + cd libmodbus-3.1.6/
    + ./configure --prefix=/opt/libmodbus-3.1.6
    + make && sudo make install-strip
    + Update conf.d/00-????-config.cmake with choosen installation directory. ex: set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/libmodbus-3.1.6/lib64/pkgconfig")

## Modbus Binding build
    * mkdir build && cd build
    * cmake ..
    * make

## Running/Testing

By default Kingpigeon devices use a fix IP addr (192.168.1.110). You may need to add this network to your own desktop config before starting your test.
```
sudo ip a add  192.168.1.1/24 dev eth0 # eth0 or whatever is your ethernet card name
ping 192.168.1.110 # check you can ping your TCP modbus device
check config with a browser at http://192.168.1.110
```


## Modbus binding is an custom instance of the AGL controller, it uses a standard JSON file to describe Modbus network and sensors topologie.

    * see config sample at: cond.d/project/etc/*

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
        "uid": "King-Pigeon-MT110",
        "info": "King Pigeon 'MT110' Modbus TCP Remote I/O Module",
        "uri" : "tcp://192.168.1.110:502",
        "privilege": "global RTU requiered privilege",
        "autostart" : 1,
        "prefix": "mt110
        "sensors": [
          {
            "uid": "PRODUCT_INFO",
            "info" : "Array with Product Number, Lot, Serial, OnlineTime, Hardware, Firmware",
            "function": "Input_Register",
            "format" : "plugin://king-pigeon/devinfo",
            "register" : 26,
            "privilege": "optionnal sensor required privilege"
          },
          {
            "uid": "D01_switch",
            "function": "Single_Coil",
            "format" : "BOOL",
            "register" : 1,
            "privilege": "optionnal sensor required privilege"
          },
    ...      
```

Modbus controller will expose two sensors api/verb and one introspection verb
    * api://modbus/mt110/info (depending on verbosity return sensors list + values)
    * api://modbus/mt110/product_info
    * api://modbus/mt110/d01_switch

For each sensors the API accept 3 actions
    * action=read (return register(s) value after format decoding)
    * action=write (push value on register(s) after format encoding)
    * action=subscribe (sucribe to sensors value changes, frequency is defined by sensor or gloablly at RTU level)

Mosbus also expose two extra api/verb for test/config management
    * api://modbus/ping (check is modbus binding is alive)
    * api://modbus/info (depending on verbosity return RTU list + sensors)