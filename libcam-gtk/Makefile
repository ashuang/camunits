CFLAGS_STD   := -std=gnu99 -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_REENTRANT -Wall -Wno-unused-parameter -Wno-format-zero-length -pthread
LDFLAGS_STD  := -lm
CFLAGS_GLIB  := `pkg-config --cflags glib-2.0 gmodule-2.0`
LDFLAGS_GLIB := `pkg-config --libs glib-2.0 gmodule-2.0 gthread-2.0 gobject-2.0`
CFLAGS_GTK   :=`pkg-config --cflags gtk+-2.0`
LDFLAGS_GTK  :=`pkg-config --libs gtk+-2.0 gthread-2.0`
LIB_PATH := .

CFLAGS := -I.. $(CFLAGS_STD) $(CFLAGS_GTK) $(CFLAGS_GLIB) \
		 -ffast-math -msse2 -msse3 \
		 -DLIBCAM_PLUGINS_PATH='"$(LIBCAM_PLUGINS_PATH)"' 

static_lib:=$(LIB_PATH)/libcam.a
shared_lib:=$(LIB_PATH)/libcam.so

obj_lib := 	unit_control_widget.o \
	unit_chain_widget.o unit_manager_widget.o \
	unit_chain_gl_widget.o gl_drawing_area.o \

targets:=$(static_lib) 
objects:=$(obj_lib) 

all: $(targets)

$(static_lib): $(obj_lib) 
	@echo "    [$@]"
	ar rc $@ $^

$(shared_lib): $(obj_lib)
	@echo "    [$@]"
	$(LD) $(LDFLAGS_LIB) $(LDSH) $(obj_lib) -o $@

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects)
