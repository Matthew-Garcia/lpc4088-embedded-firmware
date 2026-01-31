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

## Disclaimer
This project was originally developed as part of a university microprocessor
interfacing course. The implementation, refactoring, and documentation
presented here are my own work.
