#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <glib-object.h>

#include "camunits-gmarshal.h"
#include "unit_chain.h"
#include "dbg.h"

#define err(args...) fprintf (stderr, args)

typedef struct _CamUnitChainSource CamUnitChainSource;
struct _CamUnitChainSource {
    GSource gsource;
    CamUnitChain *chain;
};

struct _CamUnitChain {
    GObject parent;

    CamUnitManager *manager;

    /*< private >*/
    GSourceFuncs source_funcs;
    CamUnitChainSource * event_source;

    GList *units;

    /*
     * link within units that points to the next unit in the chain ready to 
     * generate frames.
     */
    GList *pending_unit_link;

    gboolean streaming_desired;
};

struct _CamUnitChainClass {
    GObjectClass parent_class;

    /*< private >*/
};

enum {
    UNIT_ADDED_SIGNAL,
    UNIT_REMOVED_SIGNAL,
    UNIT_REORDERED_SIGNAL,
    FRAME_READY_SIGNAL,
    LAST_SIGNAL
};

static guint chain_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_chain_finalize (GObject *obj);
static gboolean update_unit_status (CamUnitChain *self, CamUnit *unit,
       gboolean streaming_desired);
static CamUnit * update_unit_statuses (CamUnitChain *self);

static gboolean cam_unit_chain_source_prepare (GSource *source, gint *timeout);
static gboolean cam_unit_chain_source_check (GSource *source);
static gboolean cam_unit_chain_source_dispatch (GSource *source, 
        GSourceFunc callback, void *user_data);
static void cam_unit_chain_source_finalize (GSource *source);
static void on_unit_status_changed (CamUnit *unit, CamUnitChain *self);

G_DEFINE_TYPE (CamUnitChain, cam_unit_chain, G_TYPE_OBJECT);

static void
cam_unit_chain_init (CamUnitChain *self)
{
    dbg (DBG_CHAIN, "constructor\n");
    self->manager = NULL;
    self->units = NULL;
    self->source_funcs.prepare  = cam_unit_chain_source_prepare;
    self->source_funcs.check    = cam_unit_chain_source_check;
    self->source_funcs.dispatch = cam_unit_chain_source_dispatch;
    self->source_funcs.finalize = cam_unit_chain_source_finalize;
    self->streaming_desired = FALSE;

    self->event_source = (CamUnitChainSource*) g_source_new (
            &self->source_funcs, sizeof (CamUnitChainSource));
    self->event_source->chain = self;
}

