#ifndef __cam_log_h__
#define __cam_log_h__

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamLog CamLog;

typedef struct _cam_log_frame_info {
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    uint32_t pixelformat;

    int64_t timestamp;
    uint32_t bus_timestamp;

    uint64_t source_uid;

    uint32_t datalen;

    uint32_t frameno;

    uint64_t frame_offset;
    uint64_t prev_frame_offset;
} cam_log_frame_info_t;

/**
 * cam_log_new:
 * @fname: the file to read or create
 * @mode:  either "r" or "w"
 *
 * constructor
 */
CamLog* cam_log_new (const char *fname, const char *mode);

void cam_log_destroy (CamLog *self);

/**
 * cam_log_peek_next_frame_info:
 *
 * Retrieves the metadata associated with the next frame, without actually
 * advancing the current position within the log.
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on failure.  In either case, the file pointer of
 * the log file is not changed.
 */
int cam_log_peek_next_frame_info (CamLog *self, 
        cam_log_frame_info_t *frame_info);

/**
 * cam_log_read_next_frame:
 *
 * Retrieves the next frame and associated metadata.  Advances the file pointer
 * of the logfile.
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on file error, EOF, or if the buffer %buf is not
 * large enough to hold the entire frame.
 */
int cam_log_read_next_frame (CamLog *self, cam_log_frame_info_t *frame_info,
        uint8_t *buf, int buf_size);

/**
 * cam_log_skip_next_frame:
 *
 * Advances the file pointer past the next frame.
 *
 * Read-mode only.
 *
 * Returns: 0 on success, -1 on file error or EOF.
 */
int cam_log_skip_next_frame (CamLog *self);

/**
 * cam_log_write_frame:
 * @width:          width of the image
 * @height:         size of the image
 * @stride:         distance, in bytes, between rows of the image
 * @pixelformat:    see pixels.h
 * @timestamp:      time, in microseconds since the epoch, of the frame
 * @source_uid:     identifier for the source of the image
 * @data:           pointer to the actual image data
 * @datalen:        size, in bytes, of the image data
 * @file_offset:    output parameter.  The file offset of the start of the
 *                  frame is stored here.
 *
 * Writes a frame to disk.
 *
 * Returns: 0 on success, -1 on failure
 */
int cam_log_write_frame (CamLog *self, int width, int height, int stride,
        int pixelformat, int64_t timestamp, 
        uint64_t source_uid,
        const uint8_t *data, int datalen, int64_t * file_offset);

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
