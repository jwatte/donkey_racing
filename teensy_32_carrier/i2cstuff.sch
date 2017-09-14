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
Text HLabel 2000 4850 0    60   Input ~ 0
R_ID_SD
Text HLabel 2000 5050 0    60   Input ~ 0
R_ID_SC
Wire Wire Line
	2000 4850 3300 4850
Wire Wire Line
	2000 5050 3300 5050
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
P 3700 3950
F 0 "#PWR043" H 3700 3800 50  0001 C CNN
F 1 "+2V5" H 3700 4090 50  0000 C CNN
F 2 "" H 3700 3950 50  0000 C CNN
F 3 "" H 3700 3950 50  0000 C CNN
	1    3700 3950
	1    0    0    -1  
$EndComp
Wire Wire Line
	3700 4100 3700 3950
Wire Wire Line
	2350 4100 4600 4100
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
	4600 4100 4600 5150
Connection ~ 3700 4100
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
	3100 4100 3100 4700
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
	4200 4450 4200 4100
Connection ~ 4200 4100
$Comp
L CJ J15
U 1 1 59A24573
P 4200 5250
F 0 "J15" H 4200 5175 39  0000 C CNN
F 1 "CJ" H 4200 5325 39  0000 C CNN
F 2 "teensy:CJ-2" H 4200 5250 39  0001 C CNN
F 3 "" H 4200 5250 39  0000 C CNN
	1    4200 5250
	0    1    1    0   
$EndComp
Connection ~ 4200 4950
Wire Wire Line
	4200 5300 4200 5700
Connection ~ 4200 5700
$Comp
L RESISTOR_0402 R?
U 1 1 59B9C7A9
P 2350 4550
AR Path="/59B9C7A9" Ref="R?"  Part="1" 
AR Path="/59B9BC3F/59B9C7A9" Ref="R18"  Part="1" 
F 0 "R18" V 2300 4450 45  0000 L BNN
F 1 "MF-RES-0402-2K" V 2450 4300 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-R0402" H 2600 4460 20  0001 C CNN
F 3 "" H 2350 4550 60  0001 C CNN
	1    2350 4550
	1    0    0    -1  
$EndComp
Wire Wire Line
	2350 4750 2350 5050
Wire Wire Line
	2350 4100 2350 4350
Connection ~ 3100 4100
Connection ~ 2350 5050
$Comp
L RESISTOR_0402 R?
U 1 1 59B9C874
P 2700 4550
AR Path="/59B9C874" Ref="R?"  Part="1" 
AR Path="/59B9BC3F/59B9C874" Ref="R19"  Part="1" 
F 0 "R19" V 2650 4450 45  0000 L BNN
F 1 "MF-RES-0402-2K" V 2800 4300 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-R0402" H 2950 4460 20  0001 C CNN
F 3 "" H 2700 4550 60  0001 C CNN
	1    2700 4550
	1    0    0    -1  
$EndComp
Wire Wire Line
	2700 4350 2700 4100
Connection ~ 2700 4100
Wire Wire Line
	2700 4750 2700 4850
Connection ~ 2700 4850
Text HLabel 7050 2450 2    60   Input ~ 0
R_SDA
Text HLabel 7050 2350 2    60   Input ~ 0
R_SCL
$Comp
L PCF8563 U6
U 1 1 59B9DABF
P 6500 2550
F 0 "U6" H 6200 2900 50  0000 L CNN
F 1 "PCF8563" H 6600 2900 50  0000 L CNN
F 2 "teensy:TSSOP-8" H 6500 2550 50  0001 C CNN
F 3 "" H 6500 2550 50  0001 C CNN
	1    6500 2550
	1    0    0    -1  
$EndComp
Wire Wire Line
	7050 2350 6900 2350
Wire Wire Line
	7050 2450 6900 2450
Wire Wire Line
	6500 2950 6500 3250
Wire Wire Line
	3900 1950 4050 1950
Wire Wire Line
	4450 1950 6500 1950
$Comp
L Crystal Y1
U 1 1 59B9DE93
P 5950 2550
F 0 "Y1" H 5950 2700 50  0000 C CNN
F 1 "Crystal" H 5950 2400 50  0000 C CNN
F 2 "teensy:SC-20S" H 5950 2550 50  0001 C CNN
F 3 "" H 5950 2550 50  0001 C CNN
	1    5950 2550
	0    1    1    0   
