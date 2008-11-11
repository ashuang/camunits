#include <QApplication>

#include "CamUnitChainQtAdapter.hpp"

struct State {
    CamUnitChain *chain;
    CamUnitChainQtAdapter *chain_adapter;
    QApplication *app;
    int write_count;
    char filename[4096];
};

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf, 
        void *user_data)
{
    State *state = (State*) user_data;

    if (state->write_count) return;
    FILE *fp = fopen (state->filename, "wb");
    if (!fp) {
        perror ("fopen");
        state->app->quit();
        return;
    }

    // write the image to disk as a PPM
    const CamUnitFormat *fmt = cam_unit_get_output_format (unit);
    fprintf(fp, "P6 %d %d %d\n", fmt->width, fmt->height, 255);
    for (int i=0; i<fmt->height; i++){
        int count = fwrite(buf->data + i*fmt->row_stride, fmt->width*3, 1, fp);
        if (1 != count) {
            perror ("fwrite");
        }
    }
    fclose (fp);
    printf ("wrote %s\n", state->filename);

    state->write_count++;
    state->app->quit();
    (void)chain;
}

int main(int argc, char **argv)
{
    // create the Qt event loop and application
    QApplication app(argc, argv);

    // abort if no input unit was specified
    if (argc < 2) {
        fprintf (stderr, "usage: %s <camunits_unit_id>\n", argv[0]);
        return 1;
    }

    // initialize the GLib type system
    g_type_init();

    // create the image processing chain
    State* state = new State();;
    state->chain = cam_unit_chain_new();
    state->app = &app;
    sprintf (state->filename, "snapshot-output.ppm");
    
    // instantiate the input unit
    CamUnit *first_unit = cam_unit_chain_add_unit_by_id (state->chain, argv[1]);
    if (!first_unit) {
        fprintf (stderr, "couldn't create unit.\n");
        cam_unit_chain_all_units_stream_shutdown (state->chain);
        g_object_unref (state->chain);
        delete state;
        return 1;
    }

    // create a unit to convert the input data to 8-bit RGB
    cam_unit_chain_add_unit_by_id (state->chain, "convert.to_rgb8");

    // start the image processing chain
    CamUnit *faulty_unit = cam_unit_chain_all_units_stream_init (state->chain);

    // did everything start up correctly?
    if (faulty_unit) {
        fprintf (stderr, "Unit [%s] is not streaming, aborting...\n",
                cam_unit_get_name (faulty_unit));
        cam_unit_chain_all_units_stream_shutdown (state->chain);
        g_object_unref (state->chain);
        delete state;
        return 1;
    }

    // register a GLib signal handler to be called when a frame is ready
    g_signal_connect (G_OBJECT(state->chain), "frame-ready",
            G_CALLBACK(on_frame_ready), state);

    // attach the unit chain to the Qt event loop
    state->chain_adapter = new CamUnitChainQtAdapter (state->chain);

    app.exec();

    cam_unit_chain_all_units_stream_shutdown (state->chain);
    g_object_unref (state->chain);
    delete state->chain_adapter;
    delete state;
    return 0;
}
