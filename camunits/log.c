#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <inttypes.h>

#include "log.h"
#include "pixels.h"
#include "dbg.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif


typedef enum {
    CAMLOG_VERSION_INVALID,
    CAMLOG_VERSION_LEGACY,
    CAMLOG_VERSION_0
} cam_log_version_t;

typedef enum {
    CAMLOG_MODE_READ,
    CAMLOG_MODE_WRITE
} cam_log_mode_t;

struct _CamLog {
    FILE *fp;
    cam_log_mode_t mode;
    off_t file_size;

    CamLogFrameInfo first_frame_info;
    CamLogFrameInfo last_frame_info;

    CamLogFrameFormat curr_format;
    CamLogFrameInfo   curr_info;
    CamFrameBuffer * curr_frame;
    int64_t curr_data_offset;

    int64_t next_offset;
    uint64_t prev_offset;
};


// ========================= legacy log functions ========================
#define LOG_MARKER  0xEDED
#define LOG_HEADER_SIZE 8

typedef enum {
    LOG_TYPE_FRAME_DATA = 1,
    LOG_TYPE_FRAME_FORMAT = 2,
    LOG_TYPE_FRAME_TIMESTAMP = 3,   // legacy, from v1
    LOG_TYPE_COMMENT = 4,           // legacy, from v1
    LOG_TYPE_SOURCE_UID = 6,        // legacy, from v1
    LOG_TYPE_FRAME_INFO_0 = 7,      // legacy, from v2
    LOG_TYPE_FRAME_INFO_1 = 8,
    LOG_TYPE_METADATA = 9,
    LOG_TYPE_MAX
} LogType;

// LOG_TYPE_FRAME_INFO_0:
//    uint16_t width;
//    uint16_t height;
//    uint16_t stride;
//    uint32_t pixelformat;
//    uint64_t timestamp;
//    uint32_t bus_timestamp;
//    uint64_t source_uid;
//    uint32_t frameno;
//    uint64_t prev_frame_offset;
#define LOG_FRAME_INFO_0_SIZE 42

// LOG_TYPE_FRAME_FORMAT:
//    uint16_t width;
//    uint16_t height;
//    uint16_t stride;
//    uint32_t pixelformat;

// LOG_TYPE_FRAME_INFO_1:
//    uint64_t timestamp;
//    uint64_t frameno;
//    uint64_t prev_frame_offset;

// LOG_TYPE_METADATA:
//    uint16_t num_keys;
//    num_keys *
//       uint16_t key_len;
//       key_len * uint8_t key;
//       uint8_t reserved; (= 0)
//       uint32_t value_len;
//       data_len * uint8_t value;

static inline int
log_put_uint8 (uint8_t val, FILE * f)
{
    return fwrite (&val, 1, 1, f);
};

static inline int
log_put_uint16 (uint16_t val, FILE * f)
{
    uint16_t fval = htons (val);
    return fwrite (&fval, 2, 1, f);
};

static inline int
log_put_uint32 (uint32_t val, FILE * f)
{
    uint32_t fval = htonl (val);
    return fwrite (&fval, 4, 1, f);
}

static inline int
log_put_uint64 (uint64_t val, FILE * f)
{
    uint8_t b[8] = { val >> 56, val >> 48, val >> 40, val >> 32,
        val >> 24, val >> 16, val >> 8, val,
    };
    return fwrite (b, 1, 8, f);
}

static inline int
log_put_field (uint16_t type, uint32_t length, FILE * f)
{
    int status;
    status = log_put_uint16 (LOG_MARKER, f);
    if(status <= 0) 
        return status;
    status = log_put_uint16 (type, f);
    if(status <= 0)
        return status;
    status = log_put_uint32 (length, f);
    if(status <= 0)
        return status;
    return 8;
};

static inline int
log_get_uint16 (uint16_t * val, FILE * f)
{
    if (fread (val, 2, 1, f) != 1)
        return -1;
    *val = ntohs (*val);
    return 0;
};

static inline int
log_get_uint32 (uint32_t * val, FILE * f)
{
    if (fread (val, 4, 1, f) != 1)
        return -1;
    *val = ntohl (*val);
    return 0;
};

static inline int
log_get_uint64 (uint64_t * val, FILE * f)
{
    uint8_t b[8];
    if (fread (b, 1, 8, f) != 8)
        return -1;
    *val = ((uint64_t)b[0] << 56) |
        ((uint64_t)b[1] << 48) |
        ((uint64_t)b[2] << 40) |
        ((uint64_t)b[3] << 32) |
        ((uint64_t)b[4] << 24) |
        ((uint64_t)b[5] << 16) |
        ((uint64_t)b[6] << 8) |
        (uint64_t)b[7];
    return 0;
};

static inline int
log_get_next_field (uint16_t * type, uint32_t * length, FILE * f)
{
    uint16_t marker;
    if (log_get_uint16 (&marker, f) < 0)
        return -1;
    if (marker != LOG_MARKER) {
        fprintf (stderr, "Error: marker not found when reading log\n");
        return -1;
    }
    if (log_get_uint16 (type, f) < 0)
        return -1;
    if (log_get_uint32 (length, f) < 0)
        return -1;
    return 0;
};

static inline int
log_seek_to_field (FILE *f, uint16_t expected_type, uint32_t expected_length)
{
    uint16_t type;
    uint32_t length;
    while (0 == log_get_next_field (&type, &length, f)) {
        if (type == expected_type) {
            if (length == expected_length) return 0;
            else return -1;
        }
        fseeko (f, length, SEEK_CUR);
    }
    return -1;
}

/* Given that we are at any point in a camera log file, sync up to
 * the next field in the file by scanning for marker bytes and confirming
 * that valid data is present there. */
static int
log_resync (FILE * f)
{
    /* First, check if we are at a marker right now.  If so, assume
     * we are already synched. */
    uint16_t marker;
    if (log_get_uint16 (&marker, f) < 0)
        return -1;
    if (marker == LOG_MARKER) {
        fseeko (f, -2, SEEK_CUR);
        return 0;
    }

    int totalbytes = 0;
    uint8_t chunk[256];
    int offset = 0;
    while (1) {
        /* Read a chunk of bytes to scan for the marker */
        int len = fread (chunk + offset, 1, sizeof (chunk) - offset, f);
        if (len == 0)
            return -1;
        len += offset;

        int i;
        /* Look for the marker, and save the last 7 bytes for later
         * so a field won't cross a chunk boundary. */
        for (i = 0; i < len - 7; i++) {
            totalbytes++;
            /* Check for marker */
            if (chunk[i] != 0xED || chunk[i+1] != 0xED)
                continue;

            /* Make sure type is sane */
            uint16_t type = (chunk[i+2] << 8) | chunk[i+3];
            uint32_t length = (chunk[i+4] << 24) | (chunk[i+5] << 16) |
                (chunk[i+6] << 8) | chunk[i+7];
            if (type >= LOG_TYPE_MAX || type == 0)
                continue;
            
            /* Assume the length is correct, and seek to the next field. */
            off_t seekdist = (off_t)length - (off_t)(len-i-8);
            if (fseeko (f, seekdist, SEEK_CUR) < 0)
                return -1;

            /* Check for the presence of marker and type at next field */
            if (log_get_uint16 (&marker, f) < 0)
                return -1;
            if (log_get_uint16 (&type, f) < 0)
                return -1;
            if (marker == LOG_MARKER && type > 0 && type < 10) {
                /* Seek back to the start of the field */
                fseeko (f, -(off_t)length-12, SEEK_CUR);
                return 0;
            }
            /* Seek back to where the last chunk left off */
            fseeko (f, -seekdist-4, SEEK_CUR);
        }

        /* Copy any unscanned bytes at the end of the chunk to the
         * beginning of the next chunk. */
        memmove (chunk, chunk + i, len - i);
        offset = len - i;
    }
    return -1;
}
// =================================================