$EndComp
Wire Wire Line
	5450 2350 6100 2350
Wire Wire Line
	5950 2350 5950 2400
Wire Wire Line
	5950 2750 6100 2750
Wire Wire Line
	5950 2750 5950 2700
$Comp
L GND #PWR044
U 1 1 59B9E0B7
P 6500 3250
F 0 "#PWR044" H 6500 3000 50  0001 C CNN
F 1 "GND" H 6500 3100 50  0000 C CNN
F 2 "" H 6500 3250 50  0000 C CNN
F 3 "" H 6500 3250 50  0000 C CNN
	1    6500 3250
	1    0    0    -1  
$EndComp
$Comp
L CAPACITOR_NP_0402 C14
U 1 1 59B9E32A
P 5050 2500
F 0 "C14" V 4950 2450 45  0000 L BNN
F 1 "MF-CAP-0402-0.1uF" V 5200 2100 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-C0402" H 5300 2410 20  0001 C CNN
F 3 "" H 5050 2500 60  0001 C CNN
	1    5050 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4750 3150 6500 3150
Connection ~ 6500 3150
$Comp
L +5V #PWR045
U 1 1 59B9E5AE
P 3500 1650
F 0 "#PWR045" H 3500 1500 50  0001 C CNN
F 1 "+5V" H 3500 1790 50  0000 C CNN
F 2 "" H 3500 1650 50  0001 C CNN
F 3 "" H 3500 1650 50  0001 C CNN
	1    3500 1650
	1    0    0    -1  
$EndComp
$Comp
L CP C2
U 1 1 59B9E5E6
P 4750 2500
F 0 "C2" H 4775 2600 50  0000 L CNN
F 1 "KR-5R5V334-R" V 4600 2200 50  0000 L CNN
F 2 "Capacitors_THT:C_Rect_L7.2mm_W11.0mm_P5.00mm_FKS2_FKP2_MKS2_MKP2" H 4788 2350 50  0001 C CNN
F 3 "" H 4750 2500 50  0001 C CNN
	1    4750 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	3500 1650 3500 1950
Connection ~ 4750 1950
Wire Wire Line
	4750 3150 4750 2650
$Comp
L DIODES_BAT42_SOD-123 D4
U 1 1 59B9E769
P 3800 1950
F 0 "D4" H 3800 1800 45  0000 L BNN
F 1 "MF-DIO-SOD123-BAT42" H 3650 2100 45  0000 L BNN
F 2 "MF_Discrete_Semiconductor:MF_Discrete_Semiconductor-SOD-123" H 3988 2029 20  0001 C CNN
F 3 "" H 3800 1950 60  0001 C CNN
	1    3800 1950
	1    0    0    -1  
$EndComp
Wire Wire Line
	3500 1950 3700 1950
Wire Wire Line
	6500 1950 6500 2150
$Comp
L CAPACITOR_NP_0402 C15
U 1 1 59B9E9E0
P 5450 2750
F 0 "C15" V 5350 2700 45  0000 L BNN
F 1 "MF-CAP-0402-8.2pF" V 5600 2350 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-C0402" H 5700 2660 20  0001 C CNN
F 3 "" H 5450 2750 60  0001 C CNN
	1    5450 2750
	1    0    0    -1  
$EndComp
Wire Wire Line
	5450 2350 5450 2650
Connection ~ 5950 2350
Wire Wire Line
	5450 2850 5450 3150
Connection ~ 5450 3150
$Comp
L RESISTOR_0402 R?
U 1 1 59B9F1B3
P 4250 1950
AR Path="/59B9F1B3" Ref="R?"  Part="1" 
AR Path="/59B9BC3F/59B9F1B3" Ref="R21"  Part="1" 
F 0 "R21" V 4200 1850 45  0000 L BNN
F 1 "MF-RES-0402-470" V 4350 1700 45  0000 L BNN
F 2 "MF_Passives:MF_Passives-R0402" H 4500 1860 20  0001 C CNN
F 3 "" H 4250 1950 60  0001 C CNN
	1    4250 1950
	0    1    1    0   
$EndComp
Wire Wire Line
	4750 2350 4750 1950
Wire Wire Line
	5050 2600 5050 3150
Connection ~ 5050 3150
Wire Wire Line
	5050 2400 5050 1950
Connection ~ 5050 1950
NoConn ~ 6900 2650
NoConn ~ 6900 2750
$EndSCHEMATC
