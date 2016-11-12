#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <err.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// Process files in 16MB clumps.
#define FILE_CLUMP (16 * 1024 * 1024)

typedef unsigned int UINT;

static FILE *cpio_output;

/*
 * Original work by Jeff Garzik
 *
 * External file lists, symlink, pipe and fifo support by Thayne Harbaugh
 * Hard link support by Luciano Rocha
 */

#define xstr(s) #s
#define str(s) xstr(s)

static unsigned int offset;
static unsigned int ino = 721;
static time_t default_mtime;
static char *filebuf;

struct file_handler {
    const char *type;
    int (*handler)(const char *line);
};

static int checkpositiveint(lua_State *L, int index)
{
    double num = luaL_checknumber(L, index);
    if (num != (int)num || num < 0)
	luaL_error(L, "Number %f isn't a positive integer", num);
    return (int)num;
}

static int checkmode(lua_State *L, int index)
{
    const char *modestr = lua_tostring(L, index);
    char *ep;
    int value;

    if (modestr != NULL) {
	value = strtol(modestr, &ep, 0);
	if (!*ep && value > 0)
	    return value;
    }
    return luaL_error(L, "Bad mode value");
}

static void emit_string(const char *name)
{
    unsigned int name_len = strlen(name) + 1;

    fputs(name, cpio_output);
    putc(0, cpio_output);
    offset += name_len;
}

static void emit_pad (void)
{
    while (offset & 3) {
	putc(0, cpio_output);
	offset++;
    }
}

static void emit_rest(const char *name)
{
    unsigned int name_len = strlen(name) + 1;
    unsigned int tmp_ofs;

    fputs(name, cpio_output);
    putc(0, cpio_output);
    offset += name_len;

    tmp_ofs = name_len + 110;
    while (tmp_ofs & 3) {
	putc(0, cpio_output);
	offset++;
	tmp_ofs++;
    }
}

static void emit_hdr(const char *s)
{
    fputs(s, cpio_output);
    offset += 110;
}

static int emit_trailer(lua_State *L)
{
    char s[256];
    const char name[] = "TRAILER!!!";

    sprintf(s, "%s%08X%08X%08lX%08lX%08X%08lX"
	    "%08X%08X%08X%08X%08X%08X%08X",
	    "070701",		/* magic */
	    0,			/* ino */
	    0,			/* mode */
	    (long) 0,		/* uid */
	    (long) 0,		/* gid */
	    1,			/* nlink */
	    (long) 0,		/* mtime */
	    0,			/* filesize */
	    0,			/* major */
	    0,			/* minor */
	    0,			/* rmajor */
	    0,			/* rminor */
	    (unsigned)strlen(name)+1, /* namesize */
	    0);			/* chksum */
    emit_hdr(s);
    emit_rest(name);

    while (offset % 512) {
	putc(0, cpio_output);
	offset++;
    }

    free(filebuf);
    
    return 0;
}

static int emit_symlink(lua_State *L)
{
    const char *target = luaL_checkstring(L, 2);
    unsigned int mode = checkmode(L, 3);
    uid_t uid = checkpositiveint(L, 4);
    uid_t gid = checkpositiveint(L, 5);
    char s[256];
    int i;
    int nlinks;
    
    luaL_checktype(L, 1, LUA_TTABLE);
    nlinks = lua_objlen(L, 1);

    for (i = 1; i <= nlinks; i++) {
	const char *name;

	lua_pushinteger(L, i);
	lua_gettable(L, 1);
	name=luaL_checkstring(L, -1);
	lua_pop(L, 1);
    
	if (name[0] == '/')
	    name++;
	sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
		"%08X%08X%08X%08X%08X%08X%08X",
		"070701",		      /* magic */
		ino,			      /* ino */
		S_IFLNK | mode,		      /* mode */
		(long) uid,		      /* uid */
		(long) gid,		      /* gid */
		1,			      /* nlink */
		(long) default_mtime,	      /* mtime */
		(unsigned)strlen(target) + 1, /* filesize */
		3,			      /* major */
		1,			      /* minor */
		0,			      /* rmajor */
		0,			      /* rminor */
		(unsigned)strlen(name) + 1,   /* namesize */
		0);			      /* chksum */
	emit_hdr(s);
	emit_string(name);
	emit_pad();
	emit_string(target);
	emit_pad();
    }

    ino++;
    return 0;
}

