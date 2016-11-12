for _,key in pairs { 'emit_dir','emit_pipe','emit_socket',
		     'emit_node', 'emit_symlink', 'emit_file' } do
   local fn = cpio[key]
   cpio[key] = function(file, ...)
      return fn(type(file) == 'string' and { file } or file, ...)
   end
end

if arg[2] == '-h' or arg[2] == '--help' then
   print ([[
Command format:

	]]..arg[1]..[[ [-i] script ... [-- script_arg ...]

]]..arg[1]..[[ ends up in interactive mode if the '-i' option is given or
if there are no scripts given.

Script_args are available the the scripts and interactive session
in the arg table.

Defined functions:

   set_output(file):

        Send output to this open lua file. If the argument is absent,
        the output is flushed, and the output is set to stdout.

   emit_directory(name, mode*, uid, gid)

   emit_file(names*, source, mode, uid, gid, is_block, major, minor)

   emit_node(names*, mode*, uid, gid)

   emit_pipe(names*, mode*, uid, gid)

   emit_socket(names*, mode*, uid, gid)

   emit_symlink(names**, target, mode, uig, gid)

* Names may be a single string or a table of strings for the hard
links to this data.  Source is the location of the file to copy
into the archive.

** Names may be a single string or a table of strings for the
symbolic links to this data.  Target is the location of the file
to which to link.

* Modes and be integers or integer equivalent strings.  These are
processed via strtol, so to give an octal string, use a string of
digits with a leading zero.
]])
   os.exit(0)
end

-- Ditch program name
table.remove(arg, 1)

local interactive
if arg[1] == '-i' then
   interactive = true
   table.remove(arg, 1)
end

while arg[1] do
   local first = arg[1]
   table.remove(arg, 1)
   if first == '--' then break end
   dofile(first)
   interactive = interactive or false
end

interactive = interactive ~= false

if interactive or #arg == 1 then
   local command, next_line, chunk, error
   io.write('mkcpio/Lua shell:\n')
   repeat
      io.write('Lua> ')
      next_line, command = io.read'*l', ''
      if next_line then
	 next_line, print_it = next_line:gsub('^=(.*)$', 'return %1')
	 repeat
	    command = command..next_line..'\n'
	    chunk, error = loadstring(command, '')
	    if chunk then
	       (function (success, ...)
		     if not success then error = ...
		     elseif print_it ~= 0 then print(...)
	       end end)(pcall(chunk))
	       break
	    end
	    if not error:find('<eof>') then break end
	    io.write('>> ')
	    next_line = io.read '*l'
	 until not next_line
	 if error then print((error:gsub('^[^:]*:(.*)$', 'stdin:%1'))) end
      end
   until not next_line
   io.write 'Goodbye\n'
end
