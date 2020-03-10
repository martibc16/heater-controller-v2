As this is an evolution of heater-controller project read that repository first since they have a lot of things in common: https://github.com/mbc99/heater-controller

In this case the project is split into two main components:
* A controller: that sits on the gas heater room and does the same function as heater_controller project except that:
	* It lacks (or doesn't need) an RTC
	* It has two RS232 lines (based on MAX3232 iC)
	* It lacks a screen and buttons
* A sensor/comunication module located on the living room. This module acomplishes several functions:
	* Sense the ambient temperature
	* Control when to activate the heating, DHW etc.
	* Send and recieve all the information via MQTT and a GSM module




## sensor/comunication module
The schematics as well as the PCB design are available here: https://easyeda.com/603mbastida/termoino

<p align="center">
  <img width="460" src="/images/image1.png">
</p>

As you can see there are more things to this board than what was first explained, the additional functional modules are:
* A female header for connecting a TFT LCD SPI display (not yet implemented in code)
* Two female headers for connected 7seg modules that are controlled with TM1637
* MAX3232 comunication iC for communicating with the heater-controller module
* iC related to the SIM808 GSM module:
	* An LDO for converting 5v to 4,2V
	* Buffers

As this module was going to be located on the living room a case was needed to house the PCB and the PSU. Fusion360 was used for this purpose:
<p align="center">
  <img width="460" src="/images/image2.png">
</p>

And this is the result:
<p align="center">
  <img width="460" src="/images/image3.jpg">
</p>

The module sends all the information to a RaspberryPi running Node-Red via MQTT. The main page to control the ambient temperature, DHW water temperature and monitor the system in general looks like this:
<p align="center">
  <img width="460" src="/images/image4.png">
</p>
<p align="center">
  <img width="750" src="/images/image5.png">
</p>

## Controller

As explained before the major change in this module is the addition of a MAX3232 RS232 comunication IC. 

The 3.81 mm plugable headers have been replaced by 2.54 mm screw terminals due to availability motives.

<p align="center">
  <img width="460" src="/images/image6.png">
</p>


The code was developed using Visual Studio Code with PlatformIO plugging to develop the code using the Arduino enviorment.




