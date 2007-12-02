#ifndef _CAPTURE_RAW1394_H_
#define _CAPTURE_RAW1394_H_

#include <dc1394/control.h>
#include "unit.h"

typedef struct _CaptureRaw1394 CaptureRaw1394;

CaptureRaw1394 *
capture_raw1394_new (dc1394camera_t * cam, CamUnit * unit,
        int packet_size, int num_buffers);
void
capture_raw1394_free (CaptureRaw1394 * self);
int
capture_raw1394_get_fileno (CaptureRaw1394 * self);
int
capture_raw1394_iterate (CaptureRaw1394 * self);

#endif
