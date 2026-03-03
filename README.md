
This project is a high-speed, Formula 1-inspired Reaction Game developed for a Computer Architecture and Design course at the University of Naples Federico II.


🏎️ Project Overview: F1 Reaction Test
Inspired by the rigorous training of F1 pilots—specifically the lightning-fast reflexes required to activate and deactivate DRS—this system challenges two players to a head-to-head speed test. The goal is simple: be the first to hit your button when the "GO!" signal appears, measuring reaction times with millisecond precision.


🛠️ Hardware Requirements
The system uses a distributed architecture with three microcontrollers:


1x Master Node: STM32F401-RE Nucleo 64.


2x Player Nodes: STM32F303 Discovery boards.

Peripherals:

3x 0.96” OLED Displays (SSD1306) for real-time feedback.


2x Active Piezo Buzzers for auditory cues.

1x Green LED (Start Signal).


3x Breadboards, 330 Ohm resistors, and various jumpers (M-M/F-M).


📡 Communication Protocols
The project utilizes a multi-protocol approach to handle data and visuals:


UART (The Neural Network): Used for the primary communication between the Master and Player nodes. The Master sends commands (RESET, GO) and receive reaction timestamps through dedicated channels.



I2C (The Eyes): Each node uses the I2C protocol to drive its own OLED display, providing players with status updates like "READY!", "GO!", or "YOU WIN!".


🧠 Software Architecture
The system is built on a Finite State Machine (FSM) paradigm to ensure deterministic behavior and millisecond accuracy.


Master Node States

IDLE: Waiting for the Master user button to start a match.


WAITING: A randomized delay (1–10 seconds) to prevent players from anticipating the start.



LED_ON: The "GO!" signal is active; the Master awaits reaction data.


FINISHED: Calculates the winner and transmits results.


FALSE_START: Triggered if a player presses their button prematurely.

Player Node States

IDLE/READY: Sychronizing with the Master's reset command.


BUTTON_PRESSED: Captures the local timestamp and calculates the reaction delta.


SHOW_RESULT: Displays individual victory or defeat screens.

🏁 Key Features

Millisecond Precision: Accurate reaction time calculation.


Anti-Cheating: Randomized start delays and false start detection.


Multi-Modal Feedback: Visual (OLED/LED) and auditory (Buzzer) indicators.



Tie Handling: Experimental logic where a simultaneous press results in a double loss.

👥 The Team
Dott. Alberto Stravato 

Dott. Emanuele Santacroce 

Dott. Francesco Floriano 


Supervised by: Prof. Nicola Mazzocca
