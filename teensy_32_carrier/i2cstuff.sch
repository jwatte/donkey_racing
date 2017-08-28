EESchema Schematic File Version 2
LIBS:power
LIBS:device
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
LIBS:maxim
LIBS:teensy_32_carrier-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 3 3
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text HLabel 3000 4850 0    60   Input ~ 0
R_ID_SD
Text HLabel 3000 5050 0    60   Input ~ 0
R_ID_SC
Wire Wire Line
	3300 4850 3000 4850
Wire Wire Line
	3300 5050 3000 5050
$Comp
L GND #PWR042
U 1 1 59A2414F
P 3600 5900
F 0 "#PWR042" H 3600 5650 50  0001 C CNN
F 1 "GND" H 3600 5750 50  0000 C CNN
F 2 "" H 3600 5900 50  0000 C CNN
F 3 "" H 3600 5900 50  0000 C CNN
	1    3600 5900
	1    0    0    -1  
$EndComp
Wire Wire Line
	3600 5900 3600 5700
Wire Wire Line
	3100 5700 4600 5700
$Comp
L +2V5 #PWR043
U 1 1 59A241B6
P 3700 4250
F 0 "#PWR043" H 3700 4100 50  0001 C CNN
F 1 "+2V5" H 3700 4390 50  0000 C CNN
F 2 "" H 3700 4250 50  0000 C CNN
F 3 "" H 3700 4250 50  0000 C CNN
	1    3700 4250
	1    0    0    -1  
$EndComp
Wire Wire Line
	3700 4250 3700 4400
Wire Wire Line
	3100 4400 4600 4400
$Comp
L CAPACITOR_NP_0402 C13
U 1 1 59A241EC
P 4600 5250
F 0 "C13" V 4500 5200 45  0000 L BNN
F 1 "MF-CAP-0402-0.1uF" V 4750 4850 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-C0402" H 4850 5160 20  0001 C CNN
F 3 "" H 4600 5250 60  0001 C CNN
	1    4600 5250
	1    0    0    -1  
$EndComp
Wire Wire Line
	4600 4400 4600 5150
Connection ~ 3700 4400
Wire Wire Line
	4600 5700 4600 5350
Connection ~ 3600 5700
$Comp
L M24C32-5 U7
U 1 1 59A2435F
P 3700 4950
F 0 "U7" H 3700 5400 60  0000 C CNN
F 1 "M24C32-5" H 3700 4500 60  0000 C CNN
F 2 "teensy:UFDFPN5" H 3700 4950 60  0001 C CNN
F 3 "" H 3700 4950 60  0001 C CNN
	1    3700 4950
	1    0    0    -1  
$EndComp
Wire Wire Line
	3100 5700 3100 5200
Wire Wire Line
	3100 5200 3300 5200
Wire Wire Line
	3100 4400 3100 4700
Wire Wire Line
	3100 4700 3300 4700
$Comp
L RESISTOR_0402 R20
U 1 1 59A2446B
P 4200 4650
F 0 "R20" V 4100 4600 45  0000 L BNN
F 1 "MF-RES-0402-10K" V 4350 4150 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-R0402" H 4450 4560 20  0001 C CNN
F 3 "" H 4200 4650 60  0001 C CNN
	1    4200 4650
	1    0    0    -1  
$EndComp
Wire Wire Line
	4100 4950 4200 4950
Wire Wire Line
	4200 4850 4200 5200
Wire Wire Line
	4200 4450 4200 4400
Connection ~ 4200 4400
$Comp
L CJ J15
U 1 1 59A24573
P 4200 5250
F 0 "J15" H 4200 5175 39  0000 C CNN
F 1 "CJ" H 4200 5325 39  0000 C CNN
F 2 "teensy:CJ-2" H 4200 5250 39  0000 C CNN
F 3 "" H 4200 5250 39  0000 C CNN
	1    4200 5250
	0    1    1    0   
$EndComp
Connection ~ 4200 4950
Wire Wire Line
	4200 5300 4200 5700
Connection ~ 4200 5700
$EndSCHEMATC
