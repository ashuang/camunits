LIB_PATH := .
BIN_PATH := .

LIBCAM_PLUGINS_PATH:=$(BIN_PATH)/libcam

CFLAGS_GLIB  := `pkg-config --cflags glib-2.0 gmodule-2.0`
LDFLAGS_GLIB := `pkg-config --libs glib-2.0 gmodule-2.0 gthread-2.0 gobject-2.0`

CFLAGS_DC1394:=-DENABLE_DC1394
ifeq "$(shell uname -s)" "Darwin"
LDFLAGS_DC1394:=-ldc1394 -L/usr/local/lib
else
LDFLAGS_DC1394:=-ldc1394 
endif

CFLAGS := -std=gnu99 -g -D_REENTRANT -Wall -Wno-unused-parameter \
	-Wno-format-zero-length -pthread -fPIC $(CFLAGS_GLIB) \
	-ffast-math -msse2 -msse3 \
	-DLIBCAM_PLUGINS_PATH='"$(LIBCAM_PLUGINS_PATH)"' 

static_lib:=$(LIB_PATH)/libcam.a
#shared_lib:=$(LIB_PATH)/libcam.so

obj_lib := unit.o unit_driver.o unit_manager.o unit_chain.o framebuffer.o \
		unit_format.o unit_control.o libcam-gmarshal.o plugin.o pixels.o \
		input_example.o \
		filter_gl.o gl_texture.o filter_color_convert.o \
		filter_fast_bayer.o \
		filter_jpeg.o \
#		unit_control_widget.o \
#		unit_chain_widget.o unit_manager_widget.o \
#		unit_chain_gl_widget.o gl_drawing_area.o \

ENABLE_DC1394:=1
obj_lib += filter_bayer.o input_dc1394.o 
CFLAGS += $(CFLAGS_DC1394)
LDFLAGS += $(LDFLAGS_DC1394)

ifeq "$(shell uname -s)" "Linux"
	obj_lib += input_v4l2.o input_v4l.o
	CFLAGS += -DUSE_V4L2 -DUSE_V4L
endif

targets:=$(static_lib) 
objects:=$(obj_lib) 

all: $(targets)

$(static_lib): $(obj_lib) 
	@echo "    [$@]"
	ar rc $@ $^

#$(shared_lib): $(obj_lib)
#	@echo "    [$@]"
#	$(LD) $(LDFLAGS_LIB) $(LDSH) $(obj_lib) -o $@

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects) $(LIBCAM_PLUGINS_PATH)/*$(SHEXT)
