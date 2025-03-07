cd ..
idf.py build
cd release_esp32
/Applications/ffmpeg -i Tile.png -f rawvideo -pix_fmt rgb565 tile.raw
/Users/andyvand/Documents/GitHub/odroid-go-firmware-20181001/tools/mkfw/mkfw OpenTyrian20000 tile.raw 0 16 1048576 app ../build/OpenTyrian2000.bin
rm OpenTyrian2000.fw
mv firmware.fw OpenTyrian2000.fw
/Users/andyvand/Documents/GitHub/odroid-go-firmware-20181001/tools/mkfw/mkfw OpenTyrian2000 tile.raw 0 16 1048576 app ../build/OpenTyrian2000.bin
rm OpenTyrian2000-20181001.fw
mv firmware.fw OpenTyrian2000-20181001.fw
