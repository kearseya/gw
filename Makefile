TARGET = gw

CXXFLAGS += -Wall -std=c++17 -fno-common -dynamic -fwrapv -O3 -DNDEBUG -pipe # -g

CPPFLAGS += -I./include -I./src -I.

LDLIBS += -lskia -lm -ldl -ljpeg -lpng -lsvg -lhts -lfontconfig -lpthread

# set system
PLATFORM=
ifeq ($(OS),Windows_NT)  # assume we are using msys2-ucrt64 env
    PLATFORM = "Windows"
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM = "Linux"
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM = "Darwin"
    endif
    ifeq ($(UNAME_S),arm64)
        PLATFORM = "Arm64"
    endif
endif

# try and add conda environment
ifneq ($(CONDA_PREFIX),"")
    CPPFLAGS += -I$(CONDA_PREFIX)/include
    LDFLAGS += -L$(CONDA_PREFIX)/lib
endif

# Options to use target htslib or skia
HTSLIB ?= ""
ifneq ($(HTSLIB),"")
    CPPFLAGS += -I$(HTSLIB)
    LDFLAGS += -L$(HTSLIB)
endif

SKIA ?= ""
ifneq ($(PLATFORM), "Windows")
    ifneq ($(SKIA),"")
        CPPFLAGS += -I$(SKIA)
        LDFLAGS += -L $(wildcard $(SKIA)/out/Rel*)
    else
        CPPFLAGS += -I./lib/skia
        LDFLAGS += -L./lib/skia/out/Release-x64
    endif
endif

SKIA_LINK=
ifeq ($(PLATFORM),"Linux")
    SKIA_LINK = https://github.com/JetBrains/skia-build/releases/download/m93-87e8842e8c/Skia-m93-87e8842e8c-linux-Release-x64.zip
endif
ifeq ($(PLATFORM),"Darwin")
    SKIA_LINK = https://github.com/JetBrains/skia-build/releases/download/m93-87e8842e8c/Skia-m93-87e8842e8c-macos-Release-x64.zip
endif
ifeq ($(PLATFORM),"Arm64")
    SKIA_LINK = https://github.com/JetBrains/skia-build/releases/download/m93-87e8842e8c/Skia-m93-87e8842e8c-macos-Release-arm64.zip
endif

# set platform flags and libs
ifeq ($(PLATFORM),"Linux")
    ifeq (${XDG_SESSION_TYPE},"wayland")  # wayland is untested!
        LDLIBS += -lwayland-client
    else
        LDLIBS += -lX11
    endif
    CPPFLAGS += -I/usr/local/include
    CXXFLAGS += -D LINUX -D __STDC_FORMAT_MACROS
    LDFLAGS += -L/usr/local/lib
    LDLIBS += -lGL -lfreetype -lfontconfig -luuid -lglfw -lzlib -licu

else ifeq ($(PLATFORM),"Darwin")
    CPPFLAGS += -I/usr/local/include
    CXXFLAGS += -D OSX -stdlib=libc++ -arch x86_64 -fvisibility=hidden -mmacosx-version-min=10.15 -Wno-deprecated-declarations
    LDFLAGS += -undefined dynamic_lookup -framework OpenGL -framework AppKit -framework ApplicationServices -mmacosx-version-min=10.15 -L/usr/local/lib
    LDLIBS += -lglfw -lzlib -licu

else ifeq ($(PLATFORM),"Arm64")
    CPPFLAGS += -I/usr/local/include
    CXXFLAGS += -D OSX -stdlib=libc++ -arch arm64 -fvisibility=hidden -mmacosx-version-min=10.15 -Wno-deprecated-declarations
    LDFLAGS += -undefined dynamic_lookup -framework OpenGL -framework AppKit -framework ApplicationServices -mmacosx-version-min=10.15 -L/usr/local/lib
    LDLIBS += -lglfw -lzlib -licu

else ifeq ($(PLATFORM),"Windows")
    CXXFLAGS += -D WIN32
    CPPFLAGS += $(shell pkgconf -cflags skia)
    LDFLAGS += -L/ucrt64/lib -L/home/nopel/gw_build/ucrt64/lib
    LDLIBS += $(shell pkgconf -libs skia)
    LDLIBS += -lharfbuzz-subset -lglfw3
endif

.PHONY: default all debug clean

default: $(TARGET)

all: default
debug: default

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard ./src/*.cpp))

# download link for skia binaries, set for non-Windows platforms
prep:
    ifneq ($SKIA_LINK,"")
		$(info "Downloading pre-build skia skia from: $(SKIA_LINK)")
		cd lib/skia && wget -O skia.zip $(SKIA_LINK) && unzip -o skia.zip && rm skia.zip && cd ../../
    endif

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -g $(CPPFLAGS) -c $< -o $@

#.PRECIOUS: $(TARGET) $(OBJECTS)


$(TARGET): $(OBJECTS)
	$(CXX) -g $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	-rm -f *.o ./src/*.o ./src/*.o.tmp
	-rm -f $(TARGET)
