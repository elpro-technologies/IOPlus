/*
 * gtk_demo.c
 *
 * Demo application for Instruction List Interpreter il_interpreter.c
 * Using GTK version 3.8.1 for Windows 
 * Compiled with 
 *
 * Created on: 14 Jun 2015
 *      Author: Harry Courtice / ELPRO Technologies
 *
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



#include <gtk/gtk.h>
#include <stdint.h>
#include <stdbool.h>
#include <synchapi.h>
#include "il_interpreter.h"


/***************************************************************
 * Display data structures for GTK commands
 ***************************************************************/

/* menudata[]
 * Encodes the IL commands in a format suitable for bulding a GTK menu. 
 */

struct{
	char * text;
	gboolean sub;
} menudata[] = {
	{"_",FALSE},
	{"LOAD", FALSE},
		{"LOAD_N", TRUE}, {"LOAD_I", TRUE}, 
		{"LOAD_{", TRUE}, {"LOAD_N{", TRUE},
	{"STOR", FALSE},
		{"STOR_N", TRUE}, {"STOR_{", TRUE}, {"STOR_N{", TRUE},
	{"SET",FALSE},
	{"RST",FALSE},
	{"AND",FALSE},	
		{"AND_N",TRUE},	{"AND_I",TRUE},	
		{"AND_{",TRUE},	{"AND_N{",TRUE},
	{"OR", FALSE},	
		{"OR_N", TRUE},	{"OR_I", TRUE},	
		{"OR_{", TRUE},	{"OR_N{", TRUE},
	{"XOR", FALSE},	
		{"XOR_I",TRUE}, {"XOR_{", TRUE},
	{"ADD", FALSE},	
		{"ADD_I", TRUE},{"ADD_{", TRUE},
	{"SUB", FALSE},	
		{"SUB_I", TRUE},{"SUB_{", TRUE},
	{"MUL", FALSE},	
		{"MUL_I", TRUE},{"MUL_{", TRUE},
	{"DIV", FALSE},	
		{"DIV_I", TRUE},{"DIV_{", TRUE},
	{"GT", FALSE},	
		{"GT_I", TRUE}, {"GT_{", TRUE},
	{"GE", FALSE},	
		{"GE_I", TRUE}, {"GE_{", TRUE},
	{"EQ", FALSE},	
		{"EQ_I", TRUE},	{"EQ_{", TRUE},
	{"NE", FALSE},	
		{"NE_I", TRUE},	{"NE_{", TRUE},
	{"LE", FALSE},  
		{"LE_I", TRUE},	{"LE_{", TRUE},
	{"LT", FALSE},	
		{"LT_I", TRUE},	{"LT_{", TRUE},
	{"JUMP", FALSE},
		{"JUMP_C", TRUE}, {"JUMP_CN", TRUE},
	{"CALL", FALSE},
		{"CALL_C", TRUE}, {"CALL_CN", TRUE},
	{"RET", FALSE},	
		{"RET_C", TRUE},  {"RET_CN", TRUE},
	{"}",FALSE}
};

/* CreateCommands() 
 * Create a GtkTreeModel menu tree containing all of the 
 * instructions listed in menu[] above.
 * This is used select the instruction to execute.
 *
 * @return - The menu tree in GtkTreeModel format
 */

static GtkTreeModel* CreateCommands(void){

	GtkTreeStore * store;
	GtkTreeIter iter, subitem;
	int i;

	store = gtk_tree_store_new(1,G_TYPE_STRING);

	for(i = 0; i < sizeof(menudata)/sizeof(menudata[0]); i++){
		if(!(menudata[i].sub) || (i == 0)){
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter, 0, menudata[i].text,-1);
		} else {
			gtk_tree_store_append(store, &subitem, & iter);
			gtk_tree_store_set(store, &subitem, 0, menudata[i].text, -1);
		}
	}
	return GTK_TREE_MODEL(store);

}

/******************************************************
 * Storage for the Program Accumulator and Memory.
 * 
 ******************************************************/

#define NUM_LINES 31   // The total number of program lines
#define MEM_SIZE 26    // The total number of memory elements of each type

/* program - a single line of the the IL program */
struct {
	GtkWidget * label;    // Label for the line - 0 .. NUM_LINES-1
	GtkWidget * command;  // The actual command - the menudata[] selection
	GtkWidget * value;    // The associated value as a spin-edit
} program[NUM_LINES];

/* Memory implemented as Gtk objects for display and editing
 * - 2 sets of bit memory 0xxxx and 1xxxx
 * - 2 sets of word memory 3xxxx and 4xxxx
 */
