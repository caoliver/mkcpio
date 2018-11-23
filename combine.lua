for _,progfile in ipairs(arg) do
   local file,err=io.open(progfile, "r")
   if not file then error(err) end
   local comment = "  --[==[ "..progfile:upper().." ]==]\n\n"
   io.write("do ",comment,file:read("*a"),"\n\nend",comment,"\n")
   file:close()
end
