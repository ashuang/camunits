// gcc -o camlog2jpeg camlog2jpeg.c `pkg-config --cflags --libs camunits glib-2.0``
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include <camunits/cam.h>

typedef struct {
    GMainLoop *mainloop;
    CamUnit *log_unit;
} state_t;

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf,
        void *user_data)
{
    state_t *state = user_data;

    CamUnitControl *frameno_ctl = 
        cam_unit_find_control (state->log_unit, "frame");

    int frameno = cam_unit_control_get_int (frameno_ctl);
    int max_frameno = cam_unit_control_get_max_int (frameno_ctl);

    char *out_fname = g_strdup_printf ("%d.jpg", frameno);
    FILE *fp = fopen (out_fname, "w");
    if (!fp) {
        fprintf (stderr, "crap\n");
        g_main_loop_quit (state->mainloop);
    }
    fwrite (buf->data, buf->bytesused, 1, fp);
    fclose (fp);
    printf ("%d / %d\n", frameno, max_frameno);
    free (out_fname);

    if (frameno == max_frameno)
        g_main_loop_quit (state->mainloop);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf (stderr, "usage: %s <logfile>\n", argv[0]);
        return 1;
    }

    state_t state = { 
        NULL,
        0
    };

    g_type_init ();

    state.mainloop = g_main_loop_new (NULL, FALSE);

    CamUnitChain *chain = cam_unit_chain_new ();

    state.log_unit = cam_unit_chain_add_unit_by_id (chain, "input.log");
    cam_unit_chain_add_unit_by_id (chain, "convert.to_rgb8");
    cam_unit_chain_add_unit_by_id (chain, "convert.jpeg_compress");

    cam_unit_set_control_string (state.log_unit, "filename", argv[1]);

    g_signal_connect (G_OBJECT (chain), "frame-ready",
            G_CALLBACK (on_frame_ready), &state);

    cam_unit_chain_attach_glib (chain, 1000, NULL);

    cam_unit_chain_all_units_stream_init (chain);

    g_main_loop_run (state.mainloop);

    return 0;
}
