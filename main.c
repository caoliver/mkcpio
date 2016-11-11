#include <err.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <errno.h>

extern struct luaL_Reg cpio_fns[];
extern void cpio_init();

struct library_set {
    char *name;
    struct luaL_Reg *library;
};

static struct library_set libraries[] = {
    {"cpio", cpio_fns},
    {NULL, NULL}
};

int main(int argc, char **argv, char **env)
{
    static lua_State *L;
    int i, rc;
    struct library_set *libptr;

    L = luaL_newstate();
    luaL_openlibs(L);

    // Merge in libraries.
    for (libptr = libraries; libptr->name; ++libptr) {
	luaL_register(L, libptr->name, libptr->library);
	lua_pop(L, 1);
    }
    
    lua_newtable(L);
    for (i=1; *argv; i++) {
        lua_pushstring(L, *argv++);
        lua_rawseti(L, -2, i);
    }
    lua_setglobal(L, "arg");

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

    cpio_init();
    
    if (rc = lua_pcall(L, 0, 0, 0)) {
	const char *errmsg;

	errx(1, "Lua script failed!  %s",
	     (errmsg = lua_tostring(L, -1))
	     ? errmsg : "***CAN'T OBTAIN ERROR MESSAGE***");
    }

    lua_close(L);
    return 0;
}
