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
Sheet 4 5
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
L GND #PWR031
U 1 1 59F81478
P 9150 4300
F 0 "#PWR031" H 9150 4050 50  0001 C CNN
F 1 "GND" H 9150 4150 50  0000 C CNN
F 2 "" H 9150 4300 50  0001 C CNN
F 3 "" H 9150 4300 50  0001 C CNN
	1    9150 4300
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR032
U 1 1 59F8151A
P 7950 2450
F 0 "#PWR032" H 7950 2300 50  0001 C CNN
F 1 "+5V" H 7950 2590 50  0000 C CNN
F 2 "" H 7950 2450 50  0001 C CNN
F 3 "" H 7950 2450 50  0001 C CNN
	1    7950 2450
	1    0    0    -1  
$EndComp
Text Label 7350 3100 0    60   ~ 0
OUT_STEER
Text Label 7350 3200 0    60   ~ 0
OUT_THROTTLE
Text Label 7350 3300 0    60   ~ 0
OUT_AUX
Text Label 7350 3600 0    60   ~ 0
IN_AUX
Text Label 7350 3700 0    60   ~ 0
IN_THROTTLE
Text Label 7350 3800 0    60   ~ 0
IN_STEER
Text HLabel 7350 3100 0    60   Input ~ 0
OUT_STEER
Text HLabel 7350 3200 0    60   Input ~ 0
OUT_THROTTLE
Text HLabel 7350 3300 0    60   Input ~ 0
OUT_AUX
Text HLabel 7350 3600 0    60   Input ~ 0
IN_AUX
Text HLabel 7350 3700 0    60   Input ~ 0
IN_THROTTLE
Text HLabel 7350 3800 0    60   Input ~ 0
IN_STEER
$Comp
L +5V #PWR033
U 1 1 59F820B2
P 4550 2550
F 0 "#PWR033" H 4550 2400 50  0001 C CNN
F 1 "+5V" H 4550 2690 50  0000 C CNN
F 2 "" H 4550 2550 50  0001 C CNN
F 3 "" H 4550 2550 50  0001 C CNN
	1    4550 2550
	1    0    0    -1  
$EndComp
Text Label 3300 2800 0    60   ~ 0
T_RX1
Text Label 3300 2700 0    60   ~ 0
T_TX1
Text Label 3300 3100 0    60   ~ 0
CTX
Text Label 3300 3200 0    60   ~ 0
CRX
Text Label 5100 3200 2    60   ~ 0
T_RX2
Text Label 5100 3100 2    60   ~ 0
T_TX2
$Comp
L GND #PWR034
U 1 1 59F82B8B
P 3800 4500
F 0 "#PWR034" H 3800 4250 50  0001 C CNN
F 1 "GND" H 3800 4350 50  0000 C CNN
F 2 "" H 3800 4500 50  0001 C CNN
F 3 "" H 3800 4500 50  0001 C CNN
	1    3800 4500
	1    0    0    -1  
$EndComp
Text Label 5100 2800 2    60   ~ 0
T_A8
Text Label 5100 2700 2    60   ~ 0
T_A9
Text HLabel 3250 2800 0    60   Input ~ 0
T_RX1
Text HLabel 3250 2700 0    60   Input ~ 0
T_TX1
Text HLabel 3250 3100 0    60   Input ~ 0
T_CTX
Text HLabel 3250 3200 0    60   Input ~ 0
T_CRX
Text HLabel 5150 2800 2    60   Input ~ 0
T_A8
Text HLabel 5150 2700 2    60   Input ~ 0
T_A9
Text HLabel 5150 3200 2    60   Input ~ 0
T_RX2
Text HLabel 5150 3100 2    60   Input ~ 0
T_TX2
$Comp
L CONN_03x08 J9
U 1 1 59F812DB
P 8500 3600
F 0 "J9" H 8750 4300 60  0000 C CNN
F 1 "TSW-108-07-G-T" H 8550 3200 60  0000 C CNN
F 2 "teensy:CONN_03x08" H 8500 3600 60  0001 C CNN
F 3 "" H 8500 3600 60  0000 C CNN
	1    8500 3600
	1    0    0    -1  
$EndComp
Text HLabel 3250 4000 0    60   Input ~ 0
T_SDA
Text HLabel 3250 4100 0    60   Input ~ 0
T_SCL
Text HLabel 5150 4100 2    60   Input ~ 0
T_RX3
Text HLabel 5150 4000 2    60   Input ~ 0
T_TX3
Text Label 3300 4000 0    60   ~ 0
T_SDA
Text Label 3300 4100 0    60   ~ 0
T_SCL
Text Label 5100 4100 2    60   ~ 0
T_RX3
Text Label 5100 4000 2    60   ~ 0
T_TX3
$Comp
L Conn_02x08_Odd_Even J7
U 1 1 5A1A7D12
P 4150 3000
F 0 "J7" H 4200 3400 50  0000 C CNN
F 1 "M20-9980845" H 4200 2500 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x08_Pitch2.54mm" H 4150 3000 50  0001 C CNN
F 3 "" H 4150 3000 50  0001 C CNN
	1    4150 3000
	1    0    0    -1  