static int find_last_frame_info (CamLog *self);
static int process_frame (CamLog * self);

#define MAX64 ((uint64_t)-1)

CamLog* 
cam_log_new (const char *fname, const char *mode)
{
    if (! strcmp (mode, "r") && ! strcmp (mode, "w")) {
        g_warning ("mode must be either 'w' or 'r'");
        return NULL;
    }

    CamLog *self = (CamLog*) calloc(1, sizeof(CamLog));
    dbg (DBG_LOG, "Opening %s...\n", fname);

    struct stat statbuf;
    if (mode[0] == 'r') {
        if (stat (fname, &statbuf) < 0) {
            perror ("stat");
            dbg (DBG_LOG, "Couldn't open [%s]\n", fname);
            cam_log_destroy (self);
            return NULL;
        }
        self->mode = CAMLOG_MODE_READ;
    } else {
        self->mode = CAMLOG_MODE_WRITE;
    }

    self->fp = fopen(fname, mode);
    if (! self->fp) {
        perror ("fopen");
        dbg (DBG_LOG, "Couldn't open [%s]\n", fname);
        cam_log_destroy (self);
        return NULL;
    }

    self->prev_offset = 0;
    self->curr_info.frameno = 0;
    self->file_size = 0;
    self->first_frame_info.frameno = MAX64;

    if (self->mode == CAMLOG_MODE_READ) {
        self->file_size = statbuf.st_size;
        dbg (DBG_LOG, "File size %"PRId64" bytes\n", self->file_size);

        process_frame (self);
        memcpy (&self->first_frame_info, &self->curr_info,
                sizeof (CamLogFrameInfo));

        if (find_last_frame_info (self) < 0) {
            cam_log_destroy (self);
            return NULL;
        }
        rewind (self->fp);
        process_frame (self);
    }

    return self;
}

void 
cam_log_destroy (CamLog *self)
{
    if (self->fp) {
        fclose (self->fp);
    }
    memset (self,0,sizeof(CamLog));
    free (self);
}

int64_t cam_log_get_file_size (const CamLog *self)
{
    return self->file_size;
}

int
cam_log_next_frame (CamLog * self)
{
    return process_frame (self);
}

int
cam_log_get_frame_format (CamLog * self, CamLogFrameFormat * format)
{
    if (!self->curr_frame)
        return -1;
    memcpy (format, &self->curr_format, sizeof (CamLogFrameFormat));
    return 0;
}

int
cam_log_get_frame_info (CamLog * self, CamLogFrameInfo * info)
{
    if (!self->curr_frame)
        return -1;
    memcpy (info, &self->curr_info, sizeof (CamLogFrameInfo));
    return 0;
}

CamFrameBuffer *
cam_log_get_frame (CamLog * self)
{
    if (!self->curr_frame)
        return NULL;
    int64_t offset = ftello (self->fp);
    if (fseeko (self->fp, self->curr_info.data_offset, SEEK_SET) < 0)
        return NULL;
    CamFrameBuffer * framebuffer =
        cam_framebuffer_new_alloc (self->curr_info.data_len);
    cam_framebuffer_copy_metadata (framebuffer, self->curr_frame);
    int ret = fread (framebuffer->data, 1, self->curr_info.data_len, self->fp);
    if (ret != self->curr_info.data_len) {
        g_object_unref (framebuffer);
        return NULL;
    }
    framebuffer->bytesused = self->curr_info.data_len;
    fseeko (self->fp, offset, SEEK_SET);
    return framebuffer;
}

// =============

