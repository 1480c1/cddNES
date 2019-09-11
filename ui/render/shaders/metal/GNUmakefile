all: clear
	xcrun -sdk macosx metal -std=macos-metal1.2 -mmacosx-version-min=10.9 -c shaders.metal -o shaders.air
	xcrun -sdk macosx metallib shaders.air -o shaders.metallib
	python bin2c.py shaders.metallib library.h MTL_LIBRARY
	rm shaders.air
	rm shaders.metallib

clear:
	clear
