/*
 * il_interpreter.c
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

#include <stdbool.h>
#include <string.h>
#include "il_interpreter.h"

static il_memory_callbacks *mem;

static uint16_t accum;

static bool initialised = false;

/******************************************
 * Delayed Evaluation Stack. This supports
 * the '{' and '}' delayed evaluation 
 * functionality
 ******************************************/
#define EVAL_STACK_MAX_DEPTH 20

struct{
	uint16_t command;
	uint16_t accum;
} eval_stack[EVAL_STACK_MAX_DEPTH];

static int eval_stack_top = 0;

/* Push a command and current accumulator value onto the 
 * evaluation stack for delayed execution at the closing '}'
 *
 * @param cmd   - the 16-bit command code being executed
 * @param accum - the current accumulator value to use for delayed execution
 */
static void eval_stack_push(uint16_t cmd, uint16_t accum){
	if(eval_stack_top < EVAL_STACK_MAX_DEPTH){
		eval_stack[eval_stack_top].accum = accum;
		eval_stack[eval_stack_top].command = cmd;
	}
	eval_stack_top++;
}

/* Pop a saved command and accumulator value from the 
 * evaluation stack for execution at the closing '}'
 * 
 * @param cmd   - pointer to store the saved 16-bit command code
 * @param accum - pointer to store the saved accumulator value
 * 
 * @return - true if valid stack. false if no valid stack state
 */
static bool eval_stack_pop(uint16_t* cmd, uint16_t* accum){
	if((eval_stack_top  == 0) || (eval_stack_top >= EVAL_STACK_MAX_DEPTH)){
		return false;
	}

	eval_stack_top--;
	*cmd   = eval_stack[eval_stack_top].command;
	*accum = eval_stack[eval_stack_top].accum;
	return true;
}

/****************************************
 * call stack implementation. This implements
 * the functionality to support CALL and RET
 * instructions.
 ****************************************/ 
#define CALL_STACK_MAX_DEPTH 20
uint16_t call_stack[CALL_STACK_MAX_DEPTH];
static int call_stack_top = 0;

/* Push a return address onto the call stack
 *
 * @param addr - The return address to save
 * @return - true if value was pushed. false if stack is already full.
 */
static bool call_stack_push(uint16_t addr){
	if(call_stack_top >= CALL_STACK_MAX_DEPTH)	return false;

	call_stack[call_stack_top] = addr;
	call_stack_top++;
	return true;
}

/* Pop the return address from the top of the call stack.
 *
 * @param retaddr - pointer to save the popped line number
 * @return - true if valid value. false if stack is empty
 */
static bool call_stack_pop(uint16_t * retaddr){
	if(call_stack_top <= 0)	return false;

	call_stack_top --;
	(* retaddr) = call_stack[call_stack_top];
	return true;
}

/********************************************
 * Interface functions
 ********************************************/

/* Initialise the interpreter with callbacks to 
 * allow the interpreter to manipulate the caller's
 * memory image.
 * 
 * @param cb - structure with the caller's set() and get()
 *             memory access functions
 */
void il_interp_init(il_memory_callbacks *cb){

	mem = cb;

	accum = 0;

	eval_stack_top = 0;

	call_stack_top = 0;

	initialised = true;
}

/* Get the accumulator value (for debug)
 * 
 * @return - the current accumulator value
 */
uint16_t il_interp_get_accum(void){
	return accum;
}

/******************************************************
 * Parsing the command string 
 ******************************************************/
/* Check if the start of a string matches a string */
static bool match(char * str1, char * str2){
	return(0 == strncmp(str1, str2, strlen(str1)));
}

