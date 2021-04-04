# Drumidy
Midi drum module based on STM32 Nucleo

See the full project info at https://hackaday.io/project/176712-drumidy-electronic-midi-drum-controller

This project is a implementation of midi drum module using STM32 G431 nucleo board.
The code is is written using CubeIDE v1.6.0
The firmware uses USB port on pins PA11 and PA12 for the MIDI output and the integrated USB of the board as a configuration and debugging interface.
sending any char into the nucleo USB activates configuration routine.

in order to use all 18 inputs, the nucleo board needs following jumper modification:
Remove SB2 and SB3 (default I2C port for emulating Arduino Nano)
Add SB8 and SB11 (external clock option)
