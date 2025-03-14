# HOW TO BUILD USING CMAKE as a standalone project:
# mkdir build
# cd build
# cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
# make

add_application(opentyrian2000   
   ../src/animlib.c
   ../src/arg_parse.c
   ../src/backgrnd.c
   ../src/config.c
   ../src/config_file.c
   ../src/destruct.c
   ../src/editship.c
   ../src/episodes.c
   ../src/file.c
   ../src/font.c
   ../src/fonthand.c
   ../src/game_menu.c
   ../src/helptext.c
   ../src/joystick.c
   ../src/jukebox.c
   ../src/keyboard.c
   ../src/keyboard_vita.c
   ../src/lds_play.c
   ../src/loudness.c
   ../src/lvllib.c
   ../src/lvlmast.c
   ../src/mainint.c
   ../src/menus.c
   ../src/mouse.c
   ../src/mtrand.c
   ../src/musmast.c
   ../src/network.c
   ../src/nortsong.c
   ../src/nortvars.c
   ../src/opentyr.c
   ../src/opl.c
   ../src/palette.c
   ../src/params.c
   ../src/pcxload.c
   ../src/pcxmast.c
   ../src/picload.c
   ../src/player.c
   ../src/shots.c
   ../src/sizebuf.c
   ../src/sndmast.c
   ../src/sprite.c
   ../src/starlib.c
   ../src/tyrian2.c
   ../src/varz.c
   ../src/vga256d.c
   ../src/vga_palette.c
   ../src/video.c
   ../src/video_scale.c
   ../src/video_scale_hqNx.c
   ../src/video_vita.c
   ../src/xmas.c
    CONSOLE
   )

    # make the result as small as possible
    # by removing unused code (gc-sections)
    # and by removing macsbug function names on 68K
    # (don't do this when debugging...)
set(CMAKE_C_FLAGS "-DWITH_SDL -DWITH_SDL1 $(CMAKE_C_FLAGS)")
set_target_properties(opentyrian2000 PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(opentyrian2000 PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(opentyrian2000 PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
target_link_libraries(opentyrian2000 "-lSDL -lc -lm")
