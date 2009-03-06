#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>

#include <glib.h>

#include <camunits/cam.h>

#include "signal_pipe.h"

typedef struct _state_t {
    int verbose;
    int frameno;
    int64_t lasttime;
} state_t;

#define FRAMES_PER_PRINTF   100

static int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

static void
on_frame_ready (CamUnitChain *chain, CamUnit *unit, const CamFrameBuffer *buf, 
        void *user_data)
{
    state_t *self = user_data;

    if (self->verbose) {
        printf("frame: %d\n", self->frameno);
    }
    if (self->frameno % FRAMES_PER_PRINTF == 0) {
        int64_t timestamp;
        timestamp = _timestamp_now ();
        if (self->lasttime != 0) {
            printf ("%d frames at %.1f Hz\n", self->frameno,
                    FRAMES_PER_PRINTF * 1000000.0 /
                    (timestamp - self->lasttime));
        }
        self->lasttime = timestamp;
    }
    self->frameno++;
}

static void
print_inputs ()
{
    CamUnitManager *manager = cam_unit_manager_get_and_ref();
    fprintf(stderr, "Units available in package input:\n\n"); 
    GList *udlist = cam_unit_manager_list_package (manager, "input", TRUE);
    for (GList *uditer=udlist; uditer; uditer=uditer->next) {
        CamUnitDescription *udesc = CAM_UNIT_DESCRIPTION(uditer->data);
        printf("  %s  (%s)\n", 
                cam_unit_description_get_unit_id(udesc),
                cam_unit_description_get_name(udesc));
    }
    g_list_free(udlist);
    g_object_unref(manager);
}

static void
usage()
{
    fprintf(stderr, 
        "Usage: camlog [OPTIONS]\n"
        "\n"
        "camlog is a tool for writing video data to disk, primarily to save\n"
        "data for post-processing and analysis.  The video source written to\n"
        "disk is determined by specifying either the -i or -c flag -- exactly\n"
        "one of these must be specified.  Logfiles produced by camlog are\n"
        "suitable for loading with the Camunits input.log unit\n"
        "\n"
        "Using the -i option is simpler, and is useful for dealing with an\n"
        "input unit where the default settings are sufficient.  If the default\n"
        "unit settings are not sufficient, or pre-log processing is desired,\n"
        "then use the -c option and specify a chain description file as produced\n"
        "by camview or the cam_unit_chain_snapshot() function.\n"
        "\n"
        "Options:\n"
        " -h, --help          Shows this help text\n"
        " -i, --input ID      Use the specified Camunit ID as input.\n"
        " -c, --chain NAME    Load chain from file NAME.\n"
        "\n"
        " -o, --output FILE   Saves result to filename FILE.  If not specified, a\n"
        "                     filename is automatically chosen.\n"
        " -f, --force         Force overwrite of output_file if it already exists\n"
        "                     If not specified, and the output file already\n"
        "                     exists, then a suffix is automatically appended\n"
        "                     to the filename to prevent overwriting existing\n"
        "                     files.\n"
        " -n, --no-write      Do not write video data to disk.  Useful for testing.\n"
        " -v, --verbose       Print information about each frame.\n\n"
        " --plugin-path PATH  Add the directories in PATH to the plugin\n"
        "                     search path.  PATH should be a colon-delimited\n"
        "                     list.\n");
}

