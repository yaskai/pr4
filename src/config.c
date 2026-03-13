#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "../include/log_message.h"
#include "../include/ansi_codes.h"

Option OptionCreate(char *key, void *control, u8 val_type) {
	Option option = (Option) {0};

	memset(option.key, '\0', sizeof(option.key));
	memcpy(option.key, key, strlen(key));

	option.control = control;
	option.value_type = val_type;

	return option;
}

OptionTable OptionTableCreate() {
	return (OptionTable) {
		.capacity = 8,
		.count = 0,
		.entries = calloc(8, sizeof(Option))
	};
}

u16 OptionTableHash(char *key) {
	u16 hash = 5381;

	unsigned char c;
	while((c = *key++) != '\0') 
		hash = ((hash << 5)) + c;

	return hash;
}

u16 Probe(u16 id, u16 attempt, u16 size) {
	return (id + attempt) % size;
}

void OptionTableResize(OptionTable *table) {
	// Create temporary array with doubled table capacity  
	u16 new_cap = table->capacity << 1;
	Option *new_entries = calloc(new_cap, sizeof(Option)); 

	// Copy entries to their correct indices in temp array 
	for(u16 i = 0; i < table->capacity; i++) {
		Option entry = table->entries[i];
		if(entry.key[0] == '\0') 
			continue; 

		u16 id = OptionTableHash(entry.key) % new_cap;
		new_entries[id] = entry;
	}

	// Set table capacity to new 
	table->capacity = new_cap;

	// Resize table array
	Option *array_ptr = realloc(table->entries, sizeof(Option) * new_cap); 
	table->entries = array_ptr;

	// Copy data, free temp array
	memcpy(table->entries, new_entries, sizeof(Option) * new_cap);
	free(new_entries);
}

void OptionTableInsert(OptionTable *table, Option entry) {
	if(table->count + 1 > table->capacity) 
		OptionTableResize(table);

	u16 id = OptionTableHash(entry.key) % table->capacity;
	u16 attempt = 0;

	while(attempt < table->capacity) {
		/*
		if(!streq(table->entries[id].key, entry.key)) {;
			id = Probe(id, ++attempt, table->capacity); 
			continue;
		}
		*/
		if(table->entries[id].key[0] != '\0') {
			id = Probe(id, ++attempt, table->capacity); 
			continue;
		}

		break;
	}

	table->entries[id] = entry;		
	table->count++;
}

Option *OptionTableSearch(OptionTable *table, char *key) {
	Option *option = NULL;

	char str_key[64] = { '\0' };
	memcpy(str_key, key, strlen(key));

	u16 id = OptionTableHash(str_key) % table->capacity;	
	u16 attempt = 0;

	while(attempt < table->capacity) {
		if(streq(table->entries[id].key, str_key)) {
			option = &table->entries[id];
			break;
		}

		id = Probe(id, ++attempt, table->capacity);
	}

	return option;
}

void ConfigInit(Config *conf) {
	for(u8 i = 0; i < OPTION_BLOCK_COUNT; i++) 
		conf->option_tables[i] = OptionTableCreate();

	Option opt_ww = OptionCreate("width", &conf->window_width, VAL_INT);
	OptionTableInsert(&conf->option_tables[OPT_BLOCK_WINDOW], opt_ww);
	
	Option opt_wh = OptionCreate("height", &conf->window_height, VAL_INT);
	OptionTableInsert(&conf->option_tables[OPT_BLOCK_WINDOW], opt_wh);

	Option opt_mouse_sensitivity = OptionCreate("mouse_sens", &conf->mouse_sensitivity, VAL_INT);
	OptionTableInsert(&conf->option_tables[OPT_BLOCK_INPUT], opt_mouse_sensitivity);
}

void ConfigClose(Config *conf) {
	for(u8 i = 0; i < OPTION_BLOCK_COUNT; i++) { 
		if(conf->option_tables[i].entries) 
			free(conf->option_tables[i].entries);
	}
}

void ConfigRead(Config *conf, char *path) {
	// Open file
	FILE *pF = fopen(path, "r");	

	// Print error message and exit if file missing
	if(!pF) {
		MessageError("Could not open configuration file", path);
		return;
	}

	// Read file content by line
	u8 block = 0;
	char line[128];
	while(fgets(line, sizeof(line), pF)) {
		ConfigParseLine(conf, line, &block, 0);
	}

	// Close file
	fclose(pF);
}

void ConfigParseLine(Config *conf, char *line, u8 *block, u8 print) {
	char *block_open = strchr(line, BLOCK_OPEN_MARKER);
	char *block_close = strchr(line, BLOCK_CLOSE_MARKER);

	if(block_open && block_close) {
		u8 len = strlen(line);
		char block_name[64] = { '\0' }; 
		memcpy(block_name, line+1, len-3);

		if(streq(block_name, "window")) {
			*block = OPT_BLOCK_WINDOW;	

		} else if(streq(block_name, "input")) {
			*block = OPT_BLOCK_INPUT;

		} else if(streq(block_name, "other")) {
			*block = OPT_BLOCK_OTHER;	

		}
	}

	OptionTable *table = &conf->option_tables[*block];

	char *comment = strchr(line, COMMENT_MARKER);	
	if(comment) *comment = '\0';

	if(!line && !comment)
		return;

	if(print) {
		char *print_color = (block_open && block_close) ? ANSI_BLUE : ANSI_WHITE;
		Message(line, print_color);

		if(comment) 
			ConfigPrintComment(comment);
	}

	char *eq = strchr(line, '=');
	if(!eq) 
		return;

	*eq = '\0';
	char *key = line;
	char *val = eq + 1;

	u16 id = OptionTableHash(key) % table->capacity; 

	Option *option = OptionTableSearch(table, key);
	if(!option)
		return;	
	
	switch(option->value_type) {
		case VAL_INT: {
			i32 i = 0; 
			sscanf(val, "%d", &i);

			if(!strcmp(val, "auto"))
				i = atoi("auto");

			option->val = i;

		} break;

		case VAL_FLOAT: {
			float f = 0;
			sscanf(val, "%f", &f);

		} break;

		case VAL_BOOL: {
			bool b = 0;
			b = (streq(val, "true"));

			if(b) option->val = 1;

		} break;

		case VAL_STRING: {
			i32 i = atoi(val);
			option->val = i;

		} break;

		case VAL_HEX: {

		} break;
	}

	*option->control = option->val;
}

void ConfigPrintComment(char *comment) {
	comment++;

	char str_message[strlen(comment)];

	str_message[0] = '#';
	memcpy(str_message + 1, comment, strlen(comment));

	Message(str_message, ANSI_GREEN);
}