GtkToggleButton * bit_memory[2][MEM_SIZE];
GtkSpinButton * word_memory[2][MEM_SIZE];
GtkSpinButton * accum;

/* memory access for the IL interpreter library. This implements
 * two callbacks to allow the IL library to read and write memory.
 *  mem_set - Set a memory location to a value
 *  mem_get - Get a value from a memory location
 */

/* addr_decode
 * Turn a Modbus type address into a row and column to access
 * the bit_memory and word_memory arrays
 * @param [in ] addr - The modbus style address 0xxxx, 1xxxx, 3xxxx, 4xxxx
 * @param [out] col  - The "column" 
 *                       0 -> bit_memory[0], 1->bit_memory[1]
 *                       2 -> word_memory[0], 3->word_memory[1]
 * @param [out] row  - the row number in memory array
 *
 * @return - true if valid address else false
 */
bool addr_decode(uint16_t addr, int * col, int * row){
	int tcol = addr / 10000;
	int trow = (addr % 10000);
	if(trow == 0 || (trow > MEM_SIZE)) return false;
	trow -= 1;
	switch(tcol){
	case 0: case 1: break;
	case 3: case 4: tcol--; break;
	default: return false;
	}
	*col = tcol;
	*row = trow;
	return true;
}

/* mem_set - Set memory callback for il_interpreter
 * Set a memory address to desired value (optional invert).
 * Bit addresses are set to 1 or zero depending on the 
 * requested value. Invert is implemented bitwise. Invalid
 * addresses are ignored.
 *
 * @param[in] addr - the 16-bit modbus style address
 * @param[in] val  - the 16-bit value
 * @param[in] invert - invert the value before storing
 */
void mem_set(uint16_t addr, uint16_t val, bool invert){
	int col = addr / 10000;
	int row = (addr % 10000);
	if(addr_decode(addr, &col, & row)){
		if(col < 2){
			if(invert) val = !val;
			gtk_toggle_button_set_active(bit_memory[col][row], (val ? true : false));
		} else {
			if(invert) val = ~val;
			gtk_spin_button_set_value(word_memory[col-2][row],val);
		}
	}
}
/* mem_get - Get memory callback for il_interpreter
 * Get a value from memory (optional invert). Bit values are inverted
 * 0->1, 1->0. 16-bit values are bitwise inverted. 0 -> 0xFFFF
 * 
 * @param[in] addr - the 16-bit modbus style address
 * @param[in] invert - invert the value before returning
 * @return - 0 if invalid address.
 *           else requested value from memory (with invert)
 */
uint16_t mem_get(uint16_t addr, bool invert){
	uint16_t val = 0;
	int row, col;
	if(addr_decode(addr, &col, & row)){
		if(col < 2){
			val = (gtk_toggle_button_get_active(bit_memory[col][row]))? 1 : 0;
			if(invert) val = !val;
		} else {
			val = gtk_spin_button_get_value_as_int(word_memory[col-2][row]);
			if(invert) val = ~val;
		}
	}
	return val;
}

/* memory callback structure to initialise il_interpreter 
 * - contains pointers to the two callback funtions 
 */
il_memory_callbacks mc = {
	mem_get,
	mem_set
};

/********************************************************
 * Execution Control 
 * Control execution of the il_interpreter on the global
 * program and memory and display on screen.
 *
 * User options are:
 * - (re)Initialise the program
 * - Single step one line of program
 * - Run to the end of the current execution of the program
 * - Run the program continuously.
 * - Halt continuous execution of the program
 ********************************************************/

/* Global holding the current execution line of the IL program */
uint16_t current_line;

/* Update the display to show the current state 
 * - Highlight the current program line
 * - Update the current accumulator with the value from il_interpreter
 */
void show_state(void){
	if(current_line < NUM_LINES){
		gtk_widget_grab_focus(program[current_line].command);
	}
	// retreive the current accumulator value and display
	gtk_spin_button_set_value(accum, il_interp_get_accum());
}

/* Execute one line of the global program[] array
 * 
 * @param - line_no - The program line to execute.
 * @return - the new program counter / line number after executing the command
 */
uint16_t execute_line(line_no){
	gchar * command = NULL;
	GtkTreeIter iter;
	GtkTreeStore *cmds;
	gint value;

	// Get the iterator to indicate the active item
	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(program[line_no].command), & iter)){
		// now extract the tree model from the combobox
		cmds = GTK_TREE_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(program[line_no].command)));
		// And pull the text from the tree model
		gtk_tree_model_get(GTK_TREE_MODEL(cmds), & iter, 0, &command, -1);
		//gtk_combo_box_get_active_text(program[line_no].command, & command);
		//command = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(program[line_no].command))));
		value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(program[line_no].value));
