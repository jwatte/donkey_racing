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
Sheet 1 4
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
L TEENSY_3 U?
U 1 1 59ED5951
P 2000 4650
F 0 "U?" H 2200 3450 60  0000 C CNN
F 1 "TEENSY_3" H 2200 6150 60  0000 C CNN
F 2 "" H 2000 4650 60  0000 C CNN
F 3 "" H 2000 4650 60  0000 C CNN
	1    2000 4650
	1    0    0    -1  
$EndComp
$Comp
L Conn_02x20_Odd_Even P1
U 1 1 59ED637C
P 9450 3250
F 0 "P1" H 9500 4250 50  0000 C CNN
F 1 "S9200-ND" H 9500 2150 50  0000 C CNN
F 2 "teensy:RPI_BOTTOM_NOLABELS" H 9450 3250 50  0001 C CNN
F 3 "" H 9450 3250 50  0001 C CNN
	1    9450 3250
	1    0    0    -1  
$EndComp
$Sheet
S 1600 1000 950  1300
U 59ED6513
F0 "Power" 60
F1 "power.sch" 60
$EndSheet
$Sheet
S 4900 5900 1150 1550
U 59ED67C6
F0 "PiTeensyIO" 60
F1 "pi-teensy-io.sch" 60
F2 "R_SDA" I R 6050 6250 60 
F3 "R_SCL" I R 6050 6400 60 
F4 "R_TX" I R 6050 6600 60 
F5 "R_RX" I R 6050 6750 60 
F6 "T_SDA" I L 4900 6250 60 
F7 "T_SCL" I L 4900 6400 60 
F8 "T_RX3" I L 4900 6600 60 
F9 "T_TX3" I L 4900 6750 60 
$EndSheet
$Sheet
S 5800 1000 1150 1500
U 59ED6859
F0 "RCIO" 60
F1 "rc-io.sch" 60
$EndSheet
$Comp
L +5V #PWR?
U 1 1 59ED68B8
P 9900 2000
F 0 "#PWR?" H 9900 1850 50  0001 C CNN
F 1 "+5V" H 9900 2140 50  0000 C CNN
F 2 "" H 9900 2000 50  0001 C CNN
F 3 "" H 9900 2000 50  0001 C CNN
	1    9900 2000
	1    0    0    -1  
$EndComp
$Comp
L +5V #PWR?
U 1 1 59ED68D0
P 1150 3000
F 0 "#PWR?" H 1150 2850 50  0001 C CNN
F 1 "+5V" H 1150 3140 50  0000 C CNN
F 2 "" H 1150 3000 50  0001 C CNN
F 3 "" H 1150 3000 50  0001 C CNN
	1    1150 3000
	1    0    0    -1  
$EndComp
$Comp
L +3V3 #PWR?
U 1 1 59ED68F4
P 8600 2000
F 0 "#PWR?" H 8600 1850 50  0001 C CNN
F 1 "+3V3" H 8600 2140 50  0000 C CNN
F 2 "" H 8600 2000 50  0001 C CNN
F 3 "" H 8600 2000 50  0001 C CNN
	1    8600 2000
	1    0    0    -1  
$EndComp
NoConn ~ 1400 5150
NoConn ~ 1400 3950
$Comp
L GND #PWR?
U 1 1 59ED6925
P 1100 6100
F 0 "#PWR?" H 1100 5850 50  0001 C CNN
F 1 "GND" H 1100 5950 50  0000 C CNN
F 2 "" H 1100 6100 50  0001 C CNN
F 3 "" H 1100 6100 50  0001 C CNN
	1    1100 6100
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59ED6960
P 8400 4700
F 0 "#PWR?" H 8400 4450 50  0001 C CNN
F 1 "GND" H 8400 4550 50  0000 C CNN
F 2 "" H 8400 4700 50  0001 C CNN
F 3 "" H 8400 4700 50  0001 C CNN
	1    8400 4700
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59ED6978
P 10450 4700
F 0 "#PWR?" H 10450 4450 50  0001 C CNN
F 1 "GND" H 10450 4550 50  0000 C CNN
F 2 "" H 10450 4700 50  0001 C CNN
F 3 "" H 10450 4700 50  0001 C CNN
	1    10450 4700
	1    0    0    -1  
