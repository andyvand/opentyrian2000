import subprocess
from subprocess import call

cmd = "~/esp/xtensa-esp32-elf/bin/xtensa-esp32-elf-addr2line "
#cmd = "C:\\Users\\Jason\\.platformio\\packages\\toolchain-xtensa32\\bin\\xtensa-esp32-elf-addr2line "
cmd += "-e ~/Documents/GitHub/OpenTyrian/build/OpenTyrian.elf "
#cmd += "-e C:\\Users\\Jason\\Documents\\PlatformIO\\Projects\\OpenTyrian\\.pioenvs\\featheresp32\\firmware.elf "
cmd += "-a "
trace = raw_input("stacktrace: ")
split = trace.split(' ')

for x in split:
    subprocess.call(cmd + x, shell = True)

 