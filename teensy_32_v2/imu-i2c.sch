EESchema Schematic File Version 2
LIBS:teensy_32_v2-rescue
LIBS:power
LIBS:device
LIBS:switches
LIBS:relays
LIBS:motors
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:teensy_3
LIBS:MF_Aesthetics
LIBS:MF_Connectors
LIBS:MF_Discrete_Semiconductor
LIBS:MF_Displays
LIBS:MF_Frequency_Control
LIBS:MF_IC_Analog
LIBS:MF_IC_Digital
LIBS:MF_IC_Power
LIBS:MF_LEDs
LIBS:MF_Passives
LIBS:MF_Sensors
LIBS:MF_Switches
LIBS:teensy_32_v2-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 5 5
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Conn_01x04_Female J11
U 1 1 5A8B37CB
P 5300 2600
F 0 "J11" H 5300 2800 50  0000 C CNN
F 1 "Conn_01x04_Female" H 5300 2300 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x04_Pitch2.54mm" H 5300 2600 50  0001 C CNN
F 3 "" H 5300 2600 50  0001 C CNN
	1    5300 2600
	1    0    0    -1  
$EndComp
$Comp
L +3V3 #PWR037
U 1 1 5A8B383F
P 4600 2100
F 0 "#PWR037" H 4600 1950 50  0001 C CNN
F 1 "+3V3" H 4600 2240 50  0000 C CNN
F 2 "" H 4600 2100 50  0001 C CNN
F 3 "" H 4600 2100 50  0001 C CNN
	1    4600 2100
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR038
U 1 1 5A8B3855
P 4600 3450
F 0 "#PWR038" H 4600 3200 50  0001 C CNN
F 1 "GND" H 4600 3300 50  0000 C CNN
F 2 "" H 4600 3450 50  0001 C CNN
F 3 "" H 4600 3450 50  0001 C CNN
	1    4600 3450
	1    0    0    -1  
$EndComp
Text HLabel 3600 2700 0    60   BiDi ~ 0
SCL
Text HLabel 3600 2800 0    60   BiDi ~ 0
SDA
$Comp
L Conn_01x08_Female J12
U 1 1 5A8B70FA
P 8200 2550
F 0 "J12" H 8200 2950 50  0000 C CNN
F 1 "Conn_01x08_Female" H 8200 2050 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_1x08_Pitch2.54mm" H 8200 2550 50  0001 C CNN
F 3 "" H 8200 2550 50  0001 C CNN
	1    8200 2550
	1    0    0    -1  
$EndComp
Wire Wire Line
	5100 2500 4600 2500
Wire Wire Line
	4600 2500 4600 2100
Wire Wire Line
	5100 2600 4600 2600
Wire Wire Line
	4600 2600 4600 3450
Wire Wire Line
	5100 2700 3600 2700
Wire Wire Line
	5100 2800 3600 2800
Wire Wire Line
	8000 2250 7700 2250
Wire Wire Line
	7700 2250 7700 1750
Wire Wire Line
	8000 2350 7700 2350
Wire Wire Line
	7700 2350 7700 3250
$Comp
L +3V3 #PWR039
U 1 1 5A8B7233
P 7700 1750
F 0 "#PWR039" H 7700 1600 50  0001 C CNN
F 1 "+3V3" H 7700 1890 50  0000 C CNN
F 2 "" H 7700 1750 50  0001 C CNN
F 3 "" H 7700 1750 50  0001 C CNN
	1    7700 1750
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR040
U 1 1 5A8B724B
P 7700 3250
F 0 "#PWR040" H 7700 3000 50  0001 C CNN
F 1 "GND" H 7700 3100 50  0000 C CNN
F 2 "" H 7700 3250 50  0001 C CNN
F 3 "" H 7700 3250 50  0001 C CNN
	1    7700 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	8000 2450 7400 2450
Wire Wire Line
	8000 2550 7400 2550
Wire Wire Line
	8000 2650 7400 2650
Wire Wire Line
	8000 2750 7400 2750
Wire Wire Line
	8000 2850 7400 2850
Wire Wire Line
	8000 2950 7400 2950
Text HLabel 7400 2450 0    60   Output ~ 0
R_MOSI
Text HLabel 7400 2550 0    60   BiDi ~ 0
R_25
Text HLabel 7400 2650 0    60   Input ~ 0
R_MISO
Text HLabel 7400 2750 0    60   Output ~ 0
R_CE0
Text HLabel 7400 2850 0    60   Output ~ 0
R_SCK
Text HLabel 7400 2950 0    60   Output ~ 0
R_CE1
$EndSCHEMATC