/* Check if flag exists in a string after "_" separator */
static bool flag(char flag, char* str){
	return((str) && (str = strchr(str, '_')) && strchr(str,flag));
}
/* Command is represented as a 16-bit value 
 * bits 0-7 contain the command code. 
 * bits 8-16 contain flag bits (currently 8-11 used).
#define CMD_LOAD 1
#define CMD_STOR 2
#define CMD_SET  3
#define CMD_RST  4
#define CMD_AND  5
#define CMD_OR   6
#define CMD_XOR  7
#define CMD_ADD  8
#define CMD_SUB  9
#define CMD_MUL  10
#define CMD_DIV  11
#define CMD_GT   12
#define CMD_GE   13
#define CMD_EQ   14
#define CMD_NE   15
#define CMD_LE   16
#define CMD_LT   17
#define CMD_JMP  18
#define CMD_CAL  19
#define CMD_RET  20
#define CMD_PAR  21   // '}' - Closing Parenthesis for sub-calculation
#define CMD_NOP  0
#define CMD_MASK 0x00FF

#define FLG_IMM 0x1000  // 'I' - Immediate value flag
#define FLG_NEG 0x2000  // 'N' - Negate
#define FLG_CND 0x4000  // 'C' - Conditional for branch and call
#define FLG_PAR 0x8000  // '{' - Begin sub-calculation 

/* Parse a command string to a 16-bit command code
 * 
 * @param - command string representing the command.
 * @return - 16-bit internal representaiton of the command
 */
uint16_t il_interp_parse(char * command){
	uint32_t ret;

	if(match("LOAD",command)) { ret = CMD_LOAD;	} else
	if(match("STOR",command)) { ret = CMD_STOR;	} else
	if(match("SET", command)) { ret = CMD_SET;  } else
	if(match("RST", command)) { ret = CMD_RST;  } else
	if(match("AND", command)) { ret = CMD_AND;  } else
	if(match("OR",  command)) { ret = CMD_OR;   } else
	if(match("XOR", command)) { ret = CMD_XOR;  } else
	if(match("ADD", command)) { ret = CMD_ADD;  } else
	if(match("SUB", command)) { ret = CMD_SUB;  } else
	if(match("MUL", command)) { ret = CMD_MUL;  } else
	if(match("DIV", command)) { ret = CMD_DIV;  } else
	if(match("GT",  command)) { ret = CMD_GT;   } else
	if(match("GE",  command)) { ret = CMD_GE;   } else
	if(match("EQ",  command)) { ret = CMD_EQ;   } else
	if(match("NE",  command)) { ret = CMD_NE;   } else
	if(match("LE",  command)) { ret = CMD_LE;   } else
	if(match("LT",  command)) { ret = CMD_LT;   } else
	if(match("JUMP",command)) { ret = CMD_JMP;  } else
	if(match("CALL",command)) { ret = CMD_CAL;  } else
	if(match("RET", command)) { ret = CMD_RET;  } else
	if(match("}",   command)) { ret = CMD_PAR;  } else
	ret = CMD_NOP;

	if(flag('I', command)){ ret |= FLG_IMM; }
	if(flag('N', command)){ ret |= FLG_NEG; }
	if(flag('C', command)){ ret |= FLG_CND; }
	if(flag('{', command)){ ret |= FLG_PAR; }

	return ret;
}


static uint16_t evaluate_operator(uint16_t cmd, uint16_t op1, uint16_t op2){
	uint16_t ret = 0;

	// Special case code for LOAD_{ command Note: This
	// code will only be called when popping from
	// the execution stack. Code for regular LOAD
	// command is below in il_interp_execute(..)
	if((cmd & CMD_MASK) == CMD_LOAD){
		ret = mem->get(op2,FLG_NEG==(cmd&FLG_NEG));
		return ret;
	}
	// and for STOR_{ Command. Note: This code will
	// only be called when popping from the execution
	// Stack. Code for regular STOR command is below
	// in il_interp_execute(..)
	if((cmd & CMD_MASK) == CMD_STOR){
		ret = op1;
		mem->set(op2, op1,FLG_NEG==(cmd&FLG_NEG));
		return ret;
	}

	// Normal binary opertors. This is executed for both
	// normal and evaluation stack versions of these commands.
	if(cmd & FLG_NEG) op2 = ~op2;
	switch(cmd & CMD_MASK){
	case CMD_AND: ret = op1 & op2; break;
	case CMD_OR:  ret = op1 | op2; break;
	case CMD_XOR: ret = op1 ^ op2; break;
	case CMD_ADD: ret = op1 + op2; break;
	case CMD_SUB: ret = op1 - op2; break;
	case CMD_MUL: ret = op1 * op2; break;
	case CMD_DIV: ret = op1 / op2; break;
	case CMD_GT:  ret = op1 > op2; break;
	case CMD_GE:  ret = op1 >=op2; break;
	case CMD_EQ:  ret = op1 ==op2; break;
	case CMD_NE:  ret = op1 !=op2; break;
	case CMD_LE:  ret = op1 <=op2; break;
	case CMD_LT:  ret = op1 < op2; break;
	}
	return ret;
}