$EndComp
Text Label 6200 6250 0    60   ~ 0
R_SDA
Text Label 6200 6400 0    60   ~ 0
R_SCL
Text Label 6200 6600 0    60   ~ 0
R_TX
Text Label 6200 6750 0    60   ~ 0
R_RX
Text Label 4500 6750 0    60   ~ 0
T_TX3
Text Label 4500 6600 0    60   ~ 0
T_RX3
Text Label 4500 6400 0    60   ~ 0
T_SCL
Text Label 4500 6250 0    60   ~ 0
T_SDA
Text Label 9850 2650 0    60   ~ 0
R_TX
Text Label 9850 2750 0    60   ~ 0
R_RX
Text Label 8900 2450 0    60   ~ 0
R_SDA
Text Label 8900 2550 0    60   ~ 0
R_SCL
Wire Wire Line
	9750 2350 9900 2350
Wire Wire Line
	9900 2000 9900 2450
Wire Wire Line
	9900 2450 9750 2450
Connection ~ 9900 2350
Wire Wire Line
	1150 3000 1150 3450
Wire Wire Line
	1150 3450 1400 3450
Wire Wire Line
	9250 2350 8600 2350
Wire Wire Line
	8600 2000 8600 3150
Wire Wire Line
	1400 5550 1100 5550
Wire Wire Line
	1100 5550 1100 6100
Wire Wire Line
	9250 2750 8400 2750
Wire Wire Line
	8400 2750 8400 4700
Wire Wire Line
	9750 2550 10450 2550
Wire Wire Line
	10450 2550 10450 4700
Wire Wire Line
	6050 6250 6600 6250
Wire Wire Line
	6050 6400 6600 6400
Wire Wire Line
	6050 6600 6600 6600
Wire Wire Line
	6050 6750 6600 6750
Wire Wire Line
	4400 6750 4900 6750
Wire Wire Line
	4400 6600 4900 6600
Wire Wire Line
	4400 6400 4900 6400
Wire Wire Line
	4400 6250 4900 6250
Wire Wire Line
	9750 2650 10200 2650
Wire Wire Line
	9750 2750 10200 2750
Wire Wire Line
	8800 2450 9250 2450
Wire Wire Line
	8800 2550 9250 2550
Wire Wire Line
	8600 3150 9250 3150
Connection ~ 8600 2350
Wire Wire Line
	9250 3550 8400 3550
Connection ~ 8400 3550
Wire Wire Line
	9250 4250 8400 4250
Connection ~ 8400 4250
Wire Wire Line
	9750 3950 10450 3950
Connection ~ 10450 3950
Wire Wire Line
	9750 3750 10450 3750
Connection ~ 10450 3750
Wire Wire Line
	9750 3250 10450 3250
Connection ~ 10450 3250
Wire Wire Line
	9750 2950 10450 2950
Connection ~ 10450 2950
Wire Wire Line
	3000 5150 3550 5150
Wire Wire Line
	3000 5250 3550 5250
Text Label 3150 5150 0    60   ~ 0
T_SDA
Text Label 3150 5250 0    60   ~ 0
T_SCL
Wire Wire Line
	3550 4050 3000 4050
Wire Wire Line
	3550 4150 3000 4150
Text Label 3100 4050 0    60   ~ 0
T_RX3
Text Label 3100 4150 0    60   ~ 0
T_TX3
Wire Wire Line
	3550 4950 3000 4950
Text Label 3150 4950 0    60   ~ 0
PWR_SENSE
Text Label 3150 5050 0    60   ~ 0
PWR_CTL
Wire Wire Line
	3550 5050 3000 5050
$EndSCHEMATC
