#include <stdio.h>
#include <libraw1394/raw1394.h>

#include "cam.h"

#include "capture_raw1394.h"

struct _CaptureRaw1394 {
    raw1394handle_t raw_handle;
    dc1394camera_t * cam;
    CamUnit * unit;
    CamFrameBuffer * buffer;
    unsigned int iso_bandwidth;
    int iso_channel;
    uint8_t * first_offset;
    int buf_len;
};

static enum raw1394_iso_disposition
raw_iso_recv_handler (raw1394handle_t handle, unsigned char * data,
        unsigned int len, unsigned char channel,
        unsigned char tag, unsigned char sy,
        unsigned int cycle, unsigned int dropped);
static int
allocate_channel_and_bandwidth (CaptureRaw1394 * self);
static int
free_channel_and_bandwidth (CaptureRaw1394 * self);

CaptureRaw1394 *
capture_raw1394_new (dc1394camera_t * cam, CamUnit * unit,
        int packet_size, int num_buffers)
{
    CaptureRaw1394 * self = malloc (sizeof (CaptureRaw1394));
    memset (self, 0, sizeof (CaptureRaw1394));

    self->raw_handle = raw1394_new_handle_on_port (cam->port);
    self->unit = unit;
    self->cam = cam;
    raw1394_set_userdata (self->raw_handle, self);

    if (allocate_channel_and_bandwidth (self) < 0)
        goto fail;

    if (raw1394_iso_recv_init (self->raw_handle, raw_iso_recv_handler,
                num_buffers, packet_size, self->iso_channel,
                RAW1394_DMA_BUFFERFILL, 16) != 0) {
        free_channel_and_bandwidth (self);
        goto fail;
    }

    if (raw1394_iso_recv_start (self->raw_handle, -1, -1, -1) != 0) {
        raw1394_iso_shutdown (self->raw_handle);
        free_channel_and_bandwidth (self);
        goto fail;
    }

    /* The following four lines are the same computation used by
     * libraw1394 to compute how big of a DMA buffer should be allocated
     * to packet capture.  This value is needed in raw_iso_recv_handler()
     * for a naughty hack. */
    int stride;
    for (stride = 4; stride < packet_size; stride *= 2);
    self->buf_len = (stride * num_buffers) & ~(getpagesize()-1);
    self->buf_len = (self->buf_len / getpagesize() - 1) * getpagesize();

    return self;

fail:
    raw1394_destroy_handle (self->raw_handle);
    free (self);
    fprintf (stderr, "Error: Failed to allocate raw1394 ISO recv context\n");
    return NULL;
}

void
capture_raw1394_free (CaptureRaw1394 * self)
{
    raw1394_iso_stop (self->raw_handle);
    raw1394_iso_shutdown (self->raw_handle);
    free_channel_and_bandwidth (self);
    raw1394_destroy_handle (self->raw_handle);
    free (self);
}

int
capture_raw1394_get_fileno (CaptureRaw1394 * self)
{
    return raw1394_get_fd (self->raw_handle);
}

int
capture_raw1394_iterate (CaptureRaw1394 * self)
{
    return raw1394_loop_iterate (self->raw_handle);
}

