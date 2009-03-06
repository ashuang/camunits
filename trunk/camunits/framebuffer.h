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
 * image.  Images produced by #CamUnit objects are contained within a
 * CamFrameBuffer object.
 *
 * In addition to image data, each CamFrameBuffer object may contain a metadata
 * dictionary that can be used to attach additional information to a frame
 * buffer.  Keys are always UTF-8 strings and values are suggested to be UTF-8
 * strings, but the exact meaning of the value is left up to the user.  Typical
 * usages may include identifying the image source (e.g. the UID of a firewire
 * camera), the exposure settings for the image, etc.
 *
 * When logging images with <literal>output.logger</literal>, the
 * metadata dictionary is also logged to disk.  Additionally, when replaying
 * logs with <literal>input.log</literal>, the metadata dictionary will be
 * repopulated from the log file.  Thus, the metadata dictionary can be used to
 * store metadata that persists in a log file.
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
};

struct _CamFrameBufferClass {
    GObjectClass parent;
};

GType cam_framebuffer_get_type (void);

/**
 * cam_framebuffer_new:
 * @data: the data buffer to use.  The returned #CamFrameBuffer does not take
 *        ownership of the data buffer, and memory management of @data is left
 *        to the user.  
 * @length: the size, in bytes, of @data.
 *
 * Returns: a newly allocated #CamFrameBuffer that does not assume ownership
 *          of @data.
 */
CamFrameBuffer * cam_framebuffer_new (uint8_t * data, int length);

/**
 * cam_framebuffer_new_alloc:
 * @length: the size, in bytes, of the data buffer to allocate.
 *
 * Returns: a newly allocated #CamFrameBuffer that has a data buffer of
 *          capacity @length bytes.  When the #CamFrameBuffer is destroyed, the
 *          data buffer is also destroyed.
 */
CamFrameBuffer * cam_framebuffer_new_alloc (int length);

/**
 * cam_framebuffer_copy_metadata:
 * @self: the CamFrameBuffer
 * @from: the source #CamFrameBuffer
 *
 * Convenience method to copy the metadata dictionary from the @from buffer to
 * @self.  Also copies the %timestamp field.
 */
void cam_framebuffer_copy_metadata (CamFrameBuffer *self, 
        const CamFrameBuffer *from);

/**
 * cam_framebuffer_metadata_get:
 * @key: the string key of the metadata.  key must be UTF-8
 * @len: output parameter.  If not NULL, then on return this stores the length
 *       of the dictionary value, in bytes.
 * 
 * Retrieves an entry from the metadata dictionary of the framebuffer.
 *
 * Returns: A pointer to the dictionary value.  The pointer is owned by
 * the dictionary and should _not_ be freed by the application.  The pointer
 * will only be valid as long as the value is in the dictionary and the
 * CamFramebuffer is not destroyed.  Returns NULL if the key is not found.
 */
uint8_t * cam_framebuffer_metadata_get (const CamFrameBuffer * self,
        const char * key, int * len);

/**
 * cam_framebuffer_metadata_set:
 * @self: the CamFrameBuffer
 * @key: The dictionary key, must be UTF-8.  A copy of this string is made
 *      internally.  Cannot be NULL.
 * @value: The dictionary value.  A copy of this data is made internally.
 *      Cannot be NULL.
 * @len: The length of @value in bytes.
 *
 * Sets an entry in the metadata dictionary of the framebuffer.  Existing
 * entries are overwritten.
 */
void cam_framebuffer_metadata_set (CamFrameBuffer *self, const char *key,
        const uint8_t *value, int len);

/**
 * cam_framebuffer_metadata_list_keys:
 * @self: the CamFrameBuffer
 *
 * Returns: a #GList of keys, each a string, that reference entries in the
 * metadata dictionary.  The list should be freed with g_list_free().  Note
 * that the pointers to the keys themselves reference the dictionary and
 * are only valid as long as the dictionary is not modified.
 *
 * Do not modify the strings in the returned list.
 */
GList * cam_framebuffer_metadata_list_keys (const CamFrameBuffer * self);

#ifdef __cplusplus
}
#endif

#endif