int main(int argc, char **argv)
{
    int status = 1;

    char *log_fname = NULL;
    char *input_id = NULL;
    char *chain_fname = NULL;
    int overwrite = 0;
    int do_logging = 1;
    GMainLoop *mainloop = NULL;
    char *extra_plugin_path = NULL;
    state_t *self = (state_t*)calloc(1, sizeof(state_t));
    self->verbose = 0;
    self->frameno = 0;

    setlinebuf (stdout);
    setlinebuf (stderr);

    char *optstring = "hi:c:o:fnvp:";
    int c;
    struct option long_opts[] = { 
        { "help", no_argument, 0, 'h' },
        { "input", required_argument, 0, 'i' },
        { "chain", required_argument, 0, 'c' },
        { "output", no_argument, 0, 'o' },
        { "force", no_argument, 0, 'f' },
        { "no-write", no_argument, 0, 'n' },
        { "verbose", no_argument, 0, 'v' },
        { "plugin-path", no_argument, 0, 'p' },
        { 0, 0, 0, 0 }
    };

    g_type_init();

    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0)
    {
        switch (c) {
            case 'i':
                free(input_id);
                input_id = strdup(optarg);
                break;
            case 'c':
                free(chain_fname);
                chain_fname = strdup(optarg);
                break;
            case 'o':
                free(log_fname);
                log_fname = strdup(optarg);
                break;
            case 'f':
                overwrite = 1;
                break;
            case 'n':
                do_logging = 0;
                break;
            case 'v':
                self->verbose = 1;
                break;
            case 'p':
                extra_plugin_path = strdup (optarg);
                break;
            case 'h':
            default:
                usage();
                return 1;
        };
    }

    // setup the image processing chain
    CamUnitChain * chain = cam_unit_chain_new();

    // search for plugins in non-standard directories
    if(extra_plugin_path) {
        CamUnitManager *manager = cam_unit_manager_get_and_ref();
        char **path_dirs = g_strsplit(extra_plugin_path, ":", 0);
        for (int i=0; path_dirs[i]; i++) {
            cam_unit_manager_add_plugin_dir (manager, path_dirs[i]);
        }
        g_strfreev (path_dirs);
        free(extra_plugin_path);
        extra_plugin_path = NULL;
        g_object_unref(manager);
    }

    if (!input_id && !chain_fname) {
        usage(); 
        print_inputs();
        goto done;
    } else if(input_id && chain_fname) {
        fprintf(stderr, "Only one of -c and -i can be specified\n");
        usage();
        goto done;
    }

    // create the GLib mainloop
    mainloop = g_main_loop_new (NULL, FALSE);
    signal_pipe_glib_quit_on_kill (mainloop);

    // instantiate the input unit
    if(input_id) {
        CamUnit *input = cam_unit_chain_add_unit_by_id(chain, input_id);
        if (! input) {
            fprintf(stderr, "CRAP!  Couldn't create input unit [%s].\n\n", 
                    input_id);
            print_inputs();
            goto done;
        }

        cam_unit_stream_init (input, NULL);
    } else {
        char *xml_str = NULL;
        if(g_file_get_contents(chain_fname, &xml_str, NULL, NULL)) {
            cam_unit_chain_load_from_str(chain, xml_str, NULL);
        }
        free(xml_str);
    }

    CamUnit *logger_unit = NULL;
    if (do_logging) {
        // create the logger unit and add it to the chain.
        logger_unit = cam_unit_chain_add_unit_by_id(chain, 
                "output.logger");
        assert(logger_unit);

        // set the filename, and start the logger unit recording
        if (log_fname) {
            cam_unit_set_control_string(logger_unit, "desired-filename", 
                    log_fname);
            cam_unit_set_control_boolean(logger_unit, "auto-suffix-enable",
                    !overwrite);
        }
        cam_unit_set_control_boolean (logger_unit, "record", TRUE);

        // print the actual filename
        char *fname = g_object_get_data(G_OBJECT(logger_unit), 
                "actual-filename");
        if(fname) 
            fprintf(stderr, "logging to: %s\n", fname);

    } else {
        fprintf (stderr, "not logging to disk\n");
    }

    // start the chain streaming
    CamUnit *faulty_unit = cam_unit_chain_all_units_stream_init (chain);

    // did everything start up correctly?
    if (faulty_unit) {
        fprintf (stderr, "Unit [%s] is not ready, aborting...\n",
                cam_unit_get_name (faulty_unit));
        goto done;
    }

    cam_unit_chain_attach_glib (chain, 1000, NULL);
    g_signal_connect (G_OBJECT (chain), "frame-ready",
            G_CALLBACK (on_frame_ready), self);

    // run the main loop
    g_main_loop_run (mainloop);

    // stop recording
    if (log_fname && logger_unit)
        cam_unit_set_control_boolean (logger_unit, "record", FALSE);

    // cleanup
    status = 0;
done:
    if (mainloop) g_main_loop_unref (mainloop);
    if (chain) {
        cam_unit_chain_all_units_stream_shutdown (chain);
        g_object_unref (chain);
    }
    free(input_id);
    free(log_fname);
    free(chain_fname);
    if (self) {
        free (self);
    }
    return status;
}
