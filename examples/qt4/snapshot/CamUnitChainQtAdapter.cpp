#include <stdio.h>
#include <assert.h>

#include "CamUnitChainQtAdapter.hpp"

static inline int64_t _timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

CamUnitChainQtAdapterUnitHandler::CamUnitChainQtAdapterUnitHandler(CamUnit *unit)
{
    this->unit = unit;
    this->socket_notifier = NULL;
    this->timer = NULL;

    g_object_ref(this->unit);
    g_signal_connect (G_OBJECT(unit), "status-changed", 
            G_CALLBACK(CamUnitChainQtAdapterUnitHandler::on_unit_status_changed),
            this);

    this->check_events();
}

CamUnitChainQtAdapterUnitHandler::~CamUnitChainQtAdapterUnitHandler()
{
    g_signal_handlers_disconnect_by_func(G_OBJECT(this->unit),
            (void*)CamUnitChainQtAdapterUnitHandler::on_unit_status_changed, 
            this);
    g_object_unref(this->unit);
    this->unit = NULL;
}

void
CamUnitChainQtAdapterUnitHandler::check_events()
{
    if (cam_unit_is_streaming(unit)) {
        uint32_t flags = cam_unit_get_flags(unit);
        if (flags & CAM_UNIT_EVENT_METHOD_FD) {

            if (this->socket_notifier)
                delete this->socket_notifier;

            this->socket_notifier = 
                new QSocketNotifier (cam_unit_get_fileno(unit),
                        QSocketNotifier::Read);
            this->socket_notifier->setEnabled(true);
            this->connect (this->socket_notifier, SIGNAL(activated(int)), 
                    SLOT(on_unit_fd_ready(int)));
        } else if (flags & CAM_UNIT_EVENT_METHOD_TIMEOUT) {
            if (!this->timer) {
                this->timer = new QTimer();
                this->timer->setSingleShot(true);
                this->connect (this->timer, SIGNAL(timeout()),
                        SLOT(on_timer()));
            }

            int64_t now = _timestamp_now();
            int64_t event_time = cam_unit_get_next_event_time (unit);

            int dt_ms = (int) ((event_time - now) / 1000);
            if (dt_ms < 0) dt_ms = 0;

            this->timer->start(dt_ms);
        }
    } else {
        if (this->socket_notifier) {
            delete this->socket_notifier;
            this->socket_notifier = NULL;
        }
        if (this->timer) {
            delete this->timer;
            this->timer = NULL;
        }
    }
}

void
CamUnitChainQtAdapterUnitHandler::on_unit_status_changed (CamUnit *unit, 
        void *user_data)
{
    CamUnitChainQtAdapterUnitHandler *self = 
        (CamUnitChainQtAdapterUnitHandler*) (user_data);
    self->check_events();
    (void) unit;
}

void
CamUnitChainQtAdapterUnitHandler::on_unit_fd_ready (int fd)
{
    cam_unit_try_produce_frame (this->unit, 0);
    (void)fd;
}

void
CamUnitChainQtAdapterUnitHandler::on_timer()
{
    cam_unit_try_produce_frame (this->unit, 0);
    if (cam_unit_is_streaming (this->unit)) {
        int64_t now = _timestamp_now();
        int64_t event_time = cam_unit_get_next_event_time (unit);

        int dt_ms = (int) ((event_time - now) / 1000);
        if (dt_ms < 0) dt_ms = 0;

        this->timer->start(dt_ms);
    }
}

CamUnitChainQtAdapter::CamUnitChainQtAdapter (CamUnitChain *chain)
{
    this->chain = chain;
    g_signal_connect (G_OBJECT(chain), "unit-added",
            G_CALLBACK(CamUnitChainQtAdapter::on_unit_added), this);
    g_signal_connect (G_OBJECT(chain), "unit-removed",
            G_CALLBACK(CamUnitChainQtAdapter::on_unit_removed), this);
    this->unit_handlers = g_hash_table_new (g_direct_hash, g_direct_equal);
    g_object_ref (this->chain);

    GList * units = cam_unit_chain_get_units (this->chain);
    for (GList *uiter=units; uiter; uiter=uiter->next) {
        CamUnit *unit = (CamUnit*) uiter->data;
        g_hash_table_insert (this->unit_handlers, unit, 
                new CamUnitChainQtAdapterUnitHandler(unit));
    }
}

static void
_delete_handler (gpointer key, gpointer value, gpointer user_data)
{
    CamUnitChainQtAdapterUnitHandler * handler = 
        (CamUnitChainQtAdapterUnitHandler*) value;
    delete handler;
    (void) key;
    (void) user_data;
}

CamUnitChainQtAdapter::~CamUnitChainQtAdapter()
{
    g_signal_handlers_disconnect_by_func(G_OBJECT(this->chain),
            (void*)CamUnitChainQtAdapter::on_unit_added, this);
    g_signal_handlers_disconnect_by_func(G_OBJECT(this->chain),
            (void*)CamUnitChainQtAdapter::on_unit_removed, this);

    g_hash_table_foreach (this->unit_handlers, _delete_handler, NULL);
    g_hash_table_destroy (this->unit_handlers);
    this->unit_handlers = NULL;
    g_object_unref (this->chain);
    this->chain = NULL;
}

void
CamUnitChainQtAdapter::on_unit_added (CamUnitChain *chain, CamUnit *unit, 
        void *user_data)
{
    CamUnitChainQtAdapter *self = (CamUnitChainQtAdapter*) (user_data);
    CamUnitChainQtAdapterUnitHandler *existing = 
        (CamUnitChainQtAdapterUnitHandler*) g_hash_table_lookup (
                self->unit_handlers, 
            (void*)unit);
    assert (!existing);
    g_hash_table_insert (self->unit_handlers, unit, 
            new CamUnitChainQtAdapterUnitHandler(unit));
    (void) chain;
}

void
CamUnitChainQtAdapter::on_unit_removed (CamUnitChain *chain, CamUnit *unit, 
        void *user_data)
{
    CamUnitChainQtAdapter *self = (CamUnitChainQtAdapter*) (user_data);
    CamUnitChainQtAdapterUnitHandler *handler = 
        (CamUnitChainQtAdapterUnitHandler*) g_hash_table_lookup (
                self->unit_handlers, 
                (void*)unit);
    assert (handler);
    g_hash_table_remove (self->unit_handlers, unit);
    delete handler;
    (void) chain;
}
