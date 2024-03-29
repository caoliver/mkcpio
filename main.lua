local program = arg[1], arg[1]:match '.*/(.*)$' or arg[1]
table.remove(arg, 1)

help = [[
Command format:

	]]..program..[[ [-c class] [-v version] [-i] script ... [-- arg ...]

]]..program..[[ ends up in interactive mode if the '-i' option is given or
if there are no scripts given.

Args are available the the scripts and interactive session
in the arg table.

Class: A single alphabetic character version class.
The default is to preserve the old class code or set to v for new
versions.

Version:

   no version  Bump the patch.
   =           Make no change.
   +           Bump the revision.
   ++          Bump the version.
   Xx.y[.z]    Set the code (X) and version.

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

   emit_trailer()

   new_version(version_file)

   time_now(strftime_format)

* Names may be a single string or a table of strings for the hard
links to this data.  Source is the location of the file to copy
into the archive.

** Names may be a single string or a table of strings for the
symbolic links to this data.  Target is the location of the file
to which to link.

* Modes and be integers or integer equivalent strings.  These are
processed via strtol, so to give an octal string, use a string of
digits with a leading zero.
]]

local function die(message)
   print(message)
   os.exit(1)
end

local version_parameter
local default_class='v'

local function new_version(old_version)
   local version = old_version or (class_opt or default_class)..'0.0.0'
   local new_class,major,minor = version:match '^([a-zA-Z])([0-9]*)%.([0-9]*)'
   local patch
   local class
   if not major then
      major,minor,patch=0,0,0
      class = default_class
   else
      class = class_opt or new_class
      patch = version:match '^.[0-9]*%.[0-9]*%.([0-9]*)$' or 0
   end
   if not version_parameter then
      patch = 1 + patch
   elseif version_parameter == '+' then
      print 'Revision bumped!'
      minor = 1 + minor
      patch = 0
   elseif version_parameter == '++' then
      print 'Major version bumped'
      major = 1 + major
      minor = 0
      patch = 0
   elseif version_parameter == '=' then
      print 'Version unchanged!'
   elseif version_parameter:match '^[a-zA-Z][0-9]' then
      class,major,minor =
	 version_parameter:match '^([a-zA-Z])([0-9]*)%.([0-9]*)'
      patch = version_parameter:match '^[a-zA-Z][0-9]*%.[0-9]*%.(.*)$' or '0'
   end
   local version=string.format(tostring(patch) == '0' and "%s%s.%s" or "%s%s.%s.%s",
			       class,major,minor,patch)
   print("Version is "..version)
   return version
end

local function check_version_parameter(parm)
   if parm ~= '=' and parm ~= '+' and parm ~= '++' and
      not parm:match '^[a-zA-Z][0-9]*%.[0-9]*$' and
      not parm:match '^[a-zA-Z][0-9]*%.[0-9]*%.[0-9]*$' then
	 die('Invalid version parameter: '..version_parameter)
   end
end


function cpio.new_version(version_file)
   local verfile=io.open(version_file)
   local verstring
   if verfile then
      verstring=verfile:read '*l'
      verfile:close()
   end
   local version=new_version(verstring)
   verfile=io.open(version_file, 'w')
   if verfile then
      verfile:write(version,'\n')
      verfile:close()
   end
   return version
end


local function assert_argument(option, optmatch)
   local optarg
   if option == optmatch then
      optarg = arg[1]
      if not optarg or optarg:match '^-' then
	 die('Missing argument for '..option)
      end
      table.remove(arg,1)
   else
      optarg=option:match('^'..optmatch..'(.*)$')
   end
   return optarg
end
   
while arg[1] and arg[1]:match '^-' do
   local opt=arg[1]
   table.remove(arg, 1)
   if opt == '-h' or opt == '--help' then
      print(help)
      os.exit(0)
   elseif opt == '-i' then
      interactive = true
   elseif opt:match '^-c' then
      class_opt = assert_argument(opt, '-c')
      if not class_opt:match '^[a-zA-Z]' then
	 die('Bad class: '..class_opt)
      end
   elseif opt:match '^-v' then
      version_parameter = assert_argument(opt, '-v')
      check_version_parameter(version_parameter)
   elseif opt == '--' then
      no_scripts = true
      interactive = true
   else
      print('Invalid option: '..opt)
      os.exit(1)
   end
end

help = nil

for _,key in pairs { 'emit_dir','emit_pipe','emit_socket',
		     'emit_node', 'emit_symlink', 'emit_file' } do
   local fn = cpio[key]
   cpio[key] = function(file, ...)
      return fn(type(file) == 'string' and { file } or file, ...)
   end
end

local realpath,chdir=cpio.realpath,cpio.chdir
local curdir=cpio.getcwd()
cpio.getcwd,cpio.realpath,cpio.chdir=nil,nil,nil
if not no_scripts then
   while arg[1] do
      local first = arg[1]
      table.remove(arg, 1)
      if first == '--' then break end
      local scriptpath=realpath(first)
      if scriptpath then
	 local dirname,basename=scriptpath:match '^(.*)/([^/]*)'
	 chdir(dirname)
	 dofile(basename)
	 chdir(curdir)
      else
	 die('Missing or unreadable mkcpio script: '..first)
      end
      interactive = interactive or false
   end
end

interactive = interactive ~= false

if interactive then
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