static int
process_frame (CamLog * self)
{
    int got_info = 0;
    int got_data = 0;
    if (self->curr_frame) {
        g_object_unref (self->curr_frame);
        self->curr_frame = NULL;
    }
    while (!(got_info && got_data)) {
        uint16_t type;
        uint32_t len;
        FILE * f = self->fp;
        uint64_t offset = ftello (f);
        if (log_get_next_field (&type, &len, f) < 0) {
            dbg (DBG_LOG, "Failed to parse next field at %"PRIu64"\n", offset);
            return -1;
        }

        if ((type == LOG_TYPE_FRAME_FORMAT ||
                type == LOG_TYPE_FRAME_INFO_0) && !self->curr_frame) {
            self->curr_frame = cam_framebuffer_new_alloc (0);
            self->curr_info.offset = offset;
            self->curr_info.frameno = MAX64;
        }
        else if (!self->curr_frame) {
            fseeko (f, len, SEEK_CUR);
            continue;
        }

        if (type == LOG_TYPE_FRAME_FORMAT) {
            CamLogFrameFormat * cf = &self->curr_format;
            if (len != 10) {
                dbg (DBG_LOG, "Format field had wrong length\n");
                return -1;
            }
            if (log_get_uint16 (&cf->width, f) != 0 ||
                    log_get_uint16 (&cf->height, f) != 0 ||
                    log_get_uint16 (&cf->stride, f) != 0 ||
                    log_get_uint32 (&cf->pixelformat, f) != 0)  {
                dbg (DBG_LOG, "Error parsing format\n");
                return -1;
            }
        }
        else if (type == LOG_TYPE_FRAME_INFO_0) {
            CamLogFrameFormat * cf = &self->curr_format;
            CamLogFrameInfo * ci = &self->curr_info;
            uint32_t bus_timestamp, frameno;
            uint64_t source_uid;
            if (len != 42)
                return -1;
            if (log_get_uint16 (&cf->width, f) != 0 ||
                    log_get_uint16 (&cf->height, f) != 0 ||
                    log_get_uint16 (&cf->stride, f) != 0 ||
                    log_get_uint32 (&cf->pixelformat, f) != 0 ||
                    log_get_uint64 ((uint64_t*)&ci->timestamp, f) != 0 ||
                    log_get_uint32 (&bus_timestamp, f) != 0 ||
                    log_get_uint64 (&source_uid, f) != 0 ||
                    log_get_uint32 (&frameno, f) != 0 ||
                    log_get_uint64 (&self->prev_offset, f) != 0)
                return -1;
            ci->frameno = frameno;
            self->curr_frame->timestamp = ci->timestamp;
            char str[20];
            sprintf (str, "0x%016"PRIx64, source_uid);
            cam_framebuffer_metadata_set (self->curr_frame, "Source GUID",
                    (uint8_t *) str, strlen (str));
            sprintf (str, "%u", bus_timestamp);
            cam_framebuffer_metadata_set (self->curr_frame, "Bus Timestamp",
                    (uint8_t *) str, strlen (str));
            got_info = 1;
        }
        else if (type == LOG_TYPE_FRAME_DATA) {
            self->curr_info.data_len = len;
            self->curr_info.data_offset = ftello (f);
            if (fseeko (f, len, SEEK_CUR) < 0)
                return -1;
            got_data = 1;
        }
        else if (type == LOG_TYPE_FRAME_TIMESTAMP) {
            uint32_t sec, usec, bus_timestamp;
            if (len != 12)
                return -1;
            if (log_get_uint32 (&sec, f) != 0 ||
                    log_get_uint32 (&usec, f) != 0 ||
                    log_get_uint32 (&bus_timestamp, f) != 0) 
                return -1;
            self->curr_info.timestamp = (uint64_t) sec * 1000000 + usec;
            self->curr_frame->timestamp = self->curr_info.timestamp;
            char str[20];
            sprintf (str, "%u", bus_timestamp);
            cam_framebuffer_metadata_set (self->curr_frame, "Bus Timestamp",
                    (uint8_t *) str, strlen (str));
            got_info = 1;
        }
        else if (type == LOG_TYPE_SOURCE_UID) {
            uint64_t source_uid;
            if (len != 8)
                return -1;
            if (log_get_uint64 (&source_uid, f) != 0)
                return -1;
            char str[20];
            sprintf (str, "0x%016"PRIx64, source_uid);
            cam_framebuffer_metadata_set (self->curr_frame, "Source GUID",
                    (uint8_t *) str, strlen (str));
        }
        else if (type == LOG_TYPE_FRAME_INFO_1) {
            CamLogFrameInfo * ci = &self->curr_info;
            if (len != 24) {
                dbg (DBG_LOG, "Info 1 field had wrong length\n");
                return -1;
            }
            if (log_get_uint64 (&ci->timestamp, f) != 0 ||
                    log_get_uint64 (&ci->frameno, f) != 0 ||
                    log_get_uint64 (&self->prev_offset, f) != 0) {
                dbg (DBG_LOG, "Error parsing Info 1 field\n");
                return -1;
            }
            self->curr_frame->timestamp = ci->timestamp;
            self->prev_offset = offset - self->prev_offset;
            got_info = 1;
        }
        else if (type == LOG_TYPE_METADATA) {
            int b = 2, i;
            uint16_t num;
            if (log_get_uint16 (&num, f) != 0)
                return -1;
            for (i = 0; i < num && b < len; i++) {
                uint16_t key_len;
                uint32_t value_len;
                if (log_get_uint16 (&key_len, f) != 0)
                    return -1;
                char key[key_len + 1];
                if(fread (key, 1, key_len, f) != key_len)
                    return -1;
                key[key_len] = '\0';
                fseeko (f, 1, SEEK_CUR);
                if (log_get_uint32 (&value_len, f) != 0)
                    return -1;
                uint8_t value[value_len];
                if(fread (value, 1, value_len, f) != value_len)
                    return -1;
                b += 2 + key_len + 1 + 4 + value_len;
                cam_framebuffer_metadata_set (self->curr_frame, key,
                        value, value_len);
            }
            fseeko (f, len - b, SEEK_CUR);
        }
    }
    if (self->curr_info.frameno == MAX64) {
        if (self->first_frame_info.frameno == MAX64)
            self->curr_info.frameno = 0;
        else
            self->curr_info.frameno =
                (self->curr_info.offset - self->first_frame_info.offset) /
                (ftello (self->fp) - self->curr_info.offset);
    }
    return 0;
}