static void
cam_unit_chain_class_init (CamUnitChainClass *klass)
{
    dbg (DBG_CHAIN, "class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_chain_finalize;

    /**
     * CamUnitChain::unit-added
     * @chain: the CamUnitChain emitting the signal
     * @unit: the CamUnit being added.
     *
     * The unit-added signal is emitted when a new unit is added to the chain.
     */
    chain_signals[UNIT_ADDED_SIGNAL] = g_signal_new ("unit-added",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            CAM_TYPE_UNIT);
    /**
     * CamUnitChain::unit-removed
     * @chain: the CamUnitChain emitting the signal
     * @unit: the CamUnit being removed.
     *
     * The unit-removed signal is emitted when a unit is removed from the chain
     */
    chain_signals[UNIT_REMOVED_SIGNAL] = g_signal_new ("unit-removed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            CAM_TYPE_UNIT);
    /**
     * CamUnitChain::unit-reordered
     * @chain: the CamUnitChain emitting the signal
     * @unit: the CamUnit being reordered
     *
     * The unit-reordered signal is emitted when a unit is reordered within the
     * chain.
     */
    chain_signals[UNIT_REORDERED_SIGNAL] = 
        g_signal_new ("unit-reordered",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            CAM_TYPE_UNIT);
    /**
     * CamUnitChain::frame-ready
     * @chain: the CamUnitChain emitting the signal
     * @buf: the new frame
     *
     * The frame-ready signal is emitted when the last unit in the chain
     * produces a frame
     */
    chain_signals[FRAME_READY_SIGNAL] = 
        g_signal_new ("frame-ready",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2,
            CAM_TYPE_UNIT,
            CAM_TYPE_FRAMEBUFFER);
}

static void
cam_unit_chain_finalize (GObject *obj)
{
    dbg (DBG_CHAIN, "finalize\n");
    CamUnitChain *self = CAM_UNIT_CHAIN (obj);

    if (self->event_source)
        g_source_destroy ((GSource *) self->event_source);

    // release units in the chain
    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (cam_unit_is_streaming (unit)) {
            g_warning ("%s:%d -- Unit [%s] is still streaming!\n",
                    __FILE__, __LINE__, cam_unit_get_id (unit));
        }
        dbgl (DBG_REF, "unref unit\n");
        g_object_unref (unit);
    }
    g_list_free (self->units);

    // unref the CamUnitManager
    if (self->manager) {
        dbgl (DBG_REF, "unref manager\n");
        g_object_unref (self->manager);
    }

    G_OBJECT_CLASS (cam_unit_chain_parent_class)->finalize (obj);
}

CamUnitChain *
cam_unit_chain_new (void)
{
    CamUnitChain *self = 
        CAM_UNIT_CHAIN (g_object_new (CAM_TYPE_UNIT_CHAIN, NULL));
    self->manager = cam_unit_manager_get_and_ref();
    return self;
}

int
cam_unit_chain_get_length (const CamUnitChain *self)
{
    return g_list_length (self->units);
}

int 
cam_unit_chain_has_unit (const CamUnitChain *self, const CamUnit *unit)
{
    return (NULL != g_list_find (self->units, unit));
}

static void
on_last_unit_frame_ready (CamUnit *unit, const CamFrameBuffer *buf, 
        const CamUnitFormat *infmt, void *user_data)
{
    CamUnitChain *self = CAM_UNIT_CHAIN (user_data);
    g_signal_emit (self, chain_signals[FRAME_READY_SIGNAL], 0, unit, buf);
}

int 
cam_unit_chain_insert_unit (CamUnitChain *self, CamUnit *unit, 
        int position)
{
    if (position < 0 || position > cam_unit_chain_get_length (self)) {
        dbg (DBG_CHAIN, "invalid position %d for insert_unit\n", position);
        return -1;
    }

    self->units = g_list_insert (self->units, unit, position);
    dbgl (DBG_REF, "ref_sink unit [%s]\n", cam_unit_get_id (unit));
    g_object_ref_sink (unit);

    GList *link = g_list_nth (self->units, position);
    assert (link->data == unit);

    // subscribe to be notified when the status of the unit changes.
    g_signal_connect (G_OBJECT (unit), "status-changed",
            G_CALLBACK (on_unit_status_changed), self);

    // if the new unit has an input unit, then set it.
    if (link->prev) {
        CamUnit *prev_unit = (CamUnit*) link->prev->data;
        update_unit_status (self, unit, FALSE);
        cam_unit_set_input (unit, prev_unit);
    }

    if (cam_unit_is_streaming (unit) == self->streaming_desired) {
        on_unit_status_changed (unit, self);
    } else {
        update_unit_status (self, unit, self->streaming_desired);
    }

    // if the new unit comes before the end of the chain, then set the next
    // unit's input
    if (link->next) {
        CamUnit *next_unit = (CamUnit*) link->next->data;
        update_unit_status (self, next_unit, FALSE);
        cam_unit_set_input (next_unit, unit);
        update_unit_status (self, next_unit, self->streaming_desired);
    } else {
        // if the new unit is the last unit in the chain, then subscribe to its
        // frame-ready event and unsubscribe to the previous last unit's event.
        if (link->prev) {
            CamUnit *prev_unit = (CamUnit*) link->prev->data;
            g_signal_handlers_disconnect_by_func (prev_unit, 
                    on_last_unit_frame_ready, self);
        }
        g_signal_connect (G_OBJECT (unit), "frame-ready",
                G_CALLBACK (on_last_unit_frame_ready), self);
    }

    g_signal_emit (G_OBJECT (self), chain_signals[UNIT_ADDED_SIGNAL], 0, unit);

    dbg (DBG_CHAIN, "inserted unit %s at position %d\n", 
            cam_unit_get_name (unit), position);
    return 0;
}

int 
cam_unit_chain_insert_unit_tail (CamUnitChain *self, CamUnit *unit)
{
    return cam_unit_chain_insert_unit (self, unit,
            cam_unit_chain_get_length (self));
}

CamUnit *
cam_unit_chain_add_unit_by_id (CamUnitChain *self, const char *unit_id)
{
    dbg (DBG_CHAIN, "searching for %s\n", unit_id);

    if (! self->manager) {
        err ("Chain:  cannot add_unit_by_id without a manager!\n");
        return NULL;
    }
    CamUnit *new_unit = 
        cam_unit_manager_create_unit_by_id (self->manager, unit_id);
    if (new_unit && 0 == cam_unit_chain_insert_unit_tail (self, new_unit)) {
        return new_unit;
    }

    dbg (DBG_CHAIN, "add unit by ID (%s) unsuccessful\n", unit_id);
    return NULL;
}

int 
cam_unit_chain_remove_unit (CamUnitChain *self, CamUnit *unit)
{
    dbg (DBG_CHAIN, "removing unit [%s]\n", cam_unit_get_id (unit));
    GList *link = g_list_find (self->units, unit);
    if (! link) return -1;

    CamUnit *prev = link->prev ? CAM_UNIT (link->prev->data) : NULL;
    CamUnit *next = link->next ? CAM_UNIT (link->next->data) : NULL;

    update_unit_status (self, unit, FALSE);
    cam_unit_set_input (unit, NULL);

    self->units = g_list_delete_link (self->units, link);
    g_signal_handlers_disconnect_by_func (unit, on_unit_status_changed, self);
    g_signal_emit (G_OBJECT (self), chain_signals[UNIT_REMOVED_SIGNAL],
            0, unit);
    dbgl (DBG_REF, "unref unit [%s]\n", cam_unit_get_id (unit));
    g_object_unref (unit);

    if (next) {
        update_unit_status (self, next, FALSE);
        cam_unit_set_input (next, prev);
        update_unit_status (self, next, self->streaming_desired);
    } else if (prev) {
        // if this unit was the last in the chain, and it has a predecessor,
        // then subscribe to its predecessor's frame-ready signal
        g_signal_connect (G_OBJECT (prev), "frame-ready",
                G_CALLBACK (on_last_unit_frame_ready), self);
    }
    return 0;
}

void 
cam_unit_chain_remove_all_units (CamUnitChain *self)
{
    GList *ucopy = g_list_copy (self->units);
    for (GList *uiter=g_list_last (ucopy); uiter; uiter=uiter->prev) {
        cam_unit_chain_remove_unit (self, CAM_UNIT (uiter->data));
    }
    g_list_free (ucopy);
}

CamUnit * 
cam_unit_chain_get_last_unit (const CamUnitChain *self)
{
    GList *last_link = g_list_last (self->units);
    if (last_link) return CAM_UNIT (last_link->data);
    return NULL;
}

CamUnit * 
cam_unit_chain_find_unit_by_id (const CamUnitChain *self, 
        const char *unit_id)
{
    dbg (DBG_CHAIN, "searching for unit [%s]\n", unit_id);
    for (GList *uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (! strcmp (cam_unit_get_id (unit), unit_id)) {
            return unit;
        }
    }
    return NULL;
}


GList * 
cam_unit_chain_get_units (const CamUnitChain *self)
{
    return g_list_copy (self->units);
}

int 
cam_unit_chain_get_unit_index (CamUnitChain *self, const CamUnit *unit)
{
    int index = 0;
    for (GList *uiter=self->units; uiter; uiter=uiter->next) {
        if (unit == CAM_UNIT (uiter->data)) {
            return index;
        }
        index++;
    }
    return -1;
}

int 
cam_unit_chain_reorder_unit (CamUnitChain *self, CamUnit *unit,
        int new_index)
{
    dbg (DBG_CHAIN, "Reordering unit [%s] to position %d\n", 
            cam_unit_get_id (unit), new_index);
    int old_index = cam_unit_chain_get_unit_index (self, unit);
    if (old_index == new_index) return 0;

    if (old_index < 0 ||
        new_index < 0 || 
        new_index >= g_list_length (self->units)) return -1;

    GList *oldlink = g_list_nth (self->units, old_index);
    if (oldlink->next) {
        CamUnit *oldnext = CAM_UNIT (oldlink->next->data);
        CamUnit *oldprev = oldlink->prev ? 
            CAM_UNIT (oldlink->prev->data) : 
            NULL;
        update_unit_status (self, oldnext, FALSE);
        cam_unit_set_input (oldnext, oldprev);
        update_unit_status (self, oldnext, self->streaming_desired);
    }

    self->units = g_list_remove (self->units, unit);

    self->units = g_list_insert (self->units, unit, new_index);
    GList *link = g_list_nth (self->units, new_index);

    update_unit_status (self, unit, FALSE);
    if (link->prev) {
        CamUnit *prev_unit = CAM_UNIT (link->prev->data);
        cam_unit_set_input (unit, prev_unit);
    } else {
        cam_unit_set_input (unit, NULL);
    }
    update_unit_status (self, unit, self->streaming_desired);
    if (link->next) {
        CamUnit *next_unit = CAM_UNIT (link->next->data);
        update_unit_status (self, next_unit, FALSE);
        cam_unit_set_input (next_unit, unit);
        update_unit_status (self, next_unit, self->streaming_desired);
    }

    g_signal_emit (G_OBJECT (self), chain_signals[UNIT_REORDERED_SIGNAL],
            0, unit);

    return 0;
}

CamUnit * 
cam_unit_chain_all_units_stream_init (CamUnitChain *self) 
{
    self->streaming_desired = TRUE;
    return update_unit_statuses (self);
} 

CamUnit * 
cam_unit_chain_all_units_stream_shutdown (CamUnitChain *self) 
{
    self->streaming_desired = FALSE;
    return update_unit_statuses (self);
} 

static gboolean
update_unit_status (CamUnitChain *self, CamUnit *unit, gboolean desired)
{
    if (cam_unit_is_streaming (unit) == desired) return TRUE;
    const char *unit_id = cam_unit_get_id (unit);
    if (desired) {
        dbg (DBG_CHAIN, "stream_init on [%s]\n", unit_id);
        cam_unit_stream_init (unit, NULL);
    } else {
        dbg (DBG_CHAIN, "stream_shutdown on [%s]\n", unit_id);
        cam_unit_stream_shutdown (unit);
    }
    return cam_unit_is_streaming (unit) == desired;
}

static CamUnit *
update_unit_statuses (CamUnitChain *self)
{
    if (! self->units) return NULL;
    CamUnit *first_offender = NULL;
    for (GList *uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (! update_unit_status (self, unit, self->streaming_desired) &&
            !first_offender)
            first_offender = unit;
    }
    return first_offender;
}

static gboolean
cam_unit_chain_source_prepare (GSource *source, gint *timeout)
{
    CamUnitChainSource * csource = (CamUnitChainSource *) source;
    CamUnitChain * self = csource->chain;

//    dbg (DBG_CHAIN, "source prepare (%d units)\n", g_list_length (self->units));

    *timeout = -1;

    self->pending_unit_link = NULL;

    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        uint32_t uflags = cam_unit_get_flags (unit);

        if (uflags & CAM_UNIT_EVENT_METHOD_TIMEOUT &&
            cam_unit_is_streaming (unit)) {

            GTimeVal t;
            g_source_get_current_time (source, &t);
            int64_t now = (int64_t)t.tv_sec * 1000000 + t.tv_usec;

            int64_t event_time = cam_unit_get_next_event_time (unit);

            if (event_time == 0 || event_time <= now) {
                dbg (DBG_CHAIN, "%s timer ready\n", cam_unit_get_id (unit));
                self->pending_unit_link = uiter;
                return TRUE;
            }

            int tdiff = (event_time - now) / 1000;

            if (*timeout == -1 || tdiff < *timeout)
                *timeout = tdiff;
        }

        if (uflags & CAM_UNIT_EVENT_METHOD_FD) {
            GPollFD *pfd = (GPollFD*) g_object_get_data (G_OBJECT (unit),
                    "ChainPollFD");
            if (pfd && pfd->fd >= 0 && pfd->revents) {
                self->pending_unit_link = uiter;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gboolean
cam_unit_chain_source_check (GSource *source)
{
//    dbg (DBG_CHAIN, "source check\n");
    CamUnitChainSource * csource = (CamUnitChainSource *) source;
    CamUnitChain * self = csource->chain;

    self->pending_unit_link = NULL;

    for (GList *uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        uint32_t uflags = cam_unit_get_flags (unit);

        if (! cam_unit_is_streaming (unit)) {
            continue;
        }

        if (uflags & CAM_UNIT_EVENT_METHOD_TIMEOUT) {
            GTimeVal t;
            g_source_get_current_time (source, &t);
            int64_t now = (int64_t)t.tv_sec * 1000000 + t.tv_usec;

            int64_t event_time = cam_unit_get_next_event_time (unit);

            if (event_time == 0 || event_time <= now) {
                dbg (DBG_CHAIN, "%s timer ready (%"PRId64" %"PRId64")\n",
                        cam_unit_get_id (unit), event_time, now);
                self->pending_unit_link = uiter;
                return TRUE;
            }
        }
        if (uflags & CAM_UNIT_EVENT_METHOD_FD) {
            GPollFD *pfd = (GPollFD*) g_object_get_data (G_OBJECT (unit),
                    "ChainPollFD");
            if (pfd && pfd->fd >= 0 && pfd->revents) {
                self->pending_unit_link = uiter;
                return TRUE;
            }
        }
    }
    return FALSE;
}

static gboolean
cam_unit_chain_source_dispatch (GSource *source, GSourceFunc callback, 
        void *user_data)
{
//    dbg (DBG_CHAIN, "source dispatch\n");
    CamUnitChainSource * csource = (CamUnitChainSource *) source;
    CamUnitChain * self = csource->chain;

    if (!self->pending_unit_link) {
        err ("Chain: WARNING source_dispatch called, but no pending_unit!\n");
        return FALSE;
    }

    CamUnit *unit = CAM_UNIT (self->pending_unit_link->data);
    if (cam_unit_is_streaming (unit))
        cam_unit_try_produce_frame (unit, 0);
    self->pending_unit_link = NULL;

    return TRUE;
}

static void
cam_unit_chain_source_finalize (GSource *source)
{
    dbg (DBG_CHAIN, "source finalize\n");
    // TODO
}

int
cam_unit_chain_attach_glib (CamUnitChain *self, int priority,
        GMainContext * context)
{
    g_source_attach ((GSource*) self->event_source, context);
    g_source_set_priority ((GSource*) self->event_source, priority);
    if (self->manager) {
        cam_unit_manager_attach_glib (self->manager, priority, context);
    }
    return 0;
}

void 
cam_unit_chain_detach_glib (CamUnitChain *self)
{
    if (self->manager) {
        cam_unit_manager_detach_glib (self->manager);
    }
    if (!self->event_source)
        return;
    g_source_destroy ((GSource *) self->event_source);

    self->event_source = (CamUnitChainSource*) g_source_new (
            &self->source_funcs, sizeof (CamUnitChainSource));
    self->event_source->chain = self;
}

static void
on_unit_status_changed (CamUnit *unit, CamUnitChain *self)
{
    gboolean is_streaming = cam_unit_is_streaming (unit) ;
    dbg (DBG_CHAIN, "[%s] %s streaming\n", 
            cam_unit_get_id (unit), is_streaming ? "started" : "stopped");

    if (is_streaming) {
        // if the unit provides a file descriptor, then attach it to the
        // chain event source
        if ((cam_unit_get_flags (unit) & CAM_UNIT_EVENT_METHOD_FD) &&
                self->event_source) {
            GPollFD *pfd = (GPollFD*) malloc (sizeof (GPollFD));

            pfd->fd = cam_unit_get_fileno (unit);
            pfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
            pfd->revents = 0;

            g_object_set_data (G_OBJECT (unit), "ChainPollFD", pfd);
            g_source_add_poll ( (GSource *)self->event_source, pfd);
        }

        // If we detect that a unit has re-initialized, then we must restart
        // all the units after that unit, because the output format of the unit
        // may have changed.
        int restart = 0;
        GList *uiter;
        for (uiter=self->units; uiter; uiter=uiter->next) {
            CamUnit *cunit = CAM_UNIT (uiter->data);
            if (restart) {
                dbg (DBG_CHAIN, "Restarting [%s]\n", cam_unit_get_id (cunit));
                g_signal_handlers_block_by_func (cunit, 
                        on_unit_status_changed, self);
                update_unit_status (self, cunit, FALSE);
                cam_unit_set_input (cunit, cam_unit_get_input (cunit));
                update_unit_status (self, cunit, self->streaming_desired);
                g_signal_handlers_unblock_by_func (cunit,
                        on_unit_status_changed, self);
            }
            if (cunit == unit) restart = 1;
        }
    }
    else {
        // remove a GPollFD if it was setup earlier
        GPollFD *pfd = g_object_get_data (G_OBJECT (unit), "ChainPollFD");
        if (pfd) {
            if (self->event_source)
                g_source_remove_poll ( (GSource*)self->event_source, pfd);
            g_object_set_data (G_OBJECT (unit), "ChainPollFD", NULL);
            free (pfd);
        }
    }
}

char *
cam_unit_chain_snapshot (const CamUnitChain *self)
{
    GString *result = 
        g_string_new ("<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n");

    g_string_append (result, "<chain>\n");

    // loop through all units and output their states
    for (GList *uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);

        // start describing unit
        g_string_append_printf (result, "    <unit id=\"%s\"", 
                cam_unit_get_id (unit));

        // output format?
        const CamUnitFormat *fmt = cam_unit_get_output_format (unit);
        if (fmt) {
            GEnumClass *pf_class = 
                G_ENUM_CLASS (g_type_class_ref (CAM_TYPE_PIXEL_FORMAT));

            GEnumValue *pf_ev = g_enum_get_value(pf_class, fmt->pixelformat);
            char *fmt_name_escaped = g_strescape(fmt->name, NULL);
            g_string_append_printf (result, 
                    " width=\"%d\" height=\"%d\" pixelformat=\"%s\" format_name=\"%s\"", 
                    fmt->width, fmt->height, 
                    (pf_ev)?pf_ev->value_name:"UNRECOGNIZED",
                    fmt_name_escaped);

            g_type_class_unref (pf_class);
        }
        g_string_append (result, ">\n");

        // get the state of each control
        GList *ctls = cam_unit_list_controls(unit);
        for (GList *citer=ctls; citer; citer=citer->next) {
            CamUnitControl *ctl = CAM_UNIT_CONTROL (citer->data);

            g_string_append_printf (result, "        <control id=\"%s\">", 
                    cam_unit_control_get_id(ctl));

            CamUnitControlType ctl_type = cam_unit_control_get_control_type(ctl);
            switch (ctl_type) {
                case CAM_UNIT_CONTROL_TYPE_INT:
                    g_string_append_printf (result, "%d", 
                            cam_unit_control_get_int (ctl));
                    break;
                case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
                    g_string_append_printf (result, "%d", 
                            cam_unit_control_get_boolean (ctl));
                    break;
                case CAM_UNIT_CONTROL_TYPE_ENUM:
                    g_string_append_printf (result, "%d", 
                            cam_unit_control_get_enum (ctl));
                    break;
                case CAM_UNIT_CONTROL_TYPE_STRING:
                    {
                        char *s = g_markup_printf_escaped ("%s", 
                                cam_unit_control_get_string (ctl));
                        g_string_append (result, s);
                        free (s);
                    }
                    break;
                case CAM_UNIT_CONTROL_TYPE_FLOAT:
                    g_string_append_printf (result, "%g", 
                            cam_unit_control_get_float (ctl));
                    break;
                default:
                    g_warning ("%s: Ignoring unrecognized control type %d\n",
                            __FUNCTION__, ctl_type);
                    break;
            }
            g_string_append (result, "</control>\n");
        }
        g_list_free (ctls);

        g_string_append (result, "    </unit>\n");
    }

    g_string_append (result, "</chain>\n");
    return g_string_free (result, FALSE);
}

typedef struct {
    CamUnitChain *chain;
    int in_chain;
    CamUnit *unit;
    CamUnitControl *ctl;
    GEnumClass *pfmt_class;
    GQuark error_domain;
} ChainParseContext;

static void
_start_element (GMarkupParseContext *ctx, const char *element_name,
        const char **attribute_names, const char **attribute_values,
        void *user_data, GError **error)
{
    ChainParseContext *cpc = user_data;

    if (!strcmp (element_name, "chain")) {
        if (! cpc->in_chain) {
            cpc->in_chain = 1;
        } else {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "Unexpected <chain> element");
        }
        return;
    }

    if (! cpc->in_chain) {
        *error = g_error_new (CAM_ERROR_DOMAIN, 0, "Missing <chain> element");
        return;
    }
    g_assert (cpc->in_chain);
    if (!strcmp (element_name, "unit")) {
        if (cpc->unit || cpc->ctl) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "Unexpected <unit> element");
            return;
        }

        const char *unit_id = NULL;
        int width = -1;
        int height = -1;
        CamPixelFormat pfmt = CAM_PIXEL_FORMAT_ANY;
        char *fmt_name = NULL;

        for (int i=0; attribute_names[i]; i++) {
            if (!strcmp (attribute_names[i], "id")) {
                unit_id = attribute_values[i];
            } else if (!strcmp (attribute_names[i], "width")) {
                char *e = NULL;
                width = strtol (attribute_values[i], &e, 10);
                if (e == attribute_values[i]) {
                    width = -1;
                    *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                            "Invalid unit width [%s]", attribute_values[i]);
                    free(fmt_name);
                    return;
                }
            } else if (!strcmp (attribute_names[i], "height")) {
                char *e = NULL;
                height = strtol (attribute_values[i], &e, 10);
                if (e == attribute_values[i]) {
                    width = -1;
                    *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                            "Invalid unit height [%s]", attribute_values[i]);
                    free(fmt_name);
                    return;
                }
            } else if (!strcmp (attribute_names[i], "pixelformat")) {
                GEnumValue *ev = g_enum_get_value_by_name (cpc->pfmt_class,
                        attribute_values[i]);
                if (!ev) {
                    *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                            "Unrecognized pixelformat \"%s\"", 
                            attribute_values[i]);
                    free(fmt_name);
                    return;
                }
                pfmt = ev->value;
            } else if (!strcmp (attribute_names[i], "format_name")) {
                fmt_name = g_strcompress(attribute_values[i]);
            } else {
                *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                        "Unrecognized attribute \"%s\"", attribute_names[i]);
                free(fmt_name);
                return;
            }
        }
        if (!unit_id) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "<unit> element missing required id attribute");
            free(fmt_name);
            return;
        }

        cpc->unit = cam_unit_manager_create_unit_by_id (cpc->chain->manager, 
                unit_id);
        if (!cpc->unit) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "Unable to instantiate [%s]", unit_id);
            free(fmt_name);
            return;
        }
        cam_unit_set_preferred_format (cpc->unit, pfmt, width, height, 
                fmt_name);

        cam_unit_chain_insert_unit_tail (cpc->chain, cpc->unit);

        free(fmt_name);
        return;
    }
    if (!strcmp (element_name, "control")) {
        if (cpc->ctl || !cpc->unit) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "Unexpected <control> element");
            return;
        }
        const char *ctl_id = NULL;

        for (int i=0; attribute_names[i]; i++) {
            if (!strcmp (attribute_names[i], "id")) {
                ctl_id = attribute_values[i];
            } else {
                *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                        "Unrecognized attribute \"%s\"", attribute_names[i]);
                return;
            }
        }

        if (!ctl_id) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "<control> element missing required id attribute");
            return;
        }

        cpc->ctl = cam_unit_find_control (cpc->unit, ctl_id);

        if (!cpc->ctl) {
            dbg (DBG_CHAIN, "WARNING [%s] does not have control [%s]\n",
                    cam_unit_get_id (cpc->unit), ctl_id);
        }
    }
}

