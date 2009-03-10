#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <GL/gl.h>
#define GLX_GLXEXT_PROTOTYPES 1
#include <GL/glx.h>
#include <GL/glext.h>

#define USE_VBLANK 1

#ifdef USE_VBLANK
#include <pthread.h>
#endif

#include "gl_drawing_area.h"

#define CAM_GL_DRAWING_AREA_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CAM_TYPE_GL_DRAWING_AREA, CamGLDrawingAreaPrivate))
typedef struct _CamGLDrawingAreaPrivate CamGLDrawingAreaPrivate;

struct _CamGLDrawingAreaPrivate {
    Display * dpy;
    XVisualInfo * visual;
    GLXContext context;

    gboolean vblank_sync;

#ifdef USE_VBLANK
    guint vblank_watch;
    int pipe[2];
    pthread_t thread;
    int quit_thread;
    int swap_requested;
#endif
};

static void cam_gl_drawing_area_realize (GtkWidget * widget);
static void cam_gl_drawing_area_unrealize (GtkWidget * widget);
static void cam_gl_drawing_area_size_allocate (GtkWidget * widget,
        GtkAllocation * allocation);

G_DEFINE_TYPE (CamGLDrawingArea, cam_gl_drawing_area, GTK_TYPE_DRAWING_AREA);

static void
cam_gl_drawing_area_class_init (CamGLDrawingAreaClass * klass)
{
    GtkWidgetClass * widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

    //widget_class->expose_event = cam_gl_drawing_area_expose;
    widget_class->realize = cam_gl_drawing_area_realize;
    widget_class->unrealize = cam_gl_drawing_area_unrealize;
    widget_class->size_allocate = cam_gl_drawing_area_size_allocate;

    g_type_class_add_private (gobject_class, sizeof (CamGLDrawingAreaPrivate));
}

static int attr_list[] = {
    GLX_RGBA,
    GLX_DOUBLEBUFFER,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_DEPTH_SIZE, 16,
    GLX_STENCIL_SIZE, 8,
    None
};

static void
cam_gl_drawing_area_init (CamGLDrawingArea * self)
{
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

    gtk_widget_set_double_buffered (GTK_WIDGET (self), FALSE);
    priv->dpy = NULL;
    priv->visual = NULL;
    priv->context = NULL;
#ifdef USE_VBLANK
    priv->vblank_watch = 0;
    priv->pipe[0] = -1;
    priv->thread = 0;
    priv->quit_thread = 0;
    priv->swap_requested = 0;
#endif

    XVisualInfo * vinfo = glXChooseVisual (GDK_DISPLAY (),
            GDK_SCREEN_XNUMBER (gdk_screen_get_default ()),
            attr_list);
    if (!vinfo) {
        fprintf (stderr, "Preferred visual not found, using default...\n");
        return;
    }
    VisualID desired_id = vinfo->visualid;
    XFree (vinfo);

    GList * visuals = gdk_list_visuals ();
    GList * vis;
    for (vis = visuals; vis; vis = vis->next) {
        Visual * xv = GDK_VISUAL_XVISUAL (vis->data);
        if (XVisualIDFromVisual (xv) == desired_id) {
            GdkColormap * colormap = gdk_colormap_new (vis->data, FALSE);
            gtk_widget_set_colormap (GTK_WIDGET (self), colormap);
            g_object_unref (G_OBJECT (colormap));
            break;
        }
    }
    g_list_free (visuals);
}

GtkWidget *
cam_gl_drawing_area_new (gboolean vblank_sync)
{
    GObject * object = g_object_new (CAM_TYPE_GL_DRAWING_AREA, NULL);
    CamGLDrawingArea * self = CAM_GL_DRAWING_AREA (object);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);
    priv->vblank_sync = vblank_sync;
    return GTK_WIDGET (object);
}

void 
cam_gl_drawing_area_set_vblank_sync (CamGLDrawingArea *self, 
        gboolean vblank_sync)
{
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);
    priv->vblank_sync = vblank_sync;
}

static void
cam_gl_drawing_area_size_allocate (GtkWidget * widget,
        GtkAllocation * allocation)
{
    CamGLDrawingArea * self = CAM_GL_DRAWING_AREA (widget);

    /* chain up */
    GTK_WIDGET_CLASS (cam_gl_drawing_area_parent_class)->size_allocate (widget,
            allocation);

    /* Resize the OpenGL area to match the allocation size. */
    if (cam_gl_drawing_area_set_context (self) == 0)
        glViewport (0, 0, allocation->width, allocation->height);
}

#ifdef USE_VBLANK
static PFNGLXWAITVIDEOSYNCSGIPROC WaitVideoSyncSGI = NULL;
static PFNGLXGETVIDEOSYNCSGIPROC GetVideoSyncSGI = NULL;