static inline double
timestamp_to_offset_s (CamLog *self, int64_t timestamp)
{
    return (timestamp - self->first_frame_info.timestamp) * 1e-6;
}

int 
cam_log_seek_to_offset (CamLog *self, int64_t offset)
{
    if (self->mode != CAMLOG_MODE_READ)
        return -1;

    off_t fpos = ftello (self->fp);
    if (fseeko (self->fp, offset, SEEK_SET) < 0) {
        dbg (DBG_LOG, "Seek to offset %"PRId64" failed\n", offset);
        goto fail;
    }

    if (log_resync (self->fp) < 0) {
        dbg (DBG_LOG, "Failed to resync after seek to %"PRId64"\n", offset);
        goto fail;
    }

    if (process_frame (self) < 0) {
        dbg (DBG_LOG, "Failed to process frame after seek to %"PRId64"\n",
                offset);
        goto fail;
    }

    return 0;

fail:
    fseeko (self->fp, fpos, SEEK_SET);
    return -1;
}

int
cam_log_write_frame (CamLog * self, CamLogFrameFormat * format,
        CamFrameBuffer * frame, int64_t * offset)
{
    if (self->mode != CAMLOG_MODE_WRITE)
        return -1;

    int64_t frame_start_offset = ftello (self->fp);
    if (offset)
        *offset = frame_start_offset;

    // write frame info
    log_put_field (LOG_TYPE_FRAME_FORMAT, 10, self->fp);
    log_put_uint16 (format->width, self->fp);
    log_put_uint16 (format->height, self->fp);
    log_put_uint16 (format->stride, self->fp);
    log_put_uint32 (format->pixelformat, self->fp);

    uint64_t info_offset = ftello (self->fp);
    log_put_field (LOG_TYPE_FRAME_INFO_1, 24, self->fp);
    log_put_uint64 ((uint64_t) frame->timestamp, self->fp);
    log_put_uint64 (self->curr_info.frameno, self->fp);
    if (self->curr_info.frameno == 0)
        log_put_uint64 (0, self->fp);
    else
        log_put_uint64 (info_offset - self->prev_offset, self->fp);

    self->curr_info.frameno++;
    self->prev_offset = frame_start_offset;

    GList * list = cam_framebuffer_metadata_list_keys (frame);
    if (list) {
        int size = 2;
        for (GList * iter = list; iter; iter = iter->next) {
            size += 2 + strlen (iter->data) + 1 + 4;
            int value_len;
            cam_framebuffer_metadata_get (frame, iter->data, &value_len);
            size += value_len;
        }

        log_put_field (LOG_TYPE_METADATA, size, self->fp);
        log_put_uint16 (g_list_length (list), self->fp);
        for (GList * iter = list; iter; iter = iter->next) {
            uint16_t key_len = strlen (iter->data);
            log_put_uint16 (key_len, self->fp);
            if(fwrite (iter->data, 1, key_len, self->fp) != key_len)
                return -1;
            log_put_uint8 (0, self->fp);
            int value_len;
            uint8_t * value = cam_framebuffer_metadata_get (frame,
                    iter->data, &value_len);
            log_put_uint32 (value_len, self->fp);
            if(fwrite (value, 1, value_len, self->fp) != value_len)
                return -1;
        }
        g_list_free (list);
    }

    // write frame data
    log_put_field (LOG_TYPE_FRAME_DATA, frame->bytesused, self->fp);
    int status = fwrite (frame->data, 1, frame->bytesused, self->fp);
    self->file_size = ftello (self->fp);

    if (status != frame->bytesused)
        return -1;
    return 0;
}

