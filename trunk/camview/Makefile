LIB_PATH := ../libcam
BIN_PATH := .

CFLAGS_GTK  := `pkg-config --cflags gtk+-2.0`
LDFLAGS_GTK := `pkg-config --libs gtk+-2.0 gthread-2.0`

CFLAGS := -std=gnu99 -g $(CFLAGS_GTK) -I..
LDFLAGS := $(LDFLAGS_GTK) ../libcam/libcam.a ../libcam-gtk/libcam-gtk.a \
		-lGL -lGLU -lglut -ldc1394 -ljpeg

camview := $(BIN_PATH)/camview
obj_camview := camview.o gtk_util.o

targets := camview
objects := $(obj_camview)

all: $(targets)

$(camview): $(obj_camview) 
	@echo "    [$@]"
	$(CC) -o $@ $(obj_camview) $(LDFLAGS_LIBCAM) $(LDFLAGS) 

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects)