static void
_end_element (GMarkupParseContext *ctx, const char *element_name,
        void *user_data, GError **error)
{
    ChainParseContext *cpc = user_data;
    if (!strcmp (element_name, "control"))
        cpc->ctl = NULL;
    else if (!strcmp (element_name, "chain"))
        cpc->in_chain = 0;
    else if (!strcmp (element_name, "unit"))
        cpc->unit = NULL;
}

static void
_text (GMarkupParseContext *ctx, const char *text, gsize text_len, 
        void *user_data, GError **error)
{
    ChainParseContext *cpc = user_data;
    char buf[text_len + 1];
    memcpy (buf, text, text_len);
    buf[text_len] = 0;
    char *e = NULL;
    if (cpc->ctl) {
        const char *ctl_id = cam_unit_control_get_id(cpc->ctl);
        switch (cam_unit_control_get_control_type(cpc->ctl)) {
            case CAM_UNIT_CONTROL_TYPE_INT:
                {
                    long int val = strtol (buf, &e, 10);
                    if (e == buf) {
                        *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                                "invalid value [%s] for control [%s]", 
                                buf, ctl_id);
                    }
                    cam_unit_control_try_set_int (cpc->ctl, val);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_BOOLEAN:
                {
                    long int val = strtol (buf, &e, 10);
                    if (e == buf) {
                        *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                                "invalid value [%s] for control [%s]", 
                                buf, ctl_id);
                    }
                    cam_unit_control_try_set_boolean (cpc->ctl, val);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_ENUM:
                {
                    long int val = strtol (buf, &e, 10);
                    if (e == buf) {
                        *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                                "invalid value [%s] for control [%s]", 
                                buf, ctl_id);
                    }
                    cam_unit_control_try_set_enum (cpc->ctl, val);
                }
                break;
            case CAM_UNIT_CONTROL_TYPE_STRING:
                cam_unit_control_try_set_string (cpc->ctl, buf);
                break;
            case CAM_UNIT_CONTROL_TYPE_FLOAT:
                {
                    float val = strtof (buf, &e);
                    if (e == buf) {
                        *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                                "invalid value [%s] for control [%s]", 
                                buf, ctl_id);
                    }
                    cam_unit_control_try_set_float (cpc->ctl, val);
                }
                break;
            default:
                break;
        }
    }
}

