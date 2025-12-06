# ESPHome ZigBee Component

External ZigBee component for ESPHome, enabling integration with Zigbee devices using the ESP Zigbee SDK.

> [!TIP]
> **New simple Mode! No more endpoint definitions needed.**
>
> I started to implement the automated endpoint definition generation, see basic mode section for details.

> [!Important]
> **Please help to collect working cluster definitions [here](https://github.com/luar123/zigbee_esphome/discussions/22).**
>  
> **If something is not working please check the [troubleshooting](#troubleshooting) section first. Config validation is not complete. Always consult [Zigbee Cluster Library](https://csa-iot.org/wp-content/uploads/2022/01/07-5123-08-Zigbee-Cluster-Library-1.pdf) for cluster definitions**

## Table of Contents
1. [Features](#features)
2. [Requirements](#requirements)
3. [Installation](#installation)
4. [Usage](#usage)
5. [Configuration](#configuration)
6. [Basic Mode](#basic-mode)
7. [Advanced Mode](#advanced-mode)
8. [Actions](#actions)
9. [Time Sync](#time-sync)
10. [Troubleshooting](#troubleshooting)
11. [Limitations](#limitations)
12. [Contributing](#contributing)
13. [External Documentation](#external-documentation)

## Features

* **Automated Generation**: Automated generation of Zigbee definitions for lights, switches, sensors and binary sensors (see basic mode)
* **Manual Definitions**: Definition of endpoints, clusters and attributes supported by esp-zigbee-sdk 1.6
* **Attribute Actions**: Set attributes action
* **Reporting**: Manual report action
* **Reset**: Reset zigbee action
* **Join Trigger**: Join trigger
* **Attribute Triggers**: Attribute received trigger
* **Time Sync**: Time sync with coordinator
* **Custom Clusters**: Custom clusters and attributes
* **Component Support**: (normal, binary, text) sensors, switches and lights can be connected to attributes without need for lambdas/actions 
* **WiFi Co-existence**: WiFi co-existence on ESP32-C6 and ESP32-C5
* **Deep Sleep**: Deep-sleep should work
* **Router Support**: Router support

## Requirements

* **ESP-IDF**: >=5.1.4
* **ESPHome**: >=2025.7
* **Supported Boards**: ESP32-H2, ESP32-C5, ESP32-C6 SoC

## Installation

### Using External Component (Recommended)

Add to your ESPHome YAML configuration:

```yaml
external_components:
  - source: github://luar123/zigbee_esphome
    components: [zigbee]

zigbee:
  components: all # to add all supported components
```

## Usage

Include external component:
```yaml
external_components:
  - source: github://luar123/zigbee_esphome
    components: [zigbee]

zigbee:
  components: all # to add all supported components
```

## Configuration

### Zigbee Configuration Variables:

* **id** (Optional, string): Manually specify the ID for code generation.
* **name** (Optional, string): Zigbee Model Identifier in basic cluster. Used by coordinator to match custom converters. Defaults to esphome.name
* **manufacturer** (Optional, string): Zigbee Manufacturer Name in basic cluster. Used by coordinator to match custom converters. Defaults to "esphome"
* **date** (Optional, string): Date Code in basic cluster. Defaults to build time
* **power_supply** (Optional, int): Zigbee Power Source in basic cluster. See ZCL. Defaults to 0 = unknown
* **version** (Optional, int): Zigbee App Version in basic cluster. Defaults to 0
* **area** (Optional, int): Zigbee Physical Environment in basic cluster. See ZCL. Defaults to 0 = unknown
* **router** (Optional, bool): Create a router device instead of an end device. Defaults to false
* **debug** (Optional, bool): Print zigbee stack debug messages
* **components** (Optional, string|list): all: add definitions for all supported components that have a name and are not marked as internal. None: Add no definitions (default). List of component ids: Add only those. Can be combined with manual definitions in endpoints
* **as_generic** (Optional, bool): Use generic/basic clusters where possible. Currently sensors and switches. Defaults to false
* **endpoints** (Optional, list): endpoint list for advanced definitions. See examples

## Basic Mode

By adding `components: all` the endpoint definition is generated automatically. Currently sensor, binary_sensor, light and switch components are supported. Because this is an external component the whole implementation is a bit hacky and likely to fail with some setups. Also it is not possible to tweak the generated definitions. Each entity creates a new endpoint.

**Important**: you must include the required partition table file in your ESP32 configuration each time that you using this components:

```yaml
esp32:
  partitions: partitions_zb.csv
```

This file is mandatory for proper operation and can be found in the component directory. It defines the memory partitions needed for Zigbee operations.

### Example:
```yaml
zigbee:
  id: "zb"
  components: all
```

## Advanced Mode

Endpoint/Cluster definitions can be defined manually. Can be combined with automated definition.

### Advanced Example:
```yaml
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
    - device_type: HEATING_COOLING_UNIT
      num: 3
      clusters:
       - id: THERMOSTAT
         role: "Client"
         attributes:
           - attribute_id: 0x0008 # PIHeatingDemand
             type: U8
             value: 0
             on_report:
               then:
                 # The below lambda will be called with an argument
                 # `ZigBeeReportData <T> x` where ZigBeeReportData is defined as
                 #
                 # template<typename T> struct ZigBeeReportData {
                 #   // Value of the attribute sent from server side.
                 #   T value;
                 #   // Address of device which sent this value.
                 #   esp_zb_zcl_addr_t src_address;
                 #   // Number of the endpoint on device which sent this value.
                 #   uint8_t src_endpoint;
                 # };
                 #
                 # And T is a C++ type matching the type of the attribute.
                 - lambda: |-
                     ESP_LOGD("main", "Received PIHeatingDemand | address type: 0x%02x; short addr: 0x%04x; ieee addr: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                              x.src_address.addr_type, x.src_address.u.short_addr,
                              x.src_address.u.ieee_addr[7], x.src_address.u.ieee_addr[6],
                              x.src_address.u.ieee_addr[5], x.src_address.u.ieee_addr[4],
                              x.src_address.u.ieee_addr[3], x.src_address.u.ieee_addr[2],
                              x.src_address.u.ieee_addr[1], x.src_address.u.ieee_addr[0]);
                 - switch.control:
                     id: relay_1_switch
                     state: !lambda "return (x.value>10);"
  on_join:
    then:
      - logger.log: "Joined network"
```

## Actions

### zigbee.setAttr
* **id**: "id of attribute"
* **value**: "value, can be a lambda"
  * only numeric or string types

### zigbee.report
* **id**: "id of zigbee component"
* **Manually send reports for all attributes with report=true**

### zigbee.reportAttr
* **id**: "id of zigbee_attribute component"
* **Manually send report for attribute**

### zigbee.reset
* **id**: "id of zigbee component"
* **Reset Zigbee Network and Device. Leave the current network and tries to join open networks.**

## Time Sync

Add a 'time' component with platform 'zigbee', e.g.:

```yaml
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

* **Build errors**
  * Try to run `esphome clean <name.ymal>` 
  * Try to delete the `.esphome/build/<name>/` folder
* **ESP crashes**
  * Try to erase completely with `esptool.py erase_flash` and flash again.
  * Make sure your configuration is valid. Config validation is not complete. Always consult [Zigbee Cluster Library](https://csa-iot.org/wp-content/uploads/2022/01/07-5123-08-Zigbee-Cluster-Library-1.pdf) for cluster definitions
  * Common issues are that attributes do not support reporting (try set `report: false`), use a different type, or are not readable/writable (see ZCL).
* **Zigbee is not working as expected**
  * Whenever the cluster definition changed you need to re-interview and remove/add the device to your network.
  * Sometimes it helps to power-cycle the coordinator and restarting z2m.
  * Remove other endpoints. Sometimes coordinators struggle with multiple endpoints.

## Limitations

* **No Coordinator Support**: No coordinator devices
* **Attribute Set**: Attribute set action works only with numeric types and character string
* **OnValue Trigger**: OnValue trigger works only with numeric types
* **Reporting**: Reporting can be enabled, but not configured
* **Control Devices**: No control devices like switches [workaround available](https://github.com/luar123/zigbee_esphome/discussions/18#discussioncomment-11875376)
* **ESP-IDF Requirement**: Needs esp-idf >=5.1.4
* **ESPHome Version**: Needs esphome >=2025.7
* **Scenes**: Scenes not implemented
* **Endpoint Limit**: Officially the zigbee stack supports only 10 endpoints. However, this is not enforced and at least for sensor endpoints more than 10 seem to work. More than 10 light endpoints will crash!
* **Zigbee2MQTT Compatibility**: Only one light is supported without creating a custom converter/definition
* **Zigbee2MQTT Sensors**: Analog input cluster (used for sensors) is supported by 2025 October release, but ignores type and unit
* **ZHA Compatibility**: Analog input cluster (used for sensors) without unit/type is ignored
* **ZHA Reporting**: Minimum reporting interval is set to high values (30s) for some sensors and can't be changed. Keep that in mind if reporting seems not to work properly.

## Contributing

**Please submit all PRs here** and not to https://github.com/luar123/esphome/tree/zigbee

Use pre-commit hook by enabling you esphome environment first and then running `pre-commit install` in the git root foulder.

If looking to contribute to this project, then suggest follow steps in these guides + look at issues in Espressif's ESP Zigbee SDK repository:

* https://github.com/espressif/esp-zigbee-sdk/issues
* https://github.com/firstcontributions/first-contributions/blob/master/README.md
* https://github.com/firstcontributions/first-contributions/blob/master/github-desktop-tutorial.md

## External Documentation

### Official Resources

* [ESP Zigbee SDK Programming Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/)
* [ESP-Zigbee-SDK Github repo](https://github.com/espressif/esp-zigbee-sdk)
* [ESP-Zigbee-SDK examples](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/)
  * [Zigbee HA Example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample)
    * [Zigbee HA Light Bulb example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_on_off_light)
    * [Zigbee HA temperature sensor example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_temperature_sensor)
    * [Zigbee HA thermostat example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_thermostat)

### Additional Resources

* [ESP32-H2 Application User Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32h2/application.html)
* [ESP32-C6 Application User Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32c6/application.html)

## Example Zigbee Device

### Hardware Required

* One development board with ESP32-H2, ESP32-C5 or ESP32-C6 SoC acting as Zigbee end-device (that you will load ESPHome with the example config to).
  * For example, the official [ESP32-H2-DevKitM-1](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/esp32h2/esp32-h2-devkitm-1/user_guide.html) development kit board.
* [AHT10 Temperature+Humidity Sensor](https://next.esphome.io/components/sensor/aht10) connected to I2C pins (SDA: 12, SCL: 22) for the aht10 example.
* A USB cable for power supply and programming.
* (Optional) A USB-C cable to get ESP32 logs from the UART USB port (UART0).

### Build ESPHome Zigbee sensor

* We will build [example_esp32h2.yaml](example_esp32h2.yaml) file.
* Check [Getting Started with the ESPHome Command Line](https://esphome.io/guides/getting_started_command_line.html) tutorial to set up your dev environment.
* Build with `esphome run example_esp32h2.yaml`.

## Notes

* I don't have much free time to work on this right now, so feel free to fork/improve/create PRs/etc.
* At the moment, the C++ implementation is rather simple and generic. I tried to keep as much logic as possible in the python part. However, endpoints/clusters ~~/attributes~~ could also be classes, this would simplify the yaml setup but requires more sophisticated C++ code. 
* There is also a project with more advanced C++ zigbee libraries for esp32 that could be used here as well: https://github.com/Muk911/esphome/tree/main/esp32c6/hello-zigbee
* [parse_zigbee_headers.py](components/zigbee/files_to_parse/parse_zigbee_headers.py) is used to create the python enums and C helper functions automatically from zigbee sdk headers.
* Deprecated [custom zigbee component](https://github.com/luar123/esphome_zb_sensor)
