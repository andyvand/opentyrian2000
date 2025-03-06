set(CMAKE_C_FLAGS "-DWITH_SDL ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS "-DWITH_NETWORK ${CMAKE_C_FLAGS}")

idf_component_register(
    SRCS "animlib.c" "arg_parse.c" "backgrnd.c" "config.c" "config_file.c" "destruct.c" "editship.c" "episodes.c" "file.c" "font.c" "fonthand.c" "game_menu.c" "helptext.c" "joystick.c" "jukebox.c" "keyboard.c" "lds_play.c" "loudness.c" "lvllib.c" "lvlmast.c" "mainint.c" "menus.c" "mouse.c" "mtrand.c" "musmast.c" "network.c" "nortsong.c" "nortvars.c" "opentyr.c" "opl.c" "palette.c" "params.c" "pcxload.c" "pcxmast.c" "picload.c" "player.c" "shots.c" "sizebuf.c" "sndmast.c" "sprite.c" "starlib.c" "tyrian2.c" "varz.c" "vga256d.c" "vga_palette.c" "video.c" "video_scale.c" "video_scale_hqNx.c" "xmas.c"
    INCLUDE_DIRS "."
    REQUIRES "SDL" "SDL_net"
)
