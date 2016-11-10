#include <err.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <errno.h>
#include "tables.h"
// Other includes here.

static int foo(lua_State *L)
{
    dprintf(1, "bar\n");
    return 0;
}

struct luaL_Reg example_library[] = {
    {"foo", foo},
    {NULL, NULL}
};    

// Here be locally defined libraries.  The referenced entry point tables
// may be an export, but the name is assigned here.
static library_set libraries[] = {
    {"baz", example_library},
    {NULL, NULL}
};

#define EXAMPLE_CPP_CONST "wombat"

constant_defn example_constant_table[] = {
    CONST_BOOLEAN(thingy, 0),
    CONST_STRING(frog, "toad"),
    CONST_NUMBER(aardvarks_on_board, 42),
    CPP_CONST_STRING(EXAMPLE_CPP_CONST),
    CONST_TABLE_END
};

// Here be locally defined constants.  The referenced constant tables
// may be an export, but the name is assigned here.
static constant_table constant_tables[] = {
    {"quux", example_constant_table},
    {NULL, NULL}
};

// These are initializers exported from other modules.  They may
// set up their own libraries and constants.
static int (*initializers[])(lua_State *) = {
    NULL
};


int main(int argc, char **argv, char **env)
{
    static lua_State *L;
    int (**initfunptr)(lua_State *);
    int i, rc;
    library_set *libptr;
    constant_table *constsetptr;

    L = luaL_newstate();
    luaL_openlibs(L);

    // Merge in libraries.
    for (libptr = libraries; libptr->name; ++libptr) {
	luaL_register(L, libptr->name, libptr->library);
	lua_pop(L, 1);
    }

    // Merge in constants.
    for (constsetptr = constant_tables; constsetptr->name; ++constsetptr) {
	constant_defn *constant;
	lua_getglobal(L, constsetptr->name);
	if (lua_isnil(L, -1)) {
	    lua_pop(L, 1);
	    lua_newtable(L);
	}
	for (constant = constsetptr->table; constant->name; constant++) {
	    lua_pushstring(L, constant->name);
	    switch (constant->type) {
	    case $$BOOLEAN:
		lua_pushboolean(L, constant->value.boolean != 0);
		break;
	    case $$NUMBER:
		lua_pushnumber(L, constant->value.number);
		break;
	    case $$STRING:
		lua_pushstring(L, constant->value.string);
		break;
	    default: errx(1, "Bad constant type: %d", constant->type);
	    }
	    lua_rawset(L, -3);
	}
	lua_setglobal(L, constsetptr->name);
    }
    
    // Run initializers defined elsewhere.
    for (initfunptr = initializers; *initfunptr; initfunptr++)
	(**initfunptr)(L);

    lua_newtable(L);
    for (i=1; *argv; i++) {
        lua_pushstring(L, *argv++);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");

    lua_newtable(L);
    for (i=1; *env; i++) {
	lua_pushstring(L, *env++);
	lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "env");

#ifdef LUA_SHELL
    luaL_loadstring(L,
 "function " LUA_SHELL "(greet)\n"
 "   local command, next_line, chunk, error\n"
 "   io.write((greet and greet..'/' or '')..'Lua shell:\\n')\n"
 "   repeat\n"
 "      io.write('Lua> ')\n"
 "      next_line, command = io.read'*l', ''\n"
 "      if next_line then\n"
 "       next_line, print_it = next_line:gsub('^=(.*)$', 'return %1')\n"
 "       repeat\n"
 "          command = command..next_line..'\\n'\n"
 "          chunk, error = loadstring(command, '')\n"
 "          if chunk then\n"
 "             (function (success, ...)\n"
 "                 if not success then error = ...\n"
 "                 elseif print_it ~= 0 then print(...)\n"
 "                 end end)(pcall(chunk))\n"
 "             break\n"
 "          end\n"
 "          if not error:find('<eof>') then break end\n"
 "          io.write('>> ')\n"
 "          next_line = io.read '*l'\n"
 "       until not next_line\n"
 "       if error then print((error:gsub('^[^:]*:(.*)$', 'stdin:%1'))) end\n"
 "      end\n"
 "   until not next_line\n"
 "   io.write 'Goodbye\\n'"
 "end\n");
    lua_call(L, 0, 0);
#endif
    
    rc = luaL_loadstring(L, "require 'bytecode'");

    switch (rc) {
    case 0:
	break;
    case LUA_ERRFILE:
	errx(1, lua_tostring(L, -1));
    case LUA_ERRSYNTAX:
	errx(1, "invalid lua script.  %s", lua_tostring(L, -1));
    default:
	errx(1,"unknown lua error %d loading main script", rc);
    }

    if (rc = lua_pcall(L, 0, 0, 0)) {
	const char *errmsg;

	errx(1, "Lua script failed!  %s",
	     (errmsg = lua_tostring(L, -1))
	     ? errmsg : "***CAN'T OBTAIN ERROR MESSAGE***");
    }

    lua_close(L);
    return 0;
}
