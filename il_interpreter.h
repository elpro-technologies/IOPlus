/*
 * il_interpreter.h
 *
 * Instruction List Interpreter - As used in ELPRO Telemetry (IO Plus)
 * 
 * This is the core function providing programmable logic function in
 * the ELPRO 915, 415, 215, and 115 series products. 
 *
 * Created on: 20 Jun 2015
 *     Author: Harry Courtice
 */

/**********************************************************************
Copyright 2022 ELPRO Technologies 29 Lathe St, Virginia, QLD, Australia

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
************************************************************************/


#ifndef IL_INTERPRETER_H_
#define IL_INTERPRETER_H_

#include <stdint.h>

/* Memory call back structure provdes interface to memory get and set instrns
 * invert allows the memory interface to handle bit and word types correctly
 *  - saving a non-zero accumulator to a bit location should set the bit.
 *  - saving non-zero value with invert should clear the bit.
 *  - loading (get) a bit value gives 0 or 1.
 */
typedef struct{
	uint16_t (*get)(uint16_t address, bool invert);
	void (*set)(uint16_t address, uint16_t value, bool invert);
} il_memory_callbacks;

/* Initialise the interpreter with callbacks to 
 * allow the interpreter to manipulate the caller's
 * memory image.
 * 
 * @param cb - structure with the caller's set() and get()
 *             memory access functions
 */
void il_interp_init(il_memory_callbacks *cb);


/* Parse a command string to a 16-bit command code 
 * (actual encoding is private to il_interpreter.c)
 * 
 * @param - command string representing the command.
 * @return - 16-bit internal representaiton of the command
 */
uint16_t il_interp_parse(char * command);


/* Execute a line of the program, update the machine state 
 * and return the next line to execute.
 * 
 * @param - cmd   - The command code to execute 
 *                    (codes are private to il_interpreter.c)
 * @param - value - The Value parameter associated with the command
 * @param - line  - The current line number (for relative jumps)
 *
 * @return - The new line number according to the command and current line
 */
uint16_t il_interp_execute(uint16_t cmd, uint16_t value, uint16_t line);

/* Get the 16-bit accumulator value (for display and debug)
 * 
 * @return - the current accumulator value
 */
uint16_t il_interp_get_accum(void);



#endif /* IL_INTERPRETER_H_ */