#ifdef DEBUG
		g_print("%s : %d\n", command, value);
#endif
		// Execute the command to update state and get the new program counter value
		line_no = il_interp_execute(il_interp_parse(command), value, line_no);
	} else {
                // Empty command then just move to the next line. No change to state
		line_no = line_no +1;
	}
	return line_no;
}

/* Program execution status variables */
static bool init = false;  // Flag for one-time il initialisation
static bool halt=false;    // Flag to request halt after next instruction
static bool running=false; // Flag indicates already running

/* Single-Step the current line of the program 
 * - If already running or not initialised then ignore
 */
void prog_step(void){
	if(!running && init && (current_line < NUM_LINES)){
		current_line = execute_line(current_line);
		show_state();
	}
}

/* Initialise the program state
 * - Only initialise il_interp if not already initialised
 * - If already running then ignore
 */
void prog_init(void){
	if(running) return;
#ifdef DEBUG
	g_print("init\n");
#endif
	current_line = 0;
	show_state();
	if(!init){
		il_interp_init(&mc);
		init = true;
	}
}

/* Flag "halt" requested when program is running */
void prog_halt(void){
	halt = true;
}

/* Run to the end of the current instruction list execution */
void run_to_end(void){
	running = true;
	while((current_line < NUM_LINES) && !halt){
		current_line = execute_line(current_line);
		//show_state();
		if(gtk_events_pending()){
			gtk_main_iteration();
		}
	}
	if(!halt){
		current_line = 0;
	}
	show_state();
	running = false;
}
/* Run to the end of the current instruction list execution 
 * - If already running or not initialised then ignore
 */
void prog_run(void){
	if(!init || running) return;
	halt = false;
	run_to_end();
}

/* Delay between loop executions. Try to mimic 
 * actual hardware implementation which is exactly
 * 250 mSec per loop */
#define LOOP_TIME_MS 250UL

/* Execute the program instruction list repeatedly until
 * the 'halt' flag is set. 
 */
void execute_program(void){
	if(running) return;
	prog_run(); // Finish current run if any
	prog_init();
	do{
		Sleep(LOOP_TIME_MS);
		if(!halt){
			run_to_end();
		} else {
			show_state();
		}
	} while(!halt);
}

void app_quit(GtkWidget * window){
	halt = true;
	gtk_widget_destroy(window);
}


/*****************************************************
 * Display implementation using gtk. 
 * Executed when application is activated.
 * Generate the display panes to hold the data and 
 * Generate the objects to display on screen and attach them
 * to the display and to the program state items above.
 ****************************************************/

