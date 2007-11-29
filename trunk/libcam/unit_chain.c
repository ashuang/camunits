#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include <glib-object.h>

#include "libcam-gmarshal.h"
#include "unit_chain.h"
#include "dbg.h"

#define err(args...) fprintf (stderr, args)

struct _CamUnitChainSource {
    GSource gsource;
    CamUnitChain *chain;
};

enum {
    BUFFERS_READY_SIGNAL,
    UNIT_ADDED_SIGNAL,
    UNIT_REMOVED_SIGNAL,
    UNIT_REORDERED_SIGNAL,
    DESIRED_STATUS_CHANGED_SIGNAL,
    FRAME_READY_SIGNAL,
    LAST_SIGNAL
};

static guint chain_signals[LAST_SIGNAL] = { 0 };

static void cam_unit_chain_finalize (GObject *obj);
static int are_all_units_status (CamUnitChain *self, int status);
static void update_unit_status (CamUnitChain *self, CamUnit *unit,
       CamUnitStatus desired);
static void update_unit_statuses (CamUnitChain *self);

static gboolean cam_unit_chain_source_prepare (GSource *source, gint *timeout);
static gboolean cam_unit_chain_source_check (GSource *source);
static gboolean cam_unit_chain_source_dispatch (GSource *source, 
        GSourceFunc callback, void *user_data);
static void cam_unit_chain_source_finalize (GSource *source);
static void on_unit_status_changed (CamUnit *unit, int old_status, 
        CamUnitChain *self);
//static void print_chain (CamUnitChain *self);

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
    self->event_source = (CamUnitChainSource*) g_source_new (
            &self->source_funcs,
            sizeof (CamUnitChainSource));
    self->event_source->chain = self;
    self->desired_unit_status = CAM_UNIT_STATUS_IDLE;
}

static void
cam_unit_chain_class_init (CamUnitChainClass *klass)
{
    dbg (DBG_CHAIN, "class initializer\n");
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = cam_unit_chain_finalize;

    /**
     * CamUnitChain::buffer-ready
     *
     * The buffer-ready signal is emitted when the last unit in the chain has
     * new buffers in its outgoing queue
     */
    chain_signals[BUFFERS_READY_SIGNAL] = g_signal_new ("buffer-ready",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    /**
     * CamUnitChain::unit-added
     * @chain: the CamUnitChain emitting the signal
     * @unit: the CamUnit being added.
     *
     * The unit-added signal is emitted when a new unit is added to the chain.
     */
    chain_signals[UNIT_ADDED_SIGNAL] = g_signal_new ("unit-added",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
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
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1,
            CAM_TYPE_UNIT);
    /**
     * CamUnitChain::desired-status-changed
     * @chain: the CamUnitChain emitting the signal
     * 
     * The desired-status-changed signal is emitted when the desired status of
     * every unit in the chain changes.
     * See also cam_unit_chain_set_desired_status
     */
    chain_signals[DESIRED_STATUS_CHANGED_SIGNAL] = 
        g_signal_new ("desired-status-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
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
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
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
            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
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

    // unref the CamUnitManager
    if (self->manager) {
        dbgl (DBG_REF, "unref manager\n");
        g_object_unref (self->manager);
    }
    g_source_unref ( (GSource*) self->event_source);

    // if units are not all idle, then it's not terrible, but it is poor form.
    if (! are_all_units_status (self, CAM_UNIT_STATUS_IDLE)) {
        err ("Chain:  Destroying a chain with non-idle units!!!!\n");
    }

    // release units in the chain
    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        dbgl (DBG_REF, "unref unit\n");
        g_object_unref (unit);
    }
    g_list_free (self->units);

    G_OBJECT_CLASS (cam_unit_chain_parent_class)->finalize (obj);
}

CamUnitChain *
cam_unit_chain_new (void)
{
    CamUnitChain *self = 
        CAM_UNIT_CHAIN (g_object_new (CAM_TYPE_UNIT_CHAIN, NULL));
    self->manager = cam_unit_manager_new (TRUE);
    return self;
}

CamUnitChain *
cam_unit_chain_new_with_manager (CamUnitManager *manager)
{
    CamUnitChain *self = 
        CAM_UNIT_CHAIN (g_object_new (CAM_TYPE_UNIT_CHAIN, NULL));
    if (manager) {
        self->manager = manager;
        dbgl (DBG_REF, "ref manager\n");
        g_object_ref (manager);
    }
    return self;
}

static int
are_all_units_status (CamUnitChain *self, int status) 
{
    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (cam_unit_get_status (unit) != status) 
            return 0;
    }
    return 1;
}

