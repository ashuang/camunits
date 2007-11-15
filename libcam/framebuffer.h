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
 *
 * In addition to image data, each CamFrameBuffer object may contain a metadata
 * dictionary that can be used to attach additional information to a frame
 * buffer.  Keys and values of the metadata dictionary are both UTF8 strings,
 * and their exact meaning is left up to the user.  Typical usages may include
 * identifying the image source (e.g. the UID of a firewire camera), the
 * exposure settings for the image, etc.
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
 */
struct _CamFrameBuffer {
    GObject parent;

    /*< public >*/
    uint8_t * data;
    unsigned int length;
    unsigned int bytesused;
    int64_t timestamp;

    /*< private >*/
    int owns_data;
    GHashTable *metadata;

    uint64_t source_uid; // deleteme
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
 * Convenience method to copy the metadata dictionary from the @from buffer to
 * @self.  Also copies the %timestamp and %source_uid fields
 */
void cam_framebuffer_copy_metadata (CamFrameBuffer *self, 
        const CamFrameBuffer *from);

/**
 * cam_framebuffer_metadata_get:
 * @key: a UTF8 string
 * @val: output parameter.  Should point to an unused char *.  On successful
 *       return, points to a newly allocated UTF8 string, which must be freed
 *       with free.  Result is undefined if %key is not in the metadata
 *       dictionary.
 * @int: output parameter.  If not NULL, then on return this stores the length
 *       of val, in bytes.
 * 
 * Retrieves an entry from the metadata dictionary of the framebuffer.
 *
 * Returns: TRUE if the key was found, FALSE if not
 */
gboolean cam_framebuffer_metadata_get (const CamFrameBuffer *self,
        const char *key, char **val, int *len);

/**
 * cam_framebuffer_metadata_set:
 * @key: a UTF8 string.  A copy of this string is made internally.  Cannot be
 *       NULL.
 * @value: a UTF8 string.  A copy of this string is made internally.  Cannot be
 *         NULL.
 *
 * Sets an entry in the metadata dictionary of the framebuffer.  Existing
 * entries are overwritten.
 */
void cam_framebuffer_metadata_set (CamFrameBuffer *self, const char *key,
        const char *value);

#ifdef __cplusplus
}
#endif

#endif