/* This thread just waits for vblanks in a loop until told to stop
 * and signals the primary thread by writing to a pipe. */
static void *
swap_thread (void * arg)
{
    CamGLDrawingAreaPrivate * priv = (CamGLDrawingAreaPrivate *) arg;
    unsigned int count = 0;

    int minimal_attr_list[] = { GLX_RGBA, None };
    /* We need to create a separate connection to the display since
     * GLX is not thread-safe. */
    /* TODO: get the DISPLAY string from gtk */
    Display * display = XOpenDisplay (getenv ("DISPLAY"));
    XVisualInfo * visual = glXChooseVisual (display,
            DefaultScreen (display), minimal_attr_list);

    GLXContext ctx = glXCreateContext (display, visual, 0, GL_TRUE);
    if (!ctx) {
        fprintf (stderr, "GLX Context Error: Failed to get second GLX context\n");
        return NULL;
    }

    if (!glXMakeCurrent (display, DefaultRootWindow (display), ctx)) {
        fprintf (stderr, "GLX Context Error: Could not make second GLX context current\n");
        goto done;
    }

    int odd_even = 0;
    while (!priv->quit_thread) {
        int v;
        if ((v = WaitVideoSyncSGI (2, odd_even, &count)) != 0) {
            fprintf (stderr, "Error: glXWaitVideoSyncSGI failed %d\n", v);
            break;
        }
        if(1 != write (priv->pipe[1], "+", 1)) {
            perror("pipe write");
        }
        usleep (10);
        odd_even = !odd_even;
    }

done:
    glXDestroyContext (display, ctx);
    close (priv->pipe[1]);
    return NULL;
}

/* Called when the pipe has data on it, as written by the vblank-monitoring
 * thread. */
static gboolean
swap_func (GIOChannel * source, GIOCondition cond, gpointer data)
{
    GtkWidget * widget = GTK_WIDGET (data);
    CamGLDrawingArea * self = CAM_GL_DRAWING_AREA (data);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

    if (priv->swap_requested) {
        if (cam_gl_drawing_area_set_context (self) == 0)
            glXSwapBuffers (priv->dpy, GDK_WINDOW_XID (widget->window));
        priv->swap_requested = 0;
    }

    /* Clear out the pipe of vblank events */
    char buf[32];
    while (1) {
        int num = read (priv->pipe[0], buf, sizeof (buf));
        if (num <= 0)
            break;
    }

    return TRUE;
}
#endif

/* Returns 1 if the specified GLX extension is present, 0 if not. */
static int
is_glx_extension_present (Display * dpy, int screen, char * ext)
{
    const char * str = glXQueryExtensionsString (dpy, screen);
    if (!str)
        return 0;

    const char * a = str;
    int extlen = strlen (ext);
    while (*a) {
        int len = strcspn (a, " ");
        if (extlen == len && !strncmp (ext, a, len))
            return 1;
        a += len;
        if (*a)
            a++;
    }
    return 0;
}

