EESchema Schematic File Version 2
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
Sheet 4 4
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
L CONN_03x08 U6
U 1 1 59F812DB
P 8500 3600
F 0 "U6" H 8750 4300 60  0000 C CNN
F 1 "CONN_03x08" V 8900 3750 60  0000 C CNN
F 2 "teensy:CONN_03x08" H 8500 3600 60  0001 C CNN
F 3 "" H 8500 3600 60  0000 C CNN
	1    8500 3600
	1    0    0    -1  
$EndComp
Wire Wire Line
	8700 3200 8700 3700
Connection ~ 8700 3300
Connection ~ 8700 3400
Connection ~ 8700 3500
Connection ~ 8700 3600
Connection ~ 8700 3700
Connection ~ 8500 3700
Connection ~ 8500 3600
Connection ~ 8500 3500
Connection ~ 8500 3400
Connection ~ 8500 3300
Connection ~ 8500 3200
$Comp
L GND #PWR024
U 1 1 59F81478
P 8850 4200
F 0 "#PWR024" H 8850 3950 50  0001 C CNN
F 1 "GND" H 8850 4050 50  0000 C CNN
F 2 "" H 8850 4200 50  0001 C CNN
F 3 "" H 8850 4200 50  0001 C CNN
	1    8850 4200
	1    0    0    -1  
$EndComp
Wire Wire Line
	8500 3500 8500 3800
Wire Wire Line
	8500 3100 8500 3400
Wire Wire Line
	8700 3700 8850 3700
Wire Wire Line
	8850 3700 8850 4200
Wire Wire Line
	8100 3100 8050 3100
Wire Wire Line
	8050 2850 8050 3800
Wire Wire Line
	8700 3100 8950 3100
Wire Wire Line
	8950 3100 8950 2850
$Comp
L +5V #PWR025
U 1 1 59F8151A
P 8050 2850
F 0 "#PWR025" H 8050 2700 50  0001 C CNN
F 1 "+5V" H 8050 2990 50  0000 C CNN
F 2 "" H 8050 2850 50  0001 C CNN
F 3 "" H 8050 2850 50  0001 C CNN
	1    8050 2850
	1    0    0    -1  
$EndComp
$Comp
L +BATT #PWR026
U 1 1 59F81530
P 8950 2850
F 0 "#PWR026" H 8950 2700 50  0001 C CNN
F 1 "+BATT" H 8950 2990 50  0000 C CNN
F 2 "" H 8950 2850 50  0001 C CNN
F 3 "" H 8950 2850 50  0001 C CNN
	1    8950 2850
	1    0    0    -1  
$EndComp
Wire Wire Line
	8050 3800 8100 3800
Connection ~ 8050 3100
Wire Wire Line
	8700 3800 8600 3800
Wire Wire Line
	8600 3800 8600 3400
Wire Wire Line
	8600 3400 8500 3400
Wire Wire Line
	8100 3700 7350 3700
Wire Wire Line
	8100 3600 7350 3600
Wire Wire Line
	8100 3500 7350 3500
Wire Wire Line
	8100 3400 7350 3400
Wire Wire Line
	8100 3300 7350 3300
Wire Wire Line
	8100 3200 7350 3200
Text Label 7350 3200 0    60   ~ 0
OUT_STEER
Text Label 7350 3300 0    60   ~ 0
OUT_THROTTLE
Text Label 7350 3400 0    60   ~ 0
OUT_AUX
Text Label 7350 3500 0    60   ~ 0
IN_AUX
Text Label 7350 3600 0    60   ~ 0
IN_THROTTLE
Text Label 7350 3700 0    60   ~ 0
IN_STEER
Text HLabel 7350 3200 0    60   Input ~ 0
OUT_STEER
Text HLabel 7350 3300 0    60   Input ~ 0
OUT_THROTTLE
Text HLabel 7350 3400 0    60   Input ~ 0
OUT_AUX
Text HLabel 7350 3500 0    60   Input ~ 0
IN_AUX
Text HLabel 7350 3600 0    60   Input ~ 0
IN_THROTTLE
Text HLabel 7350 3700 0    60   Input ~ 0
IN_STEER
$Comp
L CONN_03x08 U5
U 1 1 59F81FC6
P 4050 3600
F 0 "U5" H 4300 4300 60  0000 C CNN
F 1 "CONN_03x08" V 4450 3750 60  0000 C CNN
F 2 "teensy:CONN_03x08" H 4050 3600 60  0001 C CNN
F 3 "" H 4050 3600 60  0000 C CNN
	1    4050 3600
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR027
U 1 1 59F820B2
P 4050 2450
F 0 "#PWR027" H 4050 2300 50  0001 C CNN
F 1 "+5V" H 4050 2590 50  0000 C CNN
F 2 "" H 4050 2450 50  0001 C CNN
F 3 "" H 4050 2450 50  0001 C CNN
	1    4050 2450
	1    0    0    -1  