/* Execute a line of the program. Update the machine state 
 * and return the next line to execute.
 * 
 * @param - cmd   - The command code to execute (private to il_interpreter.c)
 * @param - value - The Value parameter associated with the command
 * @param - location - The current line number (for relative jumps)
 *
 * @return - The new line number according to the command and current line
 */
uint16_t il_interp_execute(uint16_t cmd, uint16_t location, uint16_t line){

	uint16_t value;
	uint16_t s_accum;
	uint16_t s_cmd;

	value = location; // This default for "I" flag and for STOR Cmd
	line += 1;        // safe to pre-increment the line number.
	                  // Some commands re-set this

	switch(cmd & CMD_MASK){

	case CMD_SET:
		if(	(!(cmd & FLG_NEG) &&  accum) ||
			( (cmd & FLG_NEG) && !accum) ){
			mem->set(location,1,false);
		}
		break;
	case CMD_RST:
		if(	(!(cmd & FLG_NEG) &&  accum) ||
			( (cmd & FLG_NEG) && !accum) ){
			mem->set(location,0,false);
		}
		break;
	case CMD_JMP:
	case CMD_RET:
	case CMD_CAL:
		if((cmd & FLG_CND) &&  // Conditional
		   ( ( (cmd & FLG_NEG) &&  accum) ||
			 (!(cmd & FLG_NEG) && !accum) ) ){
				break; // If condition fails, no action
		}
		switch(cmd & CMD_MASK){
		case CMD_JMP:
			line = location;   // Execute the jump
			break;
		case CMD_RET:
			if(!call_stack_pop(&line)){
				// Pop failed - No context on stack
				// Exit
				line = 65535;
			}
			break;
		case CMD_CAL:
			if(call_stack_push(line)){
				line = location;
			}
			// If no space on call stack, move to next line
			break;
		}
		break;
	case CMD_STOR:
	case CMD_LOAD:
		if(cmd & FLG_PAR){
			// Push the command and current accumulator
			// to the eval stack
			eval_stack_push(cmd, accum);
			// Now start the address calculation by loading the
			// value into the accumulator
			accum = location;
		} else {
			if((cmd & CMD_MASK) == CMD_STOR){
				mem->set(location, accum,FLG_NEG==(cmd&FLG_NEG));
			} else { // CMD_LOAD
				if(cmd & FLG_IMM) accum = location;
				else accum = mem->get(location,FLG_NEG==(cmd&FLG_NEG));
			}
		}
		break;
	case CMD_AND:
	case CMD_OR:
	case CMD_XOR:
	case CMD_ADD:
	case CMD_SUB:
	case CMD_MUL:
	case CMD_DIV:
	case CMD_GT:
	case CMD_GE:
	case CMD_EQ:
	case CMD_NE:
	case CMD_LE:
	case CMD_LT:
		if(cmd & FLG_IMM) value = location;
		else              value = mem->get(location,false);
		if(cmd & FLG_PAR){     // Delayed evaluation
			eval_stack_push(cmd, accum);
			accum = value;
		} else {// Normal evaluation
			accum = evaluate_operator(cmd, accum, value);
		}
		break;
	case CMD_PAR: // Close Parentheses "}"
		if(eval_stack_pop(&s_cmd, &s_accum)){
			accum = evaluate_operator(s_cmd, s_accum, accum);
		}
		break;
	case CMD_NOP:
		// Nothing to do - line is already incremented.
		break;
	}
	return line;
}





