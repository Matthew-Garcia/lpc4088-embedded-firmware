# LPC4088 Embedded Firmware System

## Overview
Bare-metal embedded firmware developed in C for an NXP LPC4088 microcontroller.  
The system integrates multiple peripherals over I²C and implements a multi-mode
embedded application including real-time clock display, alarm functionality,
temperature monitoring, keypad input, and calculator operation.

## Hardware Platform
- NXP LPC4088 microcontroller
- DS1337 Real-Time Clock (I²C)
- DS1631 Temperature Sensor (I²C)
- PCF8574T I²C expander with 4×20 LCD
- Matrix keypad

## Firmware Architecture
- Bare-metal C
- Modular peripheral drivers (I²C, LCD, RTC, temperature, keypad)
- Finite-state machine for mode control
- Timer/interrupt-based real-time behavior

## Key Engineering Challenges
- Managing multiple peripherals on a shared I²C bus
- LCD timing and control through an I²C expander
- Input validation and mode switching
- Debugging communication and timing issues on real hardware

## Features
- Real-time clock display (DS1337) with user-configurable time/date (12h -> 24h conversion)
- Alarm1 set + expiry handling with LED blink + user acknowledge
- Temperature readout (DS1631) in C/F
- 4x20 LCD via PCF8574T I2C expander (4-bit mode)
- 4x4 keypad UI + calculator mode (press F to toggle)

## Pin Map (LPC4088)
- I2C: p9 (SDA), p10 (SCL)
- Keypad rows: p17, p18, p19, p20
- Keypad cols: p13, p14, p15, p16 (PullUp)
- Alarm LED: p25


## Disclaimer
This project was originally developed as part of a university microprocessor
interfacing course. The implementation, refactoring, and documentation
presented here are my own work.
