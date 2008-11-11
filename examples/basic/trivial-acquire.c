#include <stdio.h>

#include <glib.h>
#include <camunits/cam.h>

static void on_frame_ready (CamUnitChain *chain, CamUnit *unit, 
        const CamFrameBuffer *buf, void *user_data)
{
    GMainLoop *mainloop = (GMainLoop*) user_data;
    const CamUnitFormat *fmt = cam_unit_get_output_format (unit);
    FILE *fp = fopen ("trivial-acquire.ppm", "wb");
    if (fp) {
        fprintf(fp, "P6 %d %d %d\n", fmt->width, fmt->height, 255);
        for (int i=0; i<fmt->height; i++){
            int count = fwrite(buf->data + i*fmt->row_stride, 
                    fmt->width*3, 1, fp);
            if (1 != count) {
                perror ("fwrite");
                break;
            }
        }
        fclose (fp);
    }
    printf ("wrote to trivial-acquire.ppm\n");
    g_main_quit (mainloop);
}

int main(int argc, char **argv)
{
    g_type_init();

    // create the GLib event handling loop
    GMainLoop *mainloop = g_main_loop_new (NULL, FALSE);

    // create an image processing chain and add a single example unit
    CamUnitChain * chain = cam_unit_chain_new ();
    cam_unit_chain_add_unit_by_id (chain, "input.example");

    // start the chain
    cam_unit_chain_all_units_stream_init (chain);

    // attach the chain to the glib event loop.
    cam_unit_chain_attach_glib (chain, 1000, NULL);

    // subscribe to be notified when an image has made its way through the
    // chain
    g_signal_connect (G_OBJECT (chain), "frame-ready",
            G_CALLBACK (on_frame_ready), mainloop);

    // run 
    g_main_loop_run (mainloop);

    // cleanup
    g_main_loop_unref (mainloop);
    g_object_unref (chain);
    return 0;
}
