#!/usr/bin/env ruby

@i = 0
@line = ""

def dump_line()
  puts "strcat(ui_xml_data, \"#{@line}\");"
  @line = ""
  @i = 0
end

f = File.open("caseconvert-ui.min.xml")

#~ f.each_char { |c|
while c = f.getc do
  if @i == 507 then dump_line() end
  @line = @line + c
  @i = @i + 1
end
