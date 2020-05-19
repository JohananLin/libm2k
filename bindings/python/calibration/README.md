# ADALM2000 calibrated using predefined values

## Generating your own calibration file
A calibration file is a file used at boot time to write inside the
context some predefined values for the calibration process. The values
will be stored until they will be overwritten.

This file must be called 'm2k-calib-temp-lut.ini' and it should contain
only a row having the following format:

cal,temp_lut=[\<temperature>,<adc_offset1>,<adc_offset2>,<adc_gain1> <adc_gain2>,
<dac_offset1>,<dac_offset2>,<dac_gain1>,<dac_gain2> [...]]

### Command
    generate_temperature_calib_lut <uri> -t [max_temp=<val>] [nb_values=<val>]

### Examples
Create the ini file 'm2k-calib-temp-lut.ini'. The process can be stopped by
pressing 'CTRL + C' or when the temperature of the board rises up to 75 °C

    python3 generate_temperature_calib_lut.py 192.168.2.1 -t

Create the ini file 'm2k-calib-temp-lut.ini'. The process can be stopped by
pressing 'CTRL + C' or when the temperature of the board rises up to 54 °C

    python3 generate_temperature_calib_lut.py auto -t max_temp=54

Create the ini file 'm2k-calib-temp-lut.ini'. Extract 5 values from all
computed calibration parameters.

    python3 generate_temperature_calib_lut.py auto -t nb_values=5 max_temp=54

## Calibrating using ini file
The purpose of this calibration type is to automate and to make the calibration
process more efficient. Please make sure to have the following requirments:
* A valid calibration file. The file can be generated using the script mentioned above.
* Firmware v0.26 on your board.
* Head of master - libm2k

After the board booted, copy the ini file inside the m2k drive. Then eject the drive and wait
for ADALM2000 to boot. Now everything is ready!

In order to perform a fast calibration call
the fallowing context method: calibrateFromContext()

For more information, please visit out wiki page: https://wiki.analog.com/university/tools/m2k/libm2k/libm2k
