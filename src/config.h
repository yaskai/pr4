#include "../include/num_redefs.h"

#ifndef CONFIG_H_
#define CONFIG_H_

#define COMMENT_MARKER		'#'
#define BLOCK_OPEN_MARKER	'['
#define BLOCK_CLOSE_MARKER	']'

#define streq(a, b) (!strcmp(a, b))

enum VALUE_TYPES : u8 {
	VAL_INT 	= 0,
	VAL_FLOAT 	= 1,
	VAL_BOOL	= 2,
	VAL_STRING	= 3,
	VAL_HEX		= 4
};

#define OPTION_BLOCK_COUNT 3
enum OPTION_BLOCK_NAMES : u8 {
	OPT_BLOCK_WINDOW,
	OPT_BLOCK_INPUT,
	OPT_BLOCK_OTHER
};

typedef struct {
	char key[64];
	i32 *control;

	i32 val;

	u8 value_type;

} Option;

Option OptionCreate(char *key, void *control, u8 val_type);

typedef struct {
	Option *entries;	

	u16 count;
	u16 capacity;
	
} OptionTable;

OptionTable OptionTableCreate();
void OptionTableClose(OptionTable *table);

u16 OptionTableHash(char *key);
u16 Probe(u16 id, u16 attempt, u16 size);

void OptionTableResize(OptionTable *table);
void OptionTableInsert(OptionTable *table, Option entry);

Option *OptionTableSearch(OptionTable *table, char *key);

typedef struct {
	u32 window_width, window_height;
	u32 target_fps;

	u32 mouse_sensitivity;

	OptionTable option_tables[OPTION_BLOCK_COUNT];

} Config;

void ConfigInit(Config *conf);
void ConfigClose(Config *conf);

void ConfigRead(Config *conf, char *path);
void ConfigParseLine(Config *conf, char *line, u8 *block, u8 print);

void ConfigPrintComment(char *comment);

#endif

