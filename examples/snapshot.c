#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <glib.h>

#include <libcam/cam.h>

typedef struct _state_t {
    GMainLoop *mainloop;
} state_t;

int pgm_write (FILE *fp, const uint8_t *pixels,
        int width, int height, int rowstride)
{
    fprintf(fp, "P5\n%d\n%d\n%d\n", width, height, 255);
    int i, count;
    for (i=0; i<height; i++){
        count = fwrite(pixels + i*rowstride, width, 1, fp);
        if (1 != count) return -1;
    }
    return 0;
}

int ppm_write (FILE *fp, const uint8_t *pixels,
        int width, int height, int rowstride)
{
    fprintf(fp, "P6 %d %d %d\n", width, height, 255);
    int i, count;
    for (i=0; i<height; i++){
        count = fwrite(pixels + i*rowstride, width*3, 1, fp);
        if (1 != count) return -1;
    }
    return 0;
}

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, 
        const CamFrameBuffer *buf, void *user_data)
{
    state_t *s = user_data;
    FILE *fp = fopen ("snapshot-output.pgm", "wb");
    if (!fp) {
        perror ("fopen");
        g_main_loop_quit (s->mainloop);
        return;
    }

    // write the image to disk
    const CamUnitFormat *fmt = cam_unit_get_output_format (unit);
    pgm_write (fp, buf->data, fmt->width, fmt->height, fmt->row_stride);
    
    fclose (fp);
    g_main_loop_quit (s->mainloop);
}

int main(int argc, char **argv)
{
    printf("camunit_tester\n");

    g_type_init();

    state_t s;
    memset (&s, 0, sizeof (s));

    // create the GLib mainloop
    GMainLoop *mainloop = g_main_loop_new (NULL, FALSE);

    // create the image processing chain
    CamUnitChain * chain = cam_unit_chain_new();

    // create an input unit
    cam_unit_chain_add_unit_by_id(chain, "example_input:0");

    // create a colorspace converter
    CamUnit *color_converter = cam_unit_chain_add_unit_by_id(chain, 
                    "convert:colorspace");

    // example_input always generates RGB, so convert it to grayscale for kicks
    cam_unit_set_preferred_format (color_converter,
            CAM_PIXEL_FORMAT_GRAY, 0, 0);

    // start the image processing chain
    cam_unit_chain_set_desired_status (chain, CAM_UNIT_STATUS_STREAMING);

    // attach the chain to the glib event loop.
    cam_unit_chain_attach_glib_mainloop (chain, 1000);

    // subscribe to be notified when an image has made its way through the
    // chain
    g_signal_connect (G_OBJECT (chain), "frame-ready",
            G_CALLBACK (on_frame_ready), &s);

    // run 
    g_main_loop_run (mainloop);

    // cleanup
    g_main_loop_unref (mainloop);

    cam_unit_chain_set_desired_status (chain, CAM_UNIT_STATUS_IDLE);

    g_object_unref (chain);

    return 0;
}