static void
_parse_error (GMarkupParseContext *ctx, GError *error, void *user_data)
{
    fprintf (stderr, "parse error?\n");
    fprintf (stderr, "%s\n", error->message);
    fprintf (stderr, "===\n");
}

static GMarkupParser _parser = {
    .start_element = _start_element,
    .end_element = _end_element,
    .text = _text,
    .passthrough = NULL,
    .error = _parse_error
};

void
cam_unit_chain_load_from_str (CamUnitChain *self, const char *xml_str, 
        GError **error)
{
    cam_unit_chain_remove_all_units (self);

    cam_unit_chain_all_units_stream_init (self);

    if (! self->manager) {
        if (error) {
            *error = g_error_new (CAM_ERROR_DOMAIN, 0, 
                    "cannot load from string without a manager");
        }
        return;
    }

    GEnumClass *pfmt_class = 
        G_ENUM_CLASS (g_type_class_ref (CAM_TYPE_PIXEL_FORMAT));

    ChainParseContext cpc = {
        .chain = self,
        .in_chain = 0,
        .unit = NULL,
        .ctl = NULL,
        .error_domain = CAM_ERROR_DOMAIN,
        .pfmt_class = pfmt_class
    };

    GMarkupParseContext *ctx = g_markup_parse_context_new (&_parser,
            0, &cpc, NULL);

    GError *parse_err = NULL;
    g_markup_parse_context_parse (ctx, xml_str, strlen (xml_str), &parse_err);

    g_type_class_unref (pfmt_class);

//    int result = parse_err ? -1 : 0;
    if (parse_err) {
        if (error) {
            *error = parse_err;
        } else {
            g_error_free (parse_err);
        }
    }

    g_markup_parse_context_free (ctx);
}