static int emit_ipc(lua_State *L, int type)
{
    unsigned int mode = checkmode(L, 2) | type;
    uid_t uid = checkpositiveint(L, 3);
    uid_t gid = checkpositiveint(L, 4);
    char s[256];
    int nlinks;
    int i;
    
    luaL_checktype(L, 1, LUA_TTABLE);
    nlinks = lua_objlen(L, 1);

    for (i = 1; i <= nlinks; i++) {
	const char *name;

	lua_pushinteger(L, i);
	lua_gettable(L, 1);
	name=luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if (name[0] == '/')
	    name++;
	sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
		"%08X%08X%08X%08X%08X%08X%08X",
		"070701",	        /* magic */
		ino,			/* ino */
		mode,			/* mode */
		(long) uid,		/* uid */
		(long) gid,		/* gid */
		2,			/* nlink */
		(long) default_mtime,	/* mtime */
		0,		        /* filesize */
		3,			/* major */
		1,			/* minor */
		0,			/* rmajor */
		0,			/* rminor */
		(UINT)strlen(name) + 1,	/* namesize */
		0);		        /* chksum */
	emit_hdr(s);
	emit_string(name);
	emit_pad();
    }
    
    ino++;
    return 0;
}

static int emit_socket(lua_State *L)
{
    return emit_ipc(L, S_IFSOCK);
}

static int emit_pipe(lua_State *L)
{
    return emit_ipc(L, S_IFIFO);
}

static int emit_directory(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    unsigned int mode = checkmode(L, 2) | S_IFDIR;
    uid_t uid = checkpositiveint(L, 3);
    uid_t gid = checkpositiveint(L, 4);
    char s[256];

    if (name[0] == '/')
	name++;
    sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
	    "%08X%08X%08X%08X%08X%08X%08X",
	    "070701",		    /* magic */
	    ino++,		    /* ino */
	    mode,		    /* mode */
	    (long) uid,		    /* uid */
	    (long) gid,		    /* gid */
	    2,			    /* nlink */
	    (long) default_mtime,   /* mtime */
	    0,			    /* filesize */
	    3,			    /* major */
	    1,			    /* minor */
	    0,			    /* rmajor */
	    0,			    /* rminor */
	    (UINT)strlen(name) + 1, /* namesize */
	    0);			    /* chksum */
    emit_hdr(s);
    emit_rest(name);
    return 0;
}

static int emit_node(lua_State *L)
{
    unsigned int mode = checkmode(L, 2);
    uid_t uid = checkpositiveint(L, 3);
    uid_t gid = checkpositiveint(L, 4);
    int isblock=lua_toboolean(L,5);
    unsigned int maj = checkpositiveint(L, 6);
    unsigned int min = checkpositiveint(L, 7);
    char s[256];
    int nlinks;
    int i;

    luaL_checktype(L, 1, LUA_TTABLE);
    nlinks = lua_objlen(L, 1);
    
    if (isblock)
	mode |= S_IFBLK;
    else
	mode |= S_IFCHR;

    for (i = 1; i <= nlinks; i++) {
	const char *name;

	lua_pushinteger(L, i);
	lua_gettable(L, 1);
	name=luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if (name[0] == '/')
	    name++;
	sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
		"%08X%08X%08X%08X%08X%08X%08X",
		"070701",		/* magic */
		ino,			/* ino */
		mode,			/* mode */
		(long) uid,		/* uid */
		(long) gid,		/* gid */
		nlinks,			/* nlink */
		(long) default_mtime,	/* mtime */
		0,			/* filesize */
		3,			/* major */
		1,			/* minor */
		maj,			/* rmajor */
		min,			/* rminor */
		(unsigned)strlen(name) + 1,/* namesize */
		0);			/* chksum */
	emit_hdr(s);
	emit_string(name);
	emit_pad();
    }
    
    ino++;
    return 0;
}

