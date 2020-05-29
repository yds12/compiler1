ERROR_COLOR = "\033[31m"
SUCCESS_COLOR = "\033[32m"
STRONG_ERROR_COLOR = "\033[1;31m"
STRONG_SUCCESS_COLOR = "\033[1;32m"
END_COLOR = "\033[m"

CASES_DIR = "test/cases"

puts "Running test suite..."
puts ""

files = `ls #{CASES_DIR}`.split "\n"

total = 0
success = 0

files.each do |f|
  total += 1
  command = "build/ulpc --silent #{CASES_DIR}/#{f} ; echo $?"

#  puts "Running command:"
#  puts "\t#{command}"
#  puts ""

  result = `#{command}`
  
  if result.strip == '0' then 
    success += 1
    puts "\t#{f} #{SUCCESS_COLOR}pass#{END_COLOR}"
  else 
    puts "\t#{f} #{ERROR_COLOR}fail#{END_COLOR}" 
  end
end

print "\n#{total} tests, "

if success == 0 then
  print "#{success} passes"
else
  print "#{STRONG_SUCCESS_COLOR}#{success} passes#{END_COLOR}"
end

if success == total then
  print " and #{total - success} failures.\n"
else
  print " and #{STRONG_ERROR_COLOR}#{total - success} failures#{END_COLOR}.\n"
end
