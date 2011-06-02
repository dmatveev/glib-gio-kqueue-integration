#include "config.h"

#include "gkqueuedirectorymonitor.h"
#include <gio/giomodule.h>

struct _GKqueueDirectoryMonitor
{
  GLocalDirectoryMonitor parent_instance;
};

static gboolean g_kqueue_directory_monitor_cancel (GFileMonitor *monitor);

#define g_kqueue_directory_monitor_get_type _g_kqueue_directory_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GKqueueDirectoryMonitor, g_kqueue_directory_monitor, G_TYPE_LOCAL_DIRECTORY_MONITOR,
       g_io_extension_point_implement (G_LOCAL_DIRECTORY_MONITOR_EXTENSION_POINT_NAME,
               g_define_type_id,
               "kqueue",
               20))

static void
g_kqueue_directory_monitor_finalize (GObject *object)
{
  GKqueueDirectoryMonitor *kqueue_monitor = G_KQUEUE_DIRECTORY_MONITOR (object);

  if (G_OBJECT_CLASS (g_kqueue_directory_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_kqueue_directory_monitor_parent_class)->finalize) (object);
}

static GObject*
g_kqueue_directory_monitor_constructor (GType                 type,
                                        guint                 n_construct_properties,
                                        GObjectConstructParam *construct_properties)
{
  GObject *obj;
  GKqueueDirectoryMonitorClass *klass;
  GObjectClass *parent_class;
  GKqueueDirectoryMonitor *kqueue_monitor;

  klass = G_KQUEUE_DIRECTORY_MONITOR_CLASS (g_type_class_peek (G_TYPE_KQUEUE_DIRECTORY_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  kqueue_monitor = G_KQUEUE_DIRECTORY_MONITOR (obj);

  return obj;
}

static gboolean
g_kqueue_directory_monitor_is_supported (void)
{
  return TRUE; /* TODO kqueue startup here */
}

static void
g_kqueue_directory_monitor_class_init (GKqueueDirectoryMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *directory_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalDirectoryMonitorClass *local_directory_monitor_class = G_LOCAL_DIRECTORY_MONITOR_CLASS (klass);

  gobject_class->finalize = g_kqueue_directory_monitor_finalize;
  gobject_class->constructor = g_kqueue_directory_monitor_constructor;
  directory_monitor_class->cancel = g_kqueue_directory_monitor_cancel;

  local_directory_monitor_class->mount_notify = TRUE; /* TODO: ??? */
  local_directory_monitor_class->is_supported = g_kqueue_directory_monitor_is_supported;
}

static void
g_kqueue_directory_monitor_init (GKqueueDirectoryMonitor *monitor)
{
}

static gboolean
g_kqueue_directory_monitor_cancel (GFileMonitor *monitor)
{
  GKqueueDirectoryMonitor *kqueue_monitor = G_KQUEUE_DIRECTORY_MONITOR (monitor);

  if (G_FILE_MONITOR_CLASS (g_kqueue_directory_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_kqueue_directory_monitor_parent_class)->cancel) (monitor);
}