static int emit_file(lua_State *L)
{
    const char *location = luaL_checkstring(L, 2);
    unsigned int mode = checkmode(L, 3);
    uid_t uid = checkpositiveint(L, 4);
    uid_t gid = checkpositiveint(L, 5);
    char s[256];
    struct stat buf;
    long size;
    int file = -1;
    int retval;
    unsigned int i;
    int nlinks;

    luaL_checktype(L, 1, LUA_TTABLE);
    nlinks = lua_objlen(L, 1);

    mode |= S_IFREG;

    file = open (location, O_RDONLY);
    if (file < 0) {
	warn("File %s could not be opened for reading\n", location);
	goto error;
    }

    retval = fstat(file, &buf);
    if (retval) {
	warn("File %s could not be stat()'ed\n", location);
	goto error;
    }

    filebuf = realloc(filebuf, FILE_CLUMP);

    size = 0;
    
    for (i = 1; i <= nlinks; i++) {
	const char *name;
	
	/* data goes on last link */
	if (i == nlinks) size = buf.st_size;
	lua_pushinteger(L, i);
	lua_gettable(L, 1);
	name=luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if (name[0] == '/')
	    name++;
	sprintf(s,"%s%08X%08X%08lX%08lX%08X%08lX"
		"%08lX%08X%08X%08X%08X%08X%08X",
		"070701",	      /* magic */
		ino,		      /* ino */
		mode,		      /* mode */
		(long) uid,	      /* uid */
		(long) gid,	      /* gid */
		nlinks,		      /* nlink */
		(long) buf.st_mtime,  /* mtime */
		size,		      /* filesize */
		3,		      /* major */
		1,		      /* minor */
		0,		      /* rmajor */
		0,		      /* rminor */
		(UINT)strlen(name)+1, /* namesize */
		0);		      /* chksum */
	emit_hdr(s);
	emit_string(name);
	emit_pad();

	if (size) {
	    while (size > 0) {
		int this_read = size > FILE_CLUMP ? FILE_CLUMP : size;

		retval = read (file, filebuf, this_read);
		if (retval < 0) {
		    warn("Can not read %s file\n", location);
		    goto error;
		}

		if (fwrite(filebuf, this_read, 1, cpio_output) != 1) {
		    warn("writing filebuf failed\n");
		    goto error;
		}
		size -= this_read;
	    }
	    offset += size;
	    emit_pad();
	}
    }
    ino++;
	
error:
    if (file >= 0) close(file);
    return 0;
}

void cpio_init() {
    default_mtime = time(NULL);
    cpio_output = stdout;
}

static int set_output(lua_State *L)
{
    FILE **f;
    if (lua_isnone(L, 1)) {
	if (fileno(cpio_output) > 2)
	    fflush(cpio_output);
	cpio_output = stdout;
	return 0;
    }
    f=luaL_checkudata(L, 1, "FILE*");
    if  (*f == NULL)
	luaL_error(L, "Can't set output to closed file");
    if (fileno(*f) == 0 || fileno(*f) == 2)
	luaL_error(L, "Can't set output to stdin or stderr");
    fflush(cpio_output);
    cpio_output=*f;
    return 0;
}

struct luaL_Reg cpio_fns[] = {
    { "set_output", set_output },
    { "emit_directory", emit_directory },
    { "emit_file", emit_file },
    { "emit_node", emit_node },
    { "emit_pipe", emit_pipe },
    { "emit_socket", emit_socket },
    { "emit_symlink", emit_symlink },
    { "emit_trailer", emit_trailer },
    { NULL, NULL }
};
