#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

#include <dc1394/control.h>

static void
usage( const char *progname )
{
    fprintf(stderr, 
        "Usage: %s [OPTIONS]\n"
        "\n"
        "Issues a software trigger to all cameras on the firewire bus and\n"
        "then quits.  Use the -r option for continuous triggering.\n"
        "\n"
        "Options:\n"
        " -h      Shows this help text\n"
        " -r RATE Triggers at RATE Hz without terminating\n"
        " -v      Be verbose\n\n",
        progname);
}

static int do_quit = 0;

static void
sig_handler (int sig)
{
    do_quit = 1;
}

int main(int argc, char **argv)
{
    int verbose = 0;
    int one_shot = 1;
    double rate;

    setlinebuf (stdout);
    setlinebuf (stderr);

    char *optstring = "hr:v";
    char c;
    struct option long_opts[] = { 
        { "help", no_argument, 0, 'h' },
        { "rate", required_argument, 0, 'r' },
        { "verbose", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
    };

    while( (c = getopt_long( argc, argv, optstring, long_opts, 0 )) >= 0 )
    {
        switch( c ) {
            case 'r':
                one_shot = 0;
                rate = strtof (optarg, NULL);
                printf ("Triggering at %f Hz\n", rate);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                usage( argv[0] );
                return 1;
        };
    }

    dc1394camera_t ** cameras;
    unsigned int num_cameras = 0;
    dc1394error_t err = dc1394_find_cameras (&cameras, &num_cameras);
    if (err != DC1394_SUCCESS) {
        fprintf (stderr, "Error: %s\n", dc1394_error_strings[err]);
        return 1;
    }

    if (num_cameras == 0) {
        fprintf (stderr, "No cameras found\n");
        return 0;
    }

    dc1394_camera_set_broadcast (cameras[0], DC1394_ON);
    if (verbose)
        printf ("Triggering...\n");
    dc1394_software_trigger_set_power (cameras[0], DC1394_ON);

    if (one_shot)
        goto abort;

    signal (SIGINT, sig_handler);
    while (!do_quit) {
        usleep (1000000 / rate);
        dc1394_software_trigger_set_power (cameras[0], DC1394_ON);
        if (verbose) {
            printf (".");
            fflush (stdout);
        }
    }
    printf ("\n");

abort:
    for (int i = 0; i < num_cameras; i++)
        dc1394_free_camera (cameras[i]);
    free (cameras);
    return 0;
}
