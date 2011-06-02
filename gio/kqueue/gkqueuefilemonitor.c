#include "config.h"

#include "gkqueuefilemonitor.h"
#include <gio/giomodule.h>

struct _GKqueueFileMonitor
{
  GLocalFileMonitor parent_instance;
};

static gboolean g_kqueue_file_monitor_cancel (GFileMonitor* monitor);

#define g_kqueue_file_monitor_get_type _g_kqueue_file_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GKqueueFileMonitor, g_kqueue_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
       g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
               g_define_type_id,
               "kqueue",
               20))

static void
g_kqueue_file_monitor_finalize (GObject *object)
{
  GKqueueFileMonitor *kqueue_monitor = G_KQUEUE_FILE_MONITOR (object);

  if (G_OBJECT_CLASS (g_kqueue_file_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_kqueue_file_monitor_parent_class)->finalize) (object);
}

static GObject*
g_kqueue_file_monitor_constructor (GType                 type,
                                   guint                 n_construct_properties,
                                   GObjectConstructParam *construct_properties)
{
  GObject *obj;
  GKqueueFileMonitorClass *klass;
  GObjectClass *parent_class;
  GKqueueFileMonitor *kqueue_monitor;

  klass = G_KQUEUE_FILE_MONITOR_CLASS (g_type_class_peek (G_TYPE_KQUEUE_FILE_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  kqueue_monitor = G_KQUEUE_FILE_MONITOR (obj);

  return obj;
}

static gboolean
g_kqueue_file_monitor_is_supported (void)
{
  return TRUE; /* todo: kqueue startup code here */
}

static void
g_kqueue_file_monitor_class_init (GKqueueFileMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalFileMonitorClass *local_file_monitor_class = G_LOCAL_FILE_MONITOR_CLASS (klass);

  gobject_class->finalize = g_kqueue_file_monitor_finalize;
  gobject_class->constructor = g_kqueue_file_monitor_constructor;
  file_monitor_class->cancel = g_kqueue_file_monitor_cancel;

  local_file_monitor_class->is_supported = g_kqueue_file_monitor_is_supported;
}

static void
g_kqueue_file_monitor_init (GKqueueFileMonitor *monitor)
{
}

static gboolean
g_kqueue_file_monitor_cancel (GFileMonitor *monitor)
{
  GKqueueFileMonitor *kqueue_monitor = G_KQUEUE_FILE_MONITOR (monitor);

  if (G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel) (monitor);

  return TRUE;
}
