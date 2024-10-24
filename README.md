> [!WARNING]
> **With the latest commit a different way of loading the zigbee sdk libs is used. Therefore it is recommended to delete the `.esphome/build/<name>/` folder.**

# ESPHome ZigBee external component

External ZigBee component for ESPHome.

### Features
* Definition of endpoints, clusters and attributes supported by esp-zigbee-sdk 1.5
* Set attributes action
* Manual report action
* Reset zigbee action
* Join trigger
* Attribute received trigger
* Deep-sleep should work
* Not tested: groups/scenes

### Limitations
* Only end devices
* Attribute set action and OnValue trigger works only with integer types
* Reporting can be enabled, but not configured
* No control devices like switches
* Needs esp-idf >=5.1.4

### ToDo List (Short-Mid term)
* Custom clusters/attributes
* Switch to esp-zigbee-sdk 1.5 (done)
* Time component
* Light effects (through identify cluster commands)
* Router devices
* Easier support for sensors

### Not planed (feel free to submit a PR)
* Coordinator devices
* Binding config in yaml
* Reporting config in yaml
* Control device support (e.g. switches)

### Notes
* I don't have much free time to work on this right now, so feel free to fork/improve/create PRs/etc.
* At the moment, the C++ implementation is rather simple and generic. I tried to keep as much logic as possible in the python part. However, endpoints/clusters/attributes could also be classes, this would simplify the yaml setup but requires more sophisticated C++ code. 
* There is also a project with more advanced C++ zigbee libraries for esp32 that could be used here as well: https://github.com/Muk911/esphome/tree/main/esp32c6/hello-zigbee
* [parse_zigbee_headers.py](components/zigbee/files_to_parse/parse_zigbee_headers.py) is used to create the python enums and C helper functions automatically from zigbee sdk headers.
* Deprecated [custom zigbee component](https://github.com/luar123/esphome_zb_sensor)

## Usage

Include external component:
```
external_components:
  - source: github://luar123/zigbee_esphome
    components: [ zigbee ]

zigbee:
  ...
```
### Configuration variables:
[Todo]

Example:
```
zigbee:
  id: "zb"
  endpoints:
    - num: 1
      device_type: COLOR_DIMMABLE_LIGHT
      clusters:
        - id: ON_OFF
          attributes:
            - id: 0
              type: bool
              on_value:
                then:
                  - light.control:
                      id: light_1
                      state: !lambda "return (bool)x;"
        - id: LEVEL_CONTROL
          attributes:
            - id: 0
              type: U8
              on_value:
                then:
                  - light.control:
                      id: light_1
                      brightness: !lambda "return ((float)x)/255;"
    - device_type: TEMPERATURE_SENSOR
      num: 2
      clusters:
        - id: REL_HUMIDITY_MEASUREMENT
          attributes:
            - id: 0
              type: U16
              report: true
              value: 200
        - id: TEMP_MEASUREMENT
          attributes:
            - id: 0x0
              type: S16
              report: true
              value: 100
  on_join:
    then:
      - logger.log: "Joined network"
```

### Actions
* zigbee.setAttr
  * id: "id of zigbee component"
  * endpoint: "endpoint number"
  * cluster: "cluster id"
  * attribute: "attribute id"
  * value: "value, can be a lambda"
    * only integer types
* zigbee.report: "id of zigbee component"
  * Manually send reports
* zigbee.reset: "id of zigbee component"
  * Reset Zigbee Network and Device. Leave the current network and tries to join open networks.

Examples:
```
    on_value:
      then:
        - zigbee.setAttr:
            id: zb
            endpoint: 2
            cluster: REL_HUMIDITY_MEASUREMENT
            attribute: 0x0
            value: !lambda "return x*100;"
```
```
    on_press:
      then:
        - zigbee.report: zb
```



## Example Zigbee device

ESPHome Zigbee using only dev board or additionally [AHT10 Temperature+Humidity Sensor](https://next.esphome.io/components/sensor/aht10).

### Hardware Required

* One development board with ESP32-H2 or ESP32-C6 SoC acting as Zigbee end-device (that you will load ESPHome with the example config to).
  * For example, the official [ESP32-H2-DevKitM-1](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/user_guide.html) development kit board.
* [AHT10 Temperature+Humidity Sensor](https://next.esphome.io/components/sensor/aht10) connected to I2C pins (SDA: 12, SCL: 22) for the aht10 example.
* A USB cable for power supply and programming.
* (Optional) A USB-C cable to get ESP32 logs from the UART USB port (UART0).

### Build ESPHome Zigbee sensor

* We will build [example_esp32h2.yaml](example_esp32h2.yaml) file.
* Check [Getting Started with the ESPHome Command Line](https://esphome.io/guides/getting_started_command_line.html) tutorial to set up your dev environment.
* Build with `esphome run example_esp32h2.yaml`. 

## How to contribute

Please submit PRs targeting the zigbee external component to https://github.com/luar123/esphome/tree/zigbee

PRs targeting the examples or documentation should be submitted here.

If looking to contribute to this project, then suggest follow steps in these guides + look at issues in Espressif's ESP Zigbee SDK repository:

- https://github.com/espressif/esp-zigbee-sdk/issues
- https://github.com/firstcontributions/first-contributions/blob/master/README.md
- https://github.com/firstcontributions/first-contributions/blob/master/github-desktop-tutorial.md


## External documentation and reference

Note! The official documentation and reference examples for the ESP Zigbee SDK can currently be obtained from Espressif:

 - [ESP32 Zigbee SDK Programming Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/)
   - [ESP32-H2 Application User Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32h2/application.html)
   - [ESP32-C6 Application User Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/application.html)
- [ESP-Zigbee-SDK Github repo](https://github.com/espressif/esp-zigbee-sdk)
  - [ESP-Zigbee-SDK examples](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/)
    - [Zigbee HA Example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample)
      - [Zigbee HA Light Bulb example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_on_off_light)
      - [Zigbee HA temperature sensor example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_temperature_sensor)
      - [Zigbee HA thermostat example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_thermostat)