int 
cam_log_count_frames (CamLog *self)
{
    if (self->mode != CAMLOG_MODE_READ)
        return -1;

    return self->last_frame_info.frameno - self->first_frame_info.frameno + 1;
}

static int
do_seek_to_int64_param (CamLog *self, CamLogFrameInfo *low_frame,
        CamLogFrameInfo *high_frame, int64_t desired_val, int val_offset)
{
#define GET_VAL(s) (*(int64_t *)((void *)(s) + val_offset))
    int64_t low_val = GET_VAL (low_frame);
    int64_t high_val = GET_VAL (high_frame);
    int64_t curr_val = GET_VAL (&self->curr_info);
    dbg (DBG_LOG, " --- %"PRId64", %"PRId64", %"PRId64" (%"PRId64") ---\n",
            low_val - desired_val, desired_val, high_val - desired_val, 
            curr_val - desired_val);

    assert (low_frame->offset >= 0 && 
            low_frame->offset <= high_frame->offset && 
            low_val <= high_val &&
            desired_val <= high_val && desired_val >= low_val &&
            high_frame->offset <= self->last_frame_info.offset);

    int64_t val_spanned = high_val - low_val;
    int64_t nbytes_spanned = high_frame->offset - low_frame->offset;
    double average_bytes_per_val = nbytes_spanned / (double)val_spanned;

    // if we're within 3MB, just manually iterate to avoid the
    // resyncing process.
    while (desired_val > curr_val &&
           (desired_val - curr_val) * average_bytes_per_val < 3000000) {
        dbg (DBG_LOG, "skip fwd\n");
        if (cam_log_next_frame (self) < 0)
            return -1;
        curr_val = GET_VAL (&self->curr_info);

        // if we've iterated beyond what we actually want, then just bottom out
        // and return.
        if (curr_val > desired_val)
            return 0;
    }
#if 0
    while (self->next_frame_info.frameno > desired_frameno &&
           self->next_frame_info.frameno - desired_frameno < 20) {
        dbgl ("skip back\n");
        if (0 != cam_log_seek_to_offset (self, 
                    self->next_frame_info.prev_frame_offset))
            return -1;
    }
#endif
    if (curr_val == desired_val)
        return 0;

    // make a best guess as to where the frame starts
    int64_t offset_guess = 
        (desired_val - low_val) * average_bytes_per_val + 0.5 +
        low_frame->offset;

    cam_log_seek_to_offset (self, offset_guess);

    curr_val = GET_VAL (&self->curr_info);
    if (curr_val == desired_val)
        return 0;

    /* Prevent getting stuck in an infinite loop of seeking backwards and
     * then having to seek forwards to the high frame again. */
    while (curr_val == high_val) {
        offset_guess -= high_frame->offset - offset_guess;
        if (offset_guess < low_frame->offset)
            offset_guess = low_frame->offset;
        cam_log_seek_to_offset (self, offset_guess);
        curr_val = GET_VAL (&self->curr_info);
    }
    if (curr_val == desired_val)
        return 0;

    CamLogFrameInfo info;
    memcpy (&info, &self->curr_info, sizeof (CamLogFrameInfo));
    if (curr_val > desired_val)
        return do_seek_to_int64_param (self, low_frame, &info,
                desired_val, val_offset);

    return do_seek_to_int64_param (self, &info, high_frame,
            desired_val, val_offset);
}

