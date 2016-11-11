CFLAGS+=-fPIC -I /usr/local/include/luajit-2.0/
CFLAGS+=-Wall -Wno-parentheses -O2 -mtune=generic -fomit-frame-pointer
LDFLAGS+=-lluajit-5.1
OBJS=main.o bytecode.o cpiofns.o
# Main lua entry point must be listed first.
LUAPARTS=main.lua

# Define the name of an interactive lua shell function.
# Comment out to remove shell code.
CFLAGS+=-DLUA_SHELL=\"lua_shell\"

.PHONY: all clean

all: mkcpio

bytecode.o: ${LUAPARTS}
	lua combine.lua ${LUAPARTS} | lua -b -n bytecode - $@

mkcpio: ${OBJS}
	gcc ${LDFLAGS} -s -Wl,-E -o $@ $^

clean:
	rm -f mkcpio ${OBJS}
