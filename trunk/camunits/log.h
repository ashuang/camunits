#ifndef __cam_log_h__
#define __cam_log_h__

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>

#include "framebuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SECTION:log
 * @short_description: Class for reading and writing Camunits log files.
 */
typedef struct _CamLog CamLog;

typedef struct _CamLogFrameFormat {
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint32_t pixelformat;
} CamLogFrameFormat;

typedef struct _CamLogFrameInfo {
    uint64_t timestamp;
    uint64_t frameno;
    int64_t offset;
    uint64_t data_len;
    int64_t data_offset;
} CamLogFrameInfo;

/**
 * cam_log_new:
 * @fname: the file to read or create
 * @mode:  either "r" or "w"
 *
 * constructor
 */
CamLog* cam_log_new (const char *fname, const char *mode);

void cam_log_destroy (CamLog *self);

int cam_log_next_frame (CamLog * self);
int cam_log_prev_frame (CamLog * self);

int cam_log_get_frame_format (CamLog * self, CamLogFrameFormat * format);
int cam_log_get_frame_info (CamLog * self, CamLogFrameInfo * info);
CamFrameBuffer * cam_log_get_frame (CamLog * self);

int cam_log_write_frame (CamLog * self, CamLogFrameFormat * format,
        CamFrameBuffer * frame, int64_t * offset);

/**
 * cam_log_count_frames:
 *
 * Returns: the total number of frames in the logfile
 */
int cam_log_count_frames (CamLog *self);

/**
 * cam_log_seek_to_frame:
 *
 * Positions the file pointer of the cam_log such that the next call to
 * cam_log_read_next_frame reads frame number %frameno
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_log_seek_to_frame (CamLog *self, int frameno);

/**
 * cam_log_seek_to_offset:
 *
 * Positions the file pointer of the cam_log at the start of the first frame
 * with file offset greater than or equal to %file_offset
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_log_seek_to_offset (CamLog *self, int64_t file_offset);

/**
 * cam_log_seek_to_timestamp:
 *
 * Positions the file pointer of the cam_log at the start of the first frame
 * with timestamp greater or equal to than %timestamp
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_log_seek_to_timestamp (CamLog *self, int64_t timestamp);

/**
 * cam_log_get_file_size:
 *
 * Returns: the size of the camera log file.
 */
int64_t cam_log_get_file_size (const CamLog *self);

#ifdef __cplusplus
}
#endif

#endif
