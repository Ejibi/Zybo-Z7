Each file in this represents an aspect of what the code is completing

The stepper files represent the stepper motor code or the functions that tell how the motor should operate.

The server code represents how the webpage functions and how the user can operate the motor by initializing the motor's start-up parameters.

The network files work to port the applications server file operation to a standard webpage using the IP address of the board, its network mask, and its gateway.

The GPIO files are to show extra applications to see if the mode used by the board is correctly seen in the hardware; the LED GPIOs will blink in unique patterns for each stepper motor mode to signify that the stepper is in wave drive, halfstep, full step, alongside the user being able to creat a emergency stop function using the button peripheral GPIOs.

And finally, Main.c has the task declarations, the specific commands executed to cause the motor to function and what to do in an emergency stop scenario.