static void
activate (GtkApplication *app,
          gpointer        user_data)
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *button;
	GtkWidget *combobox;
	GtkPaned *panes;
	GtkWidget *list;
	GtkTreeModel *model;
	gchar label_text[10];
	GtkWidget* label;
	int i, j;

	/* create a new window, and set its title */
	window = gtk_application_window_new (app);
	gtk_window_set_title (GTK_WINDOW (window), "Programmable Logic Simulator");
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	/* Here we construct the container that is going pack our buttons */
	grid = gtk_grid_new (); // This will hold the command buttons and Memory

	list = gtk_grid_new();  // This will hold the program commands

	panes = GTK_PANED(gtk_paned_new(GTK_ORIENTATION_HORIZONTAL));

	gtk_paned_add1(panes, list); // Command list on the left
	gtk_paned_add2(panes, grid); // Buttons and Memory on the right
	/* Pack the container in the window */
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET(panes));

	/* Add the command buttons to the right pane */
	button = gtk_button_new_with_label ("init");
	g_signal_connect (button, "clicked", G_CALLBACK (prog_init), NULL);
	gtk_grid_attach (GTK_GRID (grid), button, 1, 0, 1, 1);
	button = gtk_button_new_with_label ("step");
	g_signal_connect (button, "clicked", G_CALLBACK (prog_step), NULL);
	gtk_grid_attach (GTK_GRID (grid), button, 2, 0, 1, 1);
	button = gtk_button_new_with_label ("run");
	g_signal_connect (button, "clicked", G_CALLBACK (prog_run), NULL);
	gtk_grid_attach (GTK_GRID (grid), button, 3, 0, 1, 1);
	button = gtk_button_new_with_label ("halt");
	g_signal_connect (button, "clicked", G_CALLBACK (prog_halt), NULL);
	gtk_grid_attach (GTK_GRID (grid), button, 4, 0, 1, 1);
	button = gtk_button_new_with_label ("execute");
	g_signal_connect (button, "clicked", G_CALLBACK (execute_program), NULL);
	gtk_grid_attach (GTK_GRID (grid), button, 5, 0, 1, 1);
	button = gtk_button_new_with_label ("Quit");
	g_signal_connect_swapped (button, "clicked", G_CALLBACK (app_quit), window);
	gtk_grid_attach (GTK_GRID (grid), button, 1, 1, 1, 2);

	/* Add the Accumulator to the right pane and attach to the global 'accum'*/
	label = gtk_label_new("Accum");
	gtk_grid_attach(GTK_GRID(grid), label, 5, 1, 1, 1);

	button = gtk_spin_button_new_with_range(0.0,65535.0,1);
	accum = GTK_SPIN_BUTTON(button);  // Save for later access.
	gtk_grid_attach (GTK_GRID (grid), button, 6, 1, 1, 1);

	/* and the memory label */
	label = gtk_label_new("Memory");
	gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 1, 1);

	/* Create the memory store on screen and connect to the memory arrays */
	for(j = 0; j < 4; j++){
		uint16_t base;
		if(j >= 2)  base = (j+1) * 10000; // For 16-bit stores 
		else        base = j     * 10000; // and for 1-bit stores
		/* Now add the memory storage */
		for(i = 0; i < MEM_SIZE; i++){
			/* Create the text for the label - the modbus style address */
			sprintf((char*)label_text, "%05d", base+i+1);
			if(j >= 2){ /* 16-bit memory stores */
				/* Create the on-screen display */
				button = gtk_spin_button_new_with_range (0.0,65535,1);
				gtk_spin_button_set_increments(GTK_SPIN_BUTTON(button), 1, 10);
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(button),0);
				gtk_spin_button_set_digits(GTK_SPIN_BUTTON(button),0);
				gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(button),true);

				label = gtk_label_new((const gchar*)label_text);
				
				/* Attach to the display */
				gtk_grid_attach (GTK_GRID (grid), label,	2*j-1, 4+i, 1, 1);
				gtk_grid_attach (GTK_GRID (grid), button, 2*j-0, 4+i, 1, 1);
				/* and attach to the memory store */
				word_memory[j-2][i] = GTK_SPIN_BUTTON(button);
			} 
			else { /* bit / discrete memory stores */
				/* Create the on-screen display */
				button = gtk_toggle_button_new_with_label(label_text);
				gtk_grid_attach(GTK_GRID(grid), button, j+1, 4+i, 1, 1);
				/* and attach to the memory store */
				bit_memory[j][i] = GTK_TOGGLE_BUTTON(button);
			}
		}
	}

	/* Now the program item entry - Combaination of combobox and spin-edit*/

	model = CreateCommands(); // Create a menu object to select the command

	// Create the list of commands
	for(i = 0; i < NUM_LINES; i++){
		gchar * label_text[10];
		GtkWidget* label;

		/* The label / line number - starting at 0 */
		sprintf((char*)label_text, "%d", i);
		label = gtk_label_new((const gchar*)label_text);

		/* The command with command selection menu */
		combobox = gtk_combo_box_new_with_model(model);
		gtk_combo_box_set_id_column(GTK_COMBO_BOX(combobox), 0);

		GtkCellRenderer *column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox),column,TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), column,"text", 0, NULL);

		/* The 16-bit command value */
		button = gtk_spin_button_new_with_range (0.0,65535.0,1);
		gtk_spin_button_set_increments(GTK_SPIN_BUTTON(button), 1, 10);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(button),0);
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(button),0);

		/* Attach the items to the display  */
		gtk_grid_attach (GTK_GRID (list), label, 0, i, 1, 1);
		gtk_grid_attach (GTK_GRID (list), combobox, 1, i, 2, 1);
		gtk_grid_attach (GTK_GRID (list), button, 3, i, 2, 1);

		/* .. and to the program[] array. */
		program[i].label = label;
		program[i].command = combobox;
		program[i].value = button;

	}
	g_object_unref(model); // model now owned by combobox...


	/* After packing the widgets, show them all in one go, by calling
     * gtk_widget_show_all() on the window.
	 * This call recursively calls gtk_widget_show() on all widgets
	 * that are contained in the window, directly or indirectly.
	 */
	gtk_widget_show_all (window);

}

/* Main program 
 * create, initialise and run the gtk application.
 * - arguments are unused
 */
int main (int	  argc, char **argv) {
	GtkApplication *app;
	int status;

	app = gtk_application_new ("demo.il.elpro", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	return status;
}
