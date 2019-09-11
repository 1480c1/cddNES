UNAME = $(shell uname -s)
MACHINE = $(shell uname -m)

NAME = \
	cddNES

OBJS = \
	src/cart.o \
	src/apu.o \
	src/nes.o \
	src/cpu.o \
	src/ppu.o \
	ui/main.o \
	ui/api.o \
	ui/fs.o \
	ui/args.o \
	ui/settings.o \
	ui/audio.o \
	ui/render/render.o \
	ui/render/gl.o \
	ui/render/ui.o

CFLAGS = \
	-Iui/include \
	-Wall \
	-Wextra \
	-Wno-unused-value \
	-Wno-missing-field-initializers \
	-Wno-switch \
	-Wno-unused-result

CXXFLAGS = \
	$(CFLAGS) \
	-std=c++11

STATIC_LIBS = \
	ui/lib/$(PLATFORM)/libimgui.a \
	ui/lib/$(PLATFORM)/libuncurl.a \
	ui/lib/$(PLATFORM)/libssl.a \
	ui/lib/$(PLATFORM)/libcrypto.a \
	ui/lib/$(PLATFORM)/libcJSON.a

ifeq ($(UNAME), Linux)

ifeq ($(MACHINE), armv7l)

PLATFORM = linux-arm

else

PLATFORM = linux

endif

DYNAMIC_LIBS = \
	-ldl \
	-lstdc++ \
	-lm \
	-lpthread \
	-lGL \
	-lSDL2
endif

ifeq ($(UNAME), Darwin)

export SDKROOT=$(shell xcrun --sdk macosx --show-sdk-path)

PLATFORM = macos

%.o: %.mm
	$(CC) $(CFLAGS)   -c -o $@ $<

OBJS := $(OBJS) \
	ui/render/metal.o \
	ui/render/ui-metal-shim.o

CFLAGS := $(CFLAGS) \
	-DGL_SILENCE_DEPRECATION

DYNAMIC_LIBS = \
	-liconv \
	-lc++ \
	-framework OpenGL \
	-framework CoreAudio \
	-framework AudioToolbox \
	-framework IOKit \
	-framework ForceFeedback \
	-framework Carbon \
	-framework AppKit \
	-framework Metal \
	-framework QuartzCore

STATIC_LIBS := $(STATIC_LIBS) \
	ui/lib/$(PLATFORM)/libSDL2.a

endif

LD_FLAGS = \

ifdef DEBUG
CFLAGS := $(CFLAGS) -O0 -g
LD_FLAGS := $(LD_FLAGS) -O0 -g
else
CFLAGS := $(CFLAGS) -fvisibility=hidden -O3 -flto
LD_FLAGS := $(LD_FLAGS) -fvisibility=hidden -O3 -flto
endif

LD_COMMAND = \
	$(CC) \
	$(OBJS) \
	$(STATIC_LIBS) \
	$(DYNAMIC_LIBS) \
	-o $(NAME) \
	$(LD_FLAGS)

all: clean clear $(OBJS)
	$(LD_COMMAND)

clean:
	rm -rf $(OBJS)

clear:
	clear