$EndComp
Connection ~ 8500 3700
Connection ~ 8500 3600
Connection ~ 8500 3500
Connection ~ 8500 3400
Connection ~ 8500 3300
Connection ~ 8500 3200
Wire Wire Line
	8500 3500 8500 3800
Wire Wire Line
	8500 3100 8500 3400
Wire Wire Line
	9150 3300 9150 4300
Wire Wire Line
	7950 2450 7950 3500
Wire Wire Line
	9300 2600 9300 3500
Wire Wire Line
	8100 3800 7350 3800
Wire Wire Line
	8100 3700 7350 3700
Wire Wire Line
	8100 3600 7350 3600
Wire Wire Line
	8100 3300 7350 3300
Wire Wire Line
	8100 3200 7350 3200
Wire Wire Line
	8100 3100 7350 3100
Wire Wire Line
	3950 2800 3250 2800
Wire Wire Line
	3950 3100 3250 3100
Wire Wire Line
	3950 3200 3250 3200
Wire Wire Line
	4450 2700 5150 2700
Wire Wire Line
	4450 2800 5150 2800
Wire Wire Line
	4450 3100 5150 3100
Wire Wire Line
	3950 2700 3250 2700
Wire Wire Line
	4450 3200 5150 3200
Wire Wire Line
	7950 3400 8100 3400
Wire Wire Line
	7950 3500 8100 3500
Connection ~ 7950 3400
Wire Wire Line
	8700 3800 9150 3800
Wire Wire Line
	8700 3600 8700 3800
Connection ~ 8700 3700
Wire Wire Line
	8700 3300 9150 3300
Connection ~ 9150 3800
Wire Wire Line
	8700 3100 8700 3300
Wire Wire Line
	9300 3400 8700 3400
Wire Wire Line
	9300 3500 8700 3500
Connection ~ 9300 3400
Wire Wire Line
	3950 4000 3250 4000
Wire Wire Line
	3950 4100 3250 4100
Wire Wire Line
	4450 4000 5150 4000
Wire Wire Line
	4450 4100 5150 4100
Wire Wire Line
	4550 3300 4550 2550
Wire Wire Line
	3950 2900 4550 2900
Connection ~ 4450 2900
Wire Wire Line
	3950 3300 4550 3300
Connection ~ 4550 2900
Connection ~ 4450 3300
Wire Wire Line
	3800 3000 3800 4500
Wire Wire Line
	3800 3400 4450 3400
Connection ~ 3950 3400
Wire Wire Line
	3800 3000 4450 3000
Connection ~ 3800 3400
Connection ~ 3950 3000
$Comp
L Conn_02x04_Odd_Even J8
U 1 1 5A1A81E2
P 4150 4100
F 0 "J8" H 4200 4300 50  0000 C CNN
F 1 "M20-9970445" H 4200 3800 50  0000 C CNN
F 2 "Pin_Headers:Pin_Header_Straight_2x04_Pitch2.54mm" H 4150 4100 50  0001 C CNN
F 3 "" H 4150 4100 50  0001 C CNN
	1    4150 4100
	1    0    0    -1  
$EndComp
Connection ~ 4550 3300
Wire Wire Line
	3800 4300 4450 4300
Connection ~ 3800 4300
Connection ~ 3950 4300
Connection ~ 8700 3200
Connection ~ 8700 3600
Connection ~ 8700 3800
Connection ~ 8500 3800
Connection ~ 8500 3100
Connection ~ 8700 3100
Connection ~ 8700 3300
Connection ~ 8700 3400
Connection ~ 8700 3500
$Comp
L +12V #PWR035
U 1 1 5A1C2C5E
P 9300 2600
F 0 "#PWR035" H 9300 2450 50  0001 C CNN
F 1 "+12V" H 9300 2740 50  0000 C CNN
F 2 "" H 9300 2600 50  0001 C CNN
F 3 "" H 9300 2600 50  0001 C CNN
	1    9300 2600
	1    0    0    -1  
$EndComp
Wire Wire Line
	3950 4200 4700 4200
Wire Wire Line
	4700 4200 4700 3750
Connection ~ 4450 4200
$Comp
L +3V3 #PWR036
U 1 1 5A495654
P 4700 3750
F 0 "#PWR036" H 4700 3600 50  0001 C CNN
F 1 "+3V3" H 4700 3890 50  0000 C CNN
F 2 "" H 4700 3750 50  0001 C CNN
F 3 "" H 4700 3750 50  0001 C CNN
	1    4700 3750
	1    0    0    -1  
$EndComp
$EndSCHEMATC