$EndComp
Connection ~ 4050 3700
Connection ~ 4050 3600
Connection ~ 4050 3200
Connection ~ 4050 3300
Wire Wire Line
	4250 3100 4250 4500
Connection ~ 4250 3300
Connection ~ 4250 3400
Connection ~ 4250 3500
Connection ~ 4250 3600
Wire Wire Line
	3650 3200 2950 3200
Wire Wire Line
	3650 3300 2950 3300
Wire Wire Line
	3650 3400 2950 3400
Wire Wire Line
	3650 3500 2950 3500
Wire Wire Line
	3650 3600 2950 3600
Wire Wire Line
	3650 3700 2950 3700
Text Label 2950 3100 0    60   ~ 0
T_RX1
Text Label 2950 3200 0    60   ~ 0
T_TX1
Text Label 2950 3300 0    60   ~ 0
CTX
Text Label 2950 3400 0    60   ~ 0
CRX
Text Label 2950 3700 0    60   ~ 0
T_RX2
Text Label 2950 3800 0    60   ~ 0
T_TX2
Connection ~ 4050 3400
Connection ~ 4050 3500
Wire Wire Line
	4050 3500 4050 4050
Wire Wire Line
	4050 2450 4050 3400
Connection ~ 4250 3200
Connection ~ 4250 3700
Connection ~ 4250 3800
$Comp
L GND #PWR028
U 1 1 59F82B8B
P 4250 4500
F 0 "#PWR028" H 4250 4250 50  0001 C CNN
F 1 "GND" H 4250 4350 50  0000 C CNN
F 2 "" H 4250 4500 50  0001 C CNN
F 3 "" H 4250 4500 50  0001 C CNN
	1    4250 4500
	1    0    0    -1  
$EndComp
Connection ~ 4050 3100
Wire Wire Line
	3650 3100 2950 3100
Wire Wire Line
	3650 3800 2950 3800
Text Label 2950 3500 0    60   ~ 0
T_A8
Text Label 2950 3600 0    60   ~ 0
T_A9
Wire Wire Line
	4050 4050 4600 4050
Wire Wire Line
	4600 4050 4600 2450
Connection ~ 4050 3800
$Comp
L +3.3VP #PWR029
U 1 1 59F82FF0
P 4600 2450
F 0 "#PWR029" H 4750 2400 50  0001 C CNN
F 1 "+3.3VP" H 4600 2550 50  0000 C CNN
F 2 "" H 4600 2450 50  0001 C CNN
F 3 "" H 4600 2450 50  0001 C CNN
	1    4600 2450
	1    0    0    -1  
$EndComp
Text HLabel 2950 3100 0    60   Input ~ 0
T_RX1
Text HLabel 2950 3200 0    60   Input ~ 0
T_TX1
Text HLabel 2950 3300 0    60   Input ~ 0
T_CTX
Text HLabel 2950 3400 0    60   Input ~ 0
T_CRX
Text HLabel 2950 3500 0    60   Input ~ 0
T_A8
Text HLabel 2950 3600 0    60   Input ~ 0
T_A9
Text HLabel 2950 3700 0    60   Input ~ 0
T_RX2
Text HLabel 2950 3800 0    60   Input ~ 0
T_TX2
Wire Wire Line
	4250 4400 4750 4400
Connection ~ 4250 4400
Wire Wire Line
	4750 4400 4750 4300
$Comp
L PWR_FLAG #FLG030
U 1 1 59F8940E
P 4750 4300
F 0 "#FLG030" H 4750 4375 50  0001 C CNN
F 1 "PWR_FLAG" H 4750 4450 50  0000 C CNN
F 2 "" H 4750 4300 50  0001 C CNN
F 3 "" H 4750 4300 50  0001 C CNN
	1    4750 4300
	1    0    0    -1  
$EndComp
$EndSCHEMATC
