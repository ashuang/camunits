BIN_PATH := .

CFLAGS_GTK  := `pkg-config --cflags glib-2.0 gtk+-2.0`
LDFLAGS_GTK := `pkg-config --libs glib-2.0 gobject-2.0 gmodule-2.0 gtk+-2.0`

CFLAGS_LIBCAM := `pkg-config --cflags libcam`
LDFLAGS_LIBCAM := `pkg-config --libs libcam libcam-gtk`

CFLAGS := -std=gnu99 -g $(CFLAGS_GTK) $(CFLAGS_LIBCAM)
LDFLAGS := $(LDFLAGS_LIBCAM) $(LDFLAGS_GTK) $(LDFLAGS_GTK) -lGL -ljpeg

filter_example := $(BIN_PATH)/filter_example
obj_filter_example := main.o filter_example.o

targets := $(filter_example)
objects := $(obj_filter_example)

all: $(targets)

$(filter_example): $(obj_filter_example) 
	@echo "    [$@]"
	$(CC) -o $@ $(obj_filter_example) $(LDFLAGS_LIBCAM) $(LDFLAGS) 

%.o: %.c %.h
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

%.o: %.c
	@echo "    [$@]"
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(targets) $(objects)
