LIB_PATH := ../libcam
BIN_PATH := .

CFLAGS_GLIB  := `pkg-config --cflags glib-2.0`
LDFLAGS_GLIB := `pkg-config --libs glib-2.0 gobject-2.0 gmodule-2.0`

LDFLAGS_LIBCAM := `pkg-config --libs libcam`
LDFLAGS_LIBCAM_GTK := `pkg-config --libs libcam-gtk`

CFLAGS := -std=gnu99 -g $(CFLAGS_GLIB) -I..
LDFLAGS := $(LDFLAGS_LIBCAM) $(LDFLAGS_GLIB) -lGL -ljpeg

snapshot := $(BIN_PATH)/snapshot
obj_snapshot := snapshot.o 

trivial_acquire := $(BIN_PATH)/trivial_acquire
obj_trivial_acquire := trivial-acquire.o 

targets := $(snapshot) $(trivial_acquire)
objects := $(obj_snapshot) $(obj_trivial_acquire)

all: $(targets)

$(snapshot): $(obj_snapshot) 
	@echo "    [$@]"
	$(CC) -o $@ $(obj_snapshot) $(LDFLAGS_LIBCAM) $(LDFLAGS) 

$(trivial_acquire): $(obj_trivial_acquire) 
	@echo "    [$@]"
	$(CC) -o $@ $(obj_trivial_acquire) $(LDFLAGS_LIBCAM) $(LDFLAGS) 

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects)
