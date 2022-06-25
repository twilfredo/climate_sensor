# Climate Sensor - WIP

## Goals

A small application developed to monitor indoor environmental parameters using the Nordic Thingy52 (nRF52840). Powered by Zephyr RTOS.

The following parameters are currently monitored:
  
  * Temperature - Degrees Celsius
  * Relative Humidity - %
  * Equivalent carbon dioxide reading (eCO2) - ppm
  * Equivalent total volatile organic compound - ppb
  * Battery Voltage/Level

The parameter are displayed on to the OLED display connected on the external I2C bus supports SSD1306/SH1106 (with kConfig) display driver IC. The metrics display can be toggled through by the onboard push button.

## Let there B-roll...

![cs4](https://user-images.githubusercontent.com/36925352/175752631-ffabccc8-6a0b-42b9-ad8c-2bac82e2cf2d.jpg)

![cs2](https://user-images.githubusercontent.com/36925352/175752445-b06e7d26-2563-4034-af1a-e44c9b4dc1be.jpg)

![cs1](https://user-images.githubusercontent.com/36925352/175752447-9ba142af-9ab5-476f-9ae4-290654b8a343.jpg)


## Building/Flashing

App has been tested on latest zephyr release v3.1 (also supports the current development branch as of 25/06/22)

```
git clone git@github.com:thulithwilfred/climate_sensor.git
cd climate_sensor

west build
west flash -r jlink
```