int 
cam_log_seek_to_frame (CamLog *self, int frameno)
{
    if (self->mode != CAMLOG_MODE_READ)
        return -1;

    frameno += self->first_frame_info.frameno;

    if (frameno < self->first_frame_info.frameno ||
            frameno > self->last_frame_info.frameno)
        return -1;

    if (!self->curr_frame)
        return do_seek_to_int64_param (self, &self->first_frame_info,
                &self->last_frame_info, frameno,
                offsetof (CamLogFrameInfo, frameno));

    if (frameno == self->curr_info.frameno)
        return 0;

    CamLogFrameInfo info;
    memcpy (&info, &self->curr_info, sizeof (CamLogFrameInfo));
    if (self->curr_info.frameno > frameno)
        return do_seek_to_int64_param (self, &self->first_frame_info,
                &info, frameno,
                offsetof (CamLogFrameInfo, frameno));

    return do_seek_to_int64_param (self, &info,
            &self->last_frame_info, frameno,
            offsetof (CamLogFrameInfo, frameno));
}

int 
cam_log_seek_to_timestamp (CamLog *self, int64_t timestamp)
{
    if (self->mode != CAMLOG_MODE_READ)
        return -1;

    if (timestamp < self->first_frame_info.timestamp || 
        timestamp > self->last_frame_info.timestamp)
        return -1;

    return do_seek_to_int64_param (self, &self->first_frame_info,
            &self->last_frame_info, timestamp,
            offsetof (CamLogFrameInfo, timestamp));
}

static int
find_last_frame_info (CamLog *self)
{
    int64_t search_inc = 5000000;

    for (int i=1; i < (self->file_size / search_inc) + 1 ; i++) {
        off_t offset = MAX (0, self->file_size - i * search_inc);

        if (0 == cam_log_seek_to_offset (self, offset)) {
            do {
                cam_log_get_frame_info (self, &self->last_frame_info);
            } while (cam_log_next_frame (self) == 0);
            break;
        }
        if (0 == offset)
            return -1;
    }
    dbg (DBG_LOG, "last frame offset: %"PRId64" timestamp: %"PRId64"\n",
            self->last_frame_info.offset, 
            self->last_frame_info.timestamp);
    dbg (DBG_LOG, "total frameno: %"PRId64"\n", self->last_frame_info.frameno);
    return 0;
}