static enum raw1394_iso_disposition
raw_iso_recv_handler (raw1394handle_t handle, unsigned char * data,
        unsigned int len, unsigned char channel,
        unsigned char tag, unsigned char sy,
        unsigned int cycle, unsigned int dropped)
{
    CaptureRaw1394 * self = raw1394_get_userdata (handle);

    if (!self->first_offset) {
        //printf ("first %p\n", data - 4);
        self->first_offset = data - 4;
    }

    if (dropped && self->buffer) {
        fprintf (stderr, "Warning: dropped = %d, probably dropped frames...\n",
                dropped);
    }

    if (sy) {
        if (!self->buffer)
            self->buffer = 
                cam_framebuffer_new_alloc (self->parent.fmt->max_data_size);
        else
            fprintf (stderr, "Warning: incomplete frame (%d bytes), discarding...\n",
                    self->buffer->bytesused);

        if (!self->buffer) {
            fprintf (stderr, "Warning: ran out of empty buffers, discarding "
                    "frame...\n");
            return RAW1394_ISO_OK;
        }

        self->buffer->bytesused = 0;

        /* Naughty hack to find the ISO trailer (xferStatus/timeStamp)
         * which sits after the end of the packet data.  This data is
         * always 4-byte aligned.  */
        uint8_t * trailer = data + ((len + 3) & ~3);
        /* If the packet wraps around the end of the DMA buffer, fixup
         * our pointer arithmetic. */
        if (trailer >= self->first_offset + self->buf_len)
            trailer -= self->buf_len;

        self->buffer->bus_timestamp = (trailer[0] << 12) | (trailer[1] << 20);

        /* We will compute the real timestamp later */
        self->buffer->timestamp = 0;
    }
    
    if (!self->buffer)
        return RAW1394_ISO_OK;

    if (self->buffer->bytesused + len > self->buffer->length)
        len = self->buffer->length - self->buffer->bytesused;

    memcpy (self->buffer->data + self->buffer->bytesused, data, len);
    self->buffer->bytesused += len;

    self->buffer->source_uid = self->cam->euid_64;

    if (self->buffer->bytesused == self->buffer->length) {
        cam_unit_produce_frame (super, self->buffer, self->parent.fmt);
        g_object_unref (self->buffer);
        self->buffer = NULL;
    }

    return RAW1394_ISO_OK;
}


static int
allocate_channel_and_bandwidth (CaptureRaw1394 * self)
{
    int i;

    self->iso_channel = -1;
    for (i = 0; i < 16; i++) {
        if (raw1394_channel_modify (self->raw_handle, i,
                    RAW1394_MODIFY_ALLOC) == 0) {
            self->iso_channel = i;
            printf ("DC1394: Allocated channel %d\n", i);
            break;
        }
    }

    if (self->iso_channel < 0) {
        fprintf (stderr, "Error: Failed to allocate ISO channel\n");
        return -1;
    }

    if (dc1394_video_set_iso_channel (self->cam, self->iso_channel) !=
            DC1394_SUCCESS) {
        fprintf (stderr, "Error: Failed to set camera's ISO channel\n");
        goto abort;
    }

    if (dc1394_video_get_bandwidth_usage (self->cam, &self->iso_bandwidth) !=
            DC1394_SUCCESS) {
        fprintf (stderr, "Error: Failed to estimate ISO bandwidth\n");
        goto abort;
    }

    if (raw1394_bandwidth_modify (self->raw_handle, self->iso_bandwidth,
                RAW1394_MODIFY_ALLOC) != 0) {
        fprintf (stderr, "Error: Failed to allocate ISO bandwidth\n");
        goto abort;
    }
    printf ("DC1394: Allocated %d bandwidth\n", self->iso_bandwidth);

    return 0;

abort:
    raw1394_channel_modify (self->raw_handle, self->iso_channel,
            RAW1394_MODIFY_FREE);
    return -1;
}

static int
free_channel_and_bandwidth (CaptureRaw1394 * self)
{
    if (raw1394_bandwidth_modify (self->raw_handle,
                self->iso_bandwidth, RAW1394_MODIFY_FREE) != 0)
        fprintf (stderr, "Warning: Failed to free ISO bandwidth\n");
    else
        printf ("DC1394: Freed %d bandwidth\n", self->iso_bandwidth);

    if (raw1394_channel_modify (self->raw_handle, self->iso_channel,
                RAW1394_MODIFY_FREE) != 0)
        fprintf (stderr, "Warning: Failed to free ISO channel\n");
    else
        printf ("DC1394: Freed channel %d\n", self->iso_channel);

    self->iso_channel = -1;

    return 0;
}