static void
cam_gl_drawing_area_realize (GtkWidget * widget)
{
    CamGLDrawingArea * self = CAM_GL_DRAWING_AREA (widget);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

    /* chain up */
    GTK_WIDGET_CLASS (cam_gl_drawing_area_parent_class)->realize (widget);

    priv->dpy = GDK_WINDOW_XDISPLAY(widget->window);

#if 0
    priv->visual = glXChooseVisual (priv->dpy,
            GDK_SCREEN_XNUMBER (gdk_drawable_get_screen (GDK_DRAWABLE (widget->window))),
            //DefaultScreen (priv->dpy),
            attr_list);
#endif
    GdkDrawable * draw = GDK_DRAWABLE (widget->window);
    int screen = GDK_SCREEN_XNUMBER (gdk_drawable_get_screen (draw));
    XVisualInfo vinfo_template = {
        .visualid = XVisualIDFromVisual (gdk_x11_visual_get_xvisual (
                    gdk_drawable_get_visual (draw))),
        .screen = screen,
        .depth = gdk_drawable_get_depth (draw),
    };
    int nitems;
//    fprintf (stderr, "Using X Visual 0x%x\n",
//            (unsigned int) vinfo_template.visualid);
    priv->visual = XGetVisualInfo (priv->dpy,
            VisualIDMask | VisualScreenMask | VisualDepthMask,
            &vinfo_template, &nitems);
    if (priv->visual == NULL) {
        g_warning ("Failed to find GLX visual\n");
        return;
    }
    if (nitems != 1)
        fprintf (stderr, "Warning: more than one matching X visual found\n");

    priv->context = glXCreateContext (priv->dpy, priv->visual, 0,
            GL_TRUE);
    if (!priv->context) {
        g_warning ("Failed to get GLX context\n");
        XFree (priv->visual);
        priv->visual = NULL;
        return;
    }

    if (!glXMakeCurrent (priv->dpy, GDK_WINDOW_XID (widget->window),
                priv->context)) {
        g_warning ("Could not make GLX context current\n");
        return;
    }

    /* If the user doesn't want vblank sync, we are done */
    if (!priv->vblank_sync)
        return;

    /* Check for the presence of the video_sync extension */
    if (!is_glx_extension_present (priv->dpy, screen, "GLX_SGI_video_sync")) {
        priv->vblank_sync = 0;
        fprintf (stderr, "Video sync functions not found, disabling...\n");
        return;
    }

#ifdef USE_VBLANK
    /* Below we create a new thread to monitor the vblank.  We will
     * signal back to this thread by writing to a file descriptor
     * when each vblank occurs. */

    /* TODO: check extension list */

    GetVideoSyncSGI = (PFNGLXGETVIDEOSYNCSGIPROC) glXGetProcAddressARB (
            (unsigned char *)"glXGetVideoSyncSGI");
    WaitVideoSyncSGI = (PFNGLXWAITVIDEOSYNCSGIPROC) glXGetProcAddressARB (
            (unsigned char *)"glXWaitVideoSyncSGI");

    if (!GetVideoSyncSGI || !WaitVideoSyncSGI) {
        priv->vblank_sync = 0;
        fprintf (stderr, "Video sync functions not found, disabling...\n");
        return;
    }

    unsigned int count = 0;
    if (GetVideoSyncSGI (&count) != 0) {
        priv->vblank_sync = 0;
        fprintf (stderr, "Video sync counter failed, disabling...\n");
        return;
    }

    if(0 != pipe (priv->pipe)) {
        perror("error creating internal pipe\n");
        return;
    }
    fcntl (priv->pipe[0], F_SETFL, O_NONBLOCK);

    if (pthread_create (&priv->thread, NULL, swap_thread, priv) != 0) {
        priv->vblank_sync = 0;
        fprintf (stderr, "Video sync thread creation failed, disabling...\n");
        return;
    }

    GIOChannel * chan = g_io_channel_unix_new (priv->pipe[0]);
    priv->vblank_watch = g_io_add_watch (chan, G_IO_IN, swap_func, self);
    g_io_channel_unref (chan);
#endif
}

static void
cam_gl_drawing_area_unrealize (GtkWidget * widget)
{
    CamGLDrawingArea * self = CAM_GL_DRAWING_AREA (widget);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

#ifdef USE_VBLANK
    if (priv->thread) {
        /* Set a flag and wait for the thread to end */
        priv->quit_thread = 1;
        pthread_join (priv->thread, NULL);
        close (priv->pipe[0]);
    }
    if (priv->vblank_watch)
        g_source_remove (priv->vblank_watch);
#endif
    if (priv->context)
        glXDestroyContext (priv->dpy, priv->context);
    if (priv->visual)
        XFree (priv->visual);

    priv->dpy = NULL;
    priv->visual = NULL;
    priv->context = NULL;
#ifdef USE_VBLANK
    priv->vblank_watch = 0;
    priv->pipe[0] = -1;
    priv->thread = 0;
    priv->quit_thread = 0;
    priv->swap_requested = 0;
#endif

    /* chain up */
    GTK_WIDGET_CLASS (cam_gl_drawing_area_parent_class)->unrealize (widget);
}

void
cam_gl_drawing_area_swap_buffers (CamGLDrawingArea * self)
{
    GtkWidget * widget = GTK_WIDGET (self);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

#ifdef USE_VBLANK
    if (priv->vblank_watch) {
        /* If we are syncing to vblank, we just set a flag that a
         * swap is desired. */
        priv->swap_requested = 1;
        return;
    }
#endif
    /* If we can't monitor vblank, we swap immediately. */
    if (!GTK_WIDGET_REALIZED (widget) || !priv->dpy)
        return;

    glXSwapBuffers (priv->dpy, GDK_WINDOW_XID (widget->window));
}

int
cam_gl_drawing_area_set_context (CamGLDrawingArea * self)
{
    GtkWidget * widget = GTK_WIDGET (self);
    CamGLDrawingAreaPrivate * priv = CAM_GL_DRAWING_AREA_GET_PRIVATE (self);

    if (!GTK_WIDGET_REALIZED (widget) || !priv->context)
        return -1;

    if (!glXMakeCurrent (priv->dpy, GDK_WINDOW_XID (widget->window),
                priv->context)) {
        g_warning ("Could not make GLX context current\n");
        return -1;
    }
    glViewport(0, 0, widget->allocation.width, widget->allocation.height);
    return 0;
}

void
cam_gl_drawing_area_invalidate (CamGLDrawingArea * self)
{
    GtkWidget * widget = GTK_WIDGET (self);
    gtk_widget_queue_draw_area (widget, 0, 0,
            widget->allocation.width, widget->allocation.height);
}

