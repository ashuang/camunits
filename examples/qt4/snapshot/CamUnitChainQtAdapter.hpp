#ifndef __cam_unit_chain_qt_adapter_hpp__
#define __cam_unit_chain_qt_adapter_hpp__

#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include <camunits/cam.h>

class CamUnitChainQtAdapterUnitHandler : public QObject
{
    Q_OBJECT

    friend class CamUnitChainQtAdapter;

    public:
        virtual ~CamUnitChainQtAdapterUnitHandler();

    private:
        CamUnitChainQtAdapterUnitHandler(CamUnit *unit);

        CamUnit *unit;
        static void on_unit_status_changed(CamUnit *unit, void *user_data);

        QSocketNotifier *socket_notifier;
        QTimer *timer;

        void check_events();

    private slots:
        void on_unit_fd_ready(int fd);
        void on_timer();
};

/**
 * Attaches a CamUnitChain to the Qt event loop.  Create an instance of this
 * class as a replacement for calling cam_unit_chain_attach_glib()
 */
class CamUnitChainQtAdapter
{
    public:
        CamUnitChainQtAdapter(CamUnitChain *chain);
        virtual ~CamUnitChainQtAdapter();
    private:
        CamUnitChain *chain;

        GHashTable *unit_handlers;

        static void on_unit_added(CamUnitChain *chain, CamUnit *unit,
                void *user_data);
        static void on_unit_removed(CamUnitChain *chain, CamUnit *unit,
                void *user_data);
};

#endif
