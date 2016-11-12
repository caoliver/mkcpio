for _,key in pairs { 'emit_dir','emit_pipe','emit_socket',
		     'emit_node', 'emit_symlink', 'emit_file' } do
   local fn = cpio[key]
   cpio[key] = function(file, ...)
      return fn(type(file) == 'string' and { file } or file, ...)
   end
end

local ix=2

local interactive
if arg[ix] == '-i' then
   ix = ix + 1
   interactive = true
end
for ix = ix, #arg do
   dofile(arg[ix])
end

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
