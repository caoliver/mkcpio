typedef struct {
    char *name;
    struct luaL_Reg *library;
} library_set;

typedef struct {
    char *name;
    enum {$$NULL, $$BOOLEAN, $$STRING, $$NUMBER} type;
    union {
	char *string;
	double number;
	int boolean;
    } value;
} constant_defn;

typedef struct {
    char *name;
    constant_defn *table;
} constant_table;

#define CONST_BOOLEAN(NAME, VALUE) { #NAME, $$BOOLEAN, {.boolean=VALUE } }
#define CONST_NUMBER(NAME, VALUE) { #NAME, $$NUMBER, {.number=VALUE } }
#define CONST_STRING(NAME, VALUE) { #NAME, $$STRING, {.string=VALUE } }
#define CONST_TABLE_END { NULL, $$NULL, {.boolean=0} }

#define CPP_CONST_BOOLEAN(NAME) { #NAME, $$BOOLEAN, {.boolean=NAME } }
#define CPP_CONST_NUMBER(NAME) { #NAME, $$NUMBER, {.number=NAME } }
#define CPP_CONST_STRING(NAME) { #NAME, $$STRING, {.string=NAME } }
