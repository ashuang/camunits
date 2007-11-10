#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#define MALLOC_ALIGNED(s) malloc(s)
#else
#include <malloc.h>
#define MALLOC_ALIGNED(s) memalign(16,s)
#endif

#include "framebuffer.h"
//#include "dbg.h"

static void cam_framebuffer_finalize( GObject *obj );

G_DEFINE_TYPE (CamFrameBuffer, cam_framebuffer, G_TYPE_OBJECT);

static void
cam_framebuffer_init( CamFrameBuffer *self )
{
    self->data = NULL;
    self->length = 0;
    self->bytesused = 0;
//    self->sequence = 0;
    self->timestamp = 0;
    self->source_uid = 0;
    self->owns_data = 0;
}

static void
cam_framebuffer_class_init( CamFrameBufferClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = cam_framebuffer_finalize;
}

static void
cam_framebuffer_finalize( GObject *obj )
{
    CamFrameBuffer *self = CAM_FRAMEBUFFER( obj );

    if( self->data && self->owns_data ) {
//        dbg(DBG_UNIT, "destroying framebuffer data %p\n", self->data);
        free( self->data );
    }
    self->data = NULL;
    self->length = 0;

    G_OBJECT_CLASS (cam_framebuffer_parent_class)->finalize(obj);
}

CamFrameBuffer *
cam_framebuffer_new (uint8_t * data, int length)
{
    CamFrameBuffer *self = 
        CAM_FRAMEBUFFER( g_object_new( CAM_TYPE_FRAMEBUFFER, NULL ) );

    self->data = data;
    self->length = length;
    self->owns_data = 0;
    return self;
}

CamFrameBuffer *
cam_framebuffer_new_alloc (int length)
{
    CamFrameBuffer *self = 
        CAM_FRAMEBUFFER( g_object_new( CAM_TYPE_FRAMEBUFFER, NULL ) );
    self->data = (uint8_t*) MALLOC_ALIGNED (length);
    self->length = length;
    self->owns_data = 1;
    return self;
}

void
cam_framebuffer_copy_metadata (CamFrameBuffer * self, 
        const CamFrameBuffer *from)
{
    self->timestamp = from->timestamp;
    self->bytesused = from->bytesused;
    self->source_uid = from->source_uid;
}
