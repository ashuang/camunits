LIB_PATH := ../libcam
BIN_PATH := .

CFLAGS_GLIB  := `pkg-config --cflags glib-2.0`
LDFLAGS_GLIB := `pkg-config --libs glib-2.0`

CFLAGS := -std=gnu99 -g $(CFLAGS_GLIB) -I..
LDFLAGS := $(LDFLAGS_GLIB) ../libcam/libcam.a ../libcam-gtk/libcam-gtk.a \
		-lGL -lGLU -lglut -ldc1394 -ljpeg

snapshot := $(BIN_PATH)/snapshot
obj_snapshot := snapshot.o 

targets := snapshot
objects := $(obj_snapshot)

all: $(targets)

$(snapshot): $(obj_snapshot) 
	@echo "    [$@]"
	$(CC) -o $@ $(obj_snapshot) $(LDFLAGS_LIBCAM) $(LDFLAGS) 

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects)
