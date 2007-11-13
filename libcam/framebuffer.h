#ifndef __cam_framebuffer_h__
#define __cam_framebuffer_h__

#include <sys/time.h>
#include <stdint.h>

#include <glib-object.h>

/**
 * SECTION:framebuffer
 * @short_description: Container class for a raw data buffer.
 *
 * A CamFrameBuffer object contains a data buffer that typically stores a 
 * image.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CamFrameBuffer CamFrameBuffer;
typedef struct _CamFrameBufferClass CamFrameBufferClass;

#define CAM_TYPE_FRAMEBUFFER  cam_framebuffer_get_type()
#define CAM_FRAMEBUFFER(obj)  (G_TYPE_CHECK_INSTANCE_CAST( (obj), \
        CAM_TYPE_FRAMEBUFFER, CamFrameBuffer))
#define CAM_FRAMEBUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
            CAM_TYPE_FRAMEBUFFER, CamFrameBufferClass ))
#define CAM_IS_FRAMEBUFFER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
            CAM_TYPE_FRAMEBUFFER ))
#define CAM_IS_FRAMEBUFFER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE( \
            (klass), CAM_TYPE_FRAMEBUFFER))
#define CAM_FRAMEBUFFER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
            CAM_TYPE_FRAMEBUFFER, CamFrameBufferClass))

/**
 * CamFrameBuffer:
 * @data:      the data buffer.
 * @length:    the capacity of allocated the data buffer in bytes.
 * @bytesused: how many bytes of the data buffer are actually used to store
 *             meaningful data.
 * @timestamp: microseconds since the epoch at which the frame was generated.
 * @bus_timestamp:  If the frame's original source has some native timestamp
 *                  format (firewire, USB, frame grabber, etc.) put that raw
 *                  data here in case the "time of day"-style timestamps need
 *                  to be corrected later.
 * @source_uid:  Unique identifier for the frame's capture source.  For
 *               example, this might identify the camera from which the frame
 *               came.
 */
struct _CamFrameBuffer {
    GObject parent;

    /*< public >*/
    uint8_t * data;
    unsigned int length;
    unsigned int bytesused;
    int64_t timestamp;
    uint64_t source_uid;

    /*< private >*/
    int owns_data;
};

struct _CamFrameBufferClass {
    GObjectClass parent;
};

GType cam_framebuffer_get_type (void);

/**
 * cam_framebuffer_new:
 * @data: the data buffer to use.  The returned #CamFrameBuffer does not take
 * ownership of the data buffer, and memory management of @data is left to the
 * user.  
 * @length: the size, in bytes, of @data.
 *
 * Returns: a newly allocated #CamFrameBuffer that does not assume ownership
 * of @data.
 */
CamFrameBuffer * cam_framebuffer_new (uint8_t * data, int length);

/**
 * cam_framebuffer_new_alloc:
 * @length: the size, in bytes, of the data buffer to allocate.
 *
 * Returns: a newly allocated #CamFrameBuffer that has a data buffer of
 * capacity @length bytes.  When the #CamFrameBuffer is destroyed, the data
 * buffer is also destroyed.
 */
CamFrameBuffer * cam_framebuffer_new_alloc (int length);

/**
 * cam_framebuffer_copy_metadata:
 *
 * Convenience method to copy the @bytesused, @timestamp, and @source_uid)
 * fields from the @from buffer to @self.
 */
void cam_framebuffer_copy_metadata (CamFrameBuffer *self, 
        const CamFrameBuffer *from);

#ifdef __cplusplus
}
#endif

#endif
