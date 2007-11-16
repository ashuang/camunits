LIB_PATH := ../libcam
BIN_PATH := .

CFLAGS_GLIB  := `pkg-config --cflags glib-2.0`
LDFLAGS_GLIB := `pkg-config --libs glib-2.0 gobject-2.0 gmodule-2.0`

LDFLAGS_LIBCAM := `pkg-config --libs libcam`
LDFLAGS_LIBCAM_GTK := `pkg-config --libs libcam-gtk`

CFLAGS := -std=gnu99 -g $(CFLAGS_GLIB) -I..
LDFLAGS := $(LDFLAGS_LIBCAM) $(LDFLAGS_GLIB) -lGL -lGLU -lglut -ljpeg

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
