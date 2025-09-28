> [!TIP]
> **New simple Mode! No more endpoint definitions needed.**
>
> I started to implement the automated endpoint definition generation, see basic mode section for details.

> [!Important]
> **Please help to collect working cluster definitions [here](https://github.com/luar123/zigbee_esphome/discussions/22).**
>  
> **If something is not working please check the [troubleshooting](#troubleshooting) section first. Config validation is not complete. Always consult [Zigbee Cluster Library](https://csa-iot.org/wp-content/uploads/2022/01/07-5123-08-Zigbee-Cluster-Library-1.pdf) for cluster definitions**

# ESPHome ZigBee external component

External ZigBee component for ESPHome.

### Features
* Automated generation of zigbee definition for lights, switches, sensors and binary sensors (see basic mode)
* Definition of endpoints, clusters and attributes supported by esp-zigbee-sdk 1.6
* Set attributes action
* Manual report action
* Reset zigbee action
* Join trigger
* Attribute received trigger
* Time sync with coordinator
* Custom clusters and attributes
* (normal, binary, text) sensors, switches and lights can be connected to attributes without need for lambdas/actions 
* Wifi co-existence on ESP32-C6
* Deep-sleep should work
* Not tested: groups
* Time sync with coordinator
* Router

### Limitations
* No coordinator devices
* Attribute set action works only with numeric types and character string
* Attribute OnValue trigger works only with numeric types
* Reporting can be enabled, but not configured
* No control devices like switches ([workaround](https://github.com/luar123/zigbee_esphome/discussions/18#discussioncomment-11875376))
* Needs esp-idf >=5.1.4
* Needs esphome >=2025.7
* scenes not implemented
* Officially the zigbee stack supports only 10 endpoints. however, this is not enforced and at least for sensor endpoints more than 10 seem to work. More then 10 light endpoints will crash!
* zigbee2mqtt: Only one light is supported without creating a custom converter/definition
* zigbee2mqtt: Analog input cluster (used for sensors) supported by 2025 October release, but ignors type and unit
* ZHA: Analog input cluster (used for sensors) without unit/type are ignored
* ZHA: Minimum reporting interval is set to high values (30s) for some sensors and can't be changed. Keep that in mind if reporting seems not to work properly.

### ToDo List (Short-Mid term)
* Light effects (through identify cluster commands)
* more components to support basic mode

### Not planed (feel free to submit a PR)
* Coordinator devices
* Binding config in yaml
* Reporting config in yaml
* Control device support like switches ([workaround](https://github.com/luar123/zigbee_esphome/discussions/18#discussioncomment-11875376))

## Usage

Include external component:
```
external_components:
  - source: github://luar123/zigbee_esphome
    components: [zigbee]

zigbee:
  components: all # to add all supported components
  ...
```
### Configuration variables:
* **id** (Optional, string): Manually specify the ID for code generation.
* **name** (Optional, string): Zigbee Model Identifier in basic cluster. Used by coordinator to match custom converters. Defaults to esphome.name
* **manufacturer** (Optional, string): Zigbee Manufacturer Name in basic cluster. Used by coordinator to match custom converters. Defaults to "esphome"
* **date** (Optional, string): Date Code in basic cluster. Defaults to build time
* **power_supply** (Optional, int): Zigbee Power Source in basic cluster. See ZCL. Defaults to 0 = unknown
* **version** (Optional, int): Zigbee App Version in basic cluster. Defaults to 0
* **area** (Optional, int): Zigbee Physical Environment in basic cluster. See ZCL. Defaults to 0 = unknown
* **router** (Optional, bool): Create a router device instead of an end device. Defaults to false
* **debug** (Optional, bool): Print zigbee stack debug messages
* **components** (Optional, string|list): all: add definitions for all supported components. None: Add no definitions (default). List of component ids: Add only those. Can be combined with manual definitions in endpoints
* **as_generic** (Optional, bool): Use generic/basic clusters where possible. Currently sensors and switches. Defaults to false
* **endpoints** (Optional, list): endpoint list for advanced definitions. See examples

[Todo]

### Basic mode
By adding `components: all` the endpoint definition is generated automatically. Currently sensor, binary_sensor, light and switch components are supported. Because this is an external component the whole implementation is a bit hacky and likely to fails with some setups. Also it is not possible to tweak the generated definitions. Each entity creates a new endpoint. Lights are using the light clusters, switches on_off or binary_output clusters, binary sensors using binary input clusters and sensors are using either special clusters (e.g. temperature) or analog input clusters. For sensors also the unit/type is set. Please note that these definitions are not complete. Feel free to open an issue or pull request (see zigbee_ep.py)

example:
```
zigbee:
  id: "zb"
  components: all
```

### Advanced mode
Endpoint/Cluster definitions can be defined manually. Can be combined with automated definition.

Advanced example:
```
zigbee:
  id: "zb"
  endpoints:
    - num: 1
      device_type: COLOR_DIMMABLE_LIGHT
      clusters:
        - id: ON_OFF
          attributes:
            - attribute_id: 0
              type: bool
              on_value:
                then:
                  - light.control:
                      id: light_1
                      state: !lambda "return (bool)x;"
        - id: LEVEL_CONTROL
          attributes:
            - attribute_id: 0
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
            - attribute_id: 0
              id: hum_attr
              type: U16
              report: true
              value: 200
        - id: TEMP_MEASUREMENT
          attributes:
            - attribute_id: 0x0
              type: S16
              report: true
              value: 100
              device: temp_sensor_id
              scale: 100
  on_join:
    then:
      - logger.log: "Joined network"
```

### Actions
* zigbee.setAttr
  * id: "id of attribute"
  * value: "value, can be a lambda"
    * only numeric or string types
* zigbee.report: "id of zigbee component"
  * Manually send reports for all attributes with report=true
* zigbee.reportAttr: "id of zigbee_attribute component"
  * Manually send report for attribute
* zigbee.reset: "id of zigbee component"
  * Reset Zigbee Network and Device. Leave the current network and tries to join open networks.

Examples:
```
    on_value:
      then:
        - zigbee.setAttr:
            id: hum_attr
            value: !lambda "return x*100;"
```
```
    on_press:
      then:
        - zigbee.report: zb
```

### Time sync
Add a 'time' component with platform 'zigbee', e.g.:
```
zigbee:
  id: "zb"
  ...

time:
  - platform: zigbee
    timezone: Europe/London
    on_time_sync:
      then:
        - logger.log: "Synchronized system clock"
    on_time:
      - seconds: /10
        then:
          - logger.log: "Tick-tock 10 seconds"
```

## Troubleshooting
* Build errors
  - Try to run `esphome clean <name.ymal>` 
  - Try to delete the `.esphome/build/<name>/` folder
* ESP crashes
  - Try to erase completely with `esptool.py erase_flash` and flash again.
  - Make sure your configuration is valid. Config validation is not complete. Always consult [Zigbee Cluster Library](https://csa-iot.org/wp-content/uploads/2022/01/07-5123-08-Zigbee-Cluster-Library-1.pdf) for cluster definitions
  - Common issues are that attributes do not support reporting (try set `report: false`), use a different type, or are not readable/writable (see ZCL).
* Zigbee is not working as expected
  - Whenever the cluster definition changed you need to re-interview and remove/add the device to your network.
  - Sometimes it helps to power-cycle the coordinator and restarting z2m.
  - Remove other endpoints. Sometimes coordinators struggle with multiple endpoints.

## Notes
* I don't have much free time to work on this right now, so feel free to fork/improve/create PRs/etc.
* At the moment, the C++ implementation is rather simple and generic. I tried to keep as much logic as possible in the python part. However, endpoints/clusters ~~/attributes~~ could also be classes, this would simplify the yaml setup but requires more sophisticated C++ code. 
* There is also a project with more advanced C++ zigbee libraries for esp32 that could be used here as well: https://github.com/Muk911/esphome/tree/main/esp32c6/hello-zigbee
* [parse_zigbee_headers.py](components/zigbee/files_to_parse/parse_zigbee_headers.py) is used to create the python enums and C helper functions automatically from zigbee sdk headers.
* Deprecated [custom zigbee component](https://github.com/luar123/esphome_zb_sensor)

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

**Please submit all PRs here** and not to https://github.com/luar123/esphome/tree/zigbee

Use pre-commit hook by enabling you esphome environment first and then running `pre-commit install` in the git root foulder.

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