CamUnitManager *
cam_unit_chain_get_manager (CamUnitChain *self)
{
    return self->manager;
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

    // if the new unit has an input unit, then set it.
    if (link->prev) {
        CamUnit *prev_unit = (CamUnit*) link->prev->data;
        update_unit_status (self, unit, CAM_UNIT_STATUS_IDLE);
        cam_unit_set_input (unit, prev_unit);
    }
    update_unit_status (self, unit, self->desired_unit_status);

    // subscribe to be notified when the status of the unit changes.
    g_signal_connect (G_OBJECT (unit), "status-changed",
            G_CALLBACK (on_unit_status_changed), self);

    // if the new unit comes before the end of the chain, then set the next
    // unit's input
    if (link->next) {
        CamUnit *next_unit = (CamUnit*) link->next->data;
        update_unit_status (self, next_unit, CAM_UNIT_STATUS_IDLE);
        cam_unit_set_input (next_unit, unit);
        update_unit_status (self, next_unit, self->desired_unit_status);
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

    self->units = g_list_delete_link (self->units, link);
    g_signal_handlers_disconnect_by_func (unit, on_unit_status_changed, self);
    g_signal_emit (G_OBJECT (self), chain_signals[UNIT_REMOVED_SIGNAL],
            0, unit);
    dbgl (DBG_REF, "unref unit [%s]\n", cam_unit_get_id (unit));
    update_unit_status (self, unit, CAM_UNIT_STATUS_IDLE);
    cam_unit_set_input (unit, NULL);
    g_object_unref (unit);

    if (next) {
        update_unit_status (self, next, CAM_UNIT_STATUS_IDLE);
        cam_unit_set_input (next, prev);
        update_unit_status (self, next, self->desired_unit_status);
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
    GList *uiter;
    for (uiter=g_list_last (ucopy); uiter; uiter=uiter->prev) {
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
    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
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
    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
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
        update_unit_status (self, oldnext, CAM_UNIT_STATUS_IDLE);
        cam_unit_set_input (oldnext, oldprev);
        update_unit_status (self, oldnext, self->desired_unit_status);
    }

    self->units = g_list_remove (self->units, unit);

    self->units = g_list_insert (self->units, unit, new_index);
    GList *link = g_list_nth (self->units, new_index);

    update_unit_status (self, unit, CAM_UNIT_STATUS_IDLE);
    if (link->prev) {
        CamUnit *prev_unit = CAM_UNIT (link->prev->data);
        cam_unit_set_input (unit, prev_unit);
    } else {
        cam_unit_set_input (unit, NULL);
    }
    update_unit_status (self, unit, self->desired_unit_status);
    if (link->next) {
        CamUnit *next_unit = CAM_UNIT (link->next->data);
        update_unit_status (self, next_unit, CAM_UNIT_STATUS_IDLE);
        cam_unit_set_input (next_unit, unit);
        update_unit_status (self, next_unit, self->desired_unit_status);
    }

    g_signal_emit (G_OBJECT (self), chain_signals[UNIT_REORDERED_SIGNAL],
            0, unit);

    return 0;
}

int 
cam_unit_chain_set_desired_status (CamUnitChain *self, 
        CamUnitStatus status)
{
    if (status == CAM_UNIT_STATUS_READY ||
        status == CAM_UNIT_STATUS_STREAMING ||
        status == CAM_UNIT_STATUS_IDLE) {
        int changing = status != self->desired_unit_status;

        self->desired_unit_status = status;
        update_unit_statuses (self);

        if (changing) {
            g_signal_emit (G_OBJECT (self), 
                    chain_signals[DESIRED_STATUS_CHANGED_SIGNAL], 0);
        }
        return 0;
    } 
    return -1;
}


CamUnitStatus 
cam_unit_chain_get_desired_status (const CamUnitChain *self)
{
    return self->desired_unit_status;
}

CamUnit *
cam_unit_chain_check_status_all_units (const CamUnitChain *self,
        CamUnitStatus status)
{
    for (const GList *uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        if (cam_unit_get_status (unit) != status) {
            return unit;
        }
    }
    return NULL;
}

static int
stream_on (CamUnitChain *self, CamUnit *unit)
{
    CamUnitStatus status = cam_unit_get_status (unit);

    if (status == CAM_UNIT_STATUS_STREAMING) return 0;

    if (status == CAM_UNIT_STATUS_READY) {
        int res = cam_unit_stream_on (unit);
        if (0 != res) {
            err ("Chain: Unit %s did not start streaming!\n", 
                    cam_unit_get_id (unit));
            return -1;
        }
        dbg (DBG_CHAIN, "Unit %s started streaming\n", cam_unit_get_id (unit));

        // if the unit provides a file descriptor, then attach it to the
        // chain event source
        if (cam_unit_get_flags (unit) & CAM_UNIT_EVENT_METHOD_FD) {
            GPollFD *pfd = (GPollFD*) malloc (sizeof (GPollFD));

            pfd->fd = cam_unit_get_fileno (unit);
            pfd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
            pfd->revents = 0;

            g_object_set_data (G_OBJECT (unit), "ChainPollFD", pfd);

            g_source_add_poll ( (GSource *)self->event_source, pfd);
        }

        return 0;
    }

    return -1;
}

static int
stream_off (CamUnitChain *self, CamUnit *unit)
{
    int res = cam_unit_stream_off (unit);
    if (0 != res) {
        err ("Chain:  Error %d while trying to stop unit %s\n", 
                res, cam_unit_get_id (unit));
    }

    // remove a GPollFD if it was setup by stream_on
    GPollFD *pfd = (GPollFD*) g_object_get_data (G_OBJECT (unit), "ChainPollFD");
    if (pfd) {
        g_source_remove_poll ( (GSource*)self->event_source, pfd);
        g_object_set_data (G_OBJECT (unit), "ChainPollFD", NULL);
        free (pfd);
    }
    return res;
}

static void
try_produce_frame (CamUnitChain *self, GList *unit_link)
{
    CamUnit *unit = CAM_UNIT (unit_link->data);

    if (cam_unit_get_status (unit) == CAM_UNIT_STATUS_STREAMING) {
        cam_unit_try_produce_frame (unit, 0);
    }

    return;
}

static void
update_unit_status (CamUnitChain *self, CamUnit *unit, CamUnitStatus desired)
{
    CamUnitStatus status = cam_unit_get_status (unit);
    if (status == desired) return;
    const char *unit_id = cam_unit_get_id (unit);
    dbg (DBG_CHAIN, "checking status of unit [%s]\n", unit_id);

    if (status == CAM_UNIT_STATUS_IDLE) {
        dbg (DBG_CHAIN, "stream_init on [%s]\n", unit_id);
        cam_unit_stream_init_any_format (unit);
        status = cam_unit_get_status (unit);
        if (status == desired) return;
    }

    if (status == CAM_UNIT_STATUS_STREAMING) {
        dbg (DBG_CHAIN, "stream_off on [%s]\n", unit_id);
        stream_off (self, unit);
        status = cam_unit_get_status (unit);
        if (status == desired) return;
    }

    if (status == CAM_UNIT_STATUS_READY) {
        if (desired == CAM_UNIT_STATUS_IDLE) {
            dbg (DBG_CHAIN, "stream_shutdown on [%s]\n", unit_id);
            cam_unit_stream_shutdown (unit);
        } else {
            dbg (DBG_CHAIN, "stream_on on [%s]\n", unit_id);
            stream_on (self, unit);
        }
    }
}

static void
update_unit_statuses (CamUnitChain *self)
{
    if (! self->units) return;

    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        // all units must be either ready or streaming to proceed
        CamUnit *unit = CAM_UNIT (uiter->data);
        update_unit_status (self, unit, self->desired_unit_status);
    }
}

static gboolean
cam_unit_chain_source_prepare (GSource *source, gint *timeout)
{
    CamUnitChainSource * csource = (CamUnitChainSource *) source;
    CamUnitChain * self = csource->chain;

//    dbg (DBG_CHAIN, "source prepare (%d units)\n", g_list_length (self->units));
//    print_chain (self);

    *timeout = -1;

    self->pending_unit_link = NULL;

    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        uint32_t uflags = cam_unit_get_flags (unit);

        if (uflags & CAM_UNIT_EVENT_METHOD_TIMEOUT &&
            cam_unit_get_status (unit) == CAM_UNIT_STATUS_STREAMING) {

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

    GList *uiter;
    for (uiter=self->units; uiter; uiter=uiter->next) {
        CamUnit *unit = CAM_UNIT (uiter->data);
        uint32_t uflags = cam_unit_get_flags (unit);

        if (cam_unit_get_status (unit) != CAM_UNIT_STATUS_STREAMING) {
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

    try_produce_frame (self, self->pending_unit_link);
    self->pending_unit_link = NULL;

    return TRUE;
}

static void
cam_unit_chain_source_finalize (GSource *source)
{
    dbg (DBG_CHAIN, "source finalize\n");
    // TODO
}

void 
cam_unit_chain_attach_glib_mainloop (CamUnitChain *self, int priority)
{
    g_source_attach ( (GSource*) self->event_source, NULL);
    g_source_set_priority ( (GSource*) self->event_source, 1000);
}

void 
cam_unit_chain_detach_glib_mainloop (CamUnitChain *self)
{
    g_source_destroy ( (GSource*) self->event_source);
    self->event_source = NULL;
}

static void
on_unit_status_changed (CamUnit *unit, int old_status, CamUnitChain *self)
{
    dbg (DBG_CHAIN, "status of [%s] changed to %s\n", 
            cam_unit_get_id (unit),
            cam_unit_status_to_str (cam_unit_get_status (unit)));
    CamUnitStatus newstatus = cam_unit_get_status (unit);
    if (old_status == CAM_UNIT_STATUS_IDLE && 
        newstatus == CAM_UNIT_STATUS_READY) {

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
                update_unit_status (self, cunit, CAM_UNIT_STATUS_IDLE);
                cam_unit_set_input (cunit, cam_unit_get_input (cunit));
                update_unit_status (self, cunit, self->desired_unit_status);
                g_signal_handlers_unblock_by_func (cunit,
                        on_unit_status_changed, self);
            }
            if (cunit == unit) restart = 1;
        }
    }
}
