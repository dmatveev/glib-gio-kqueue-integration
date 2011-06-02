#ifndef __G_KQUEUE_DIRECTORY_MONITOR_H__
#define __G_KQUEUE_DIRECTORY_MONITOR_H__

#include <glib-object.h>
#include <gio/glocaldirectorymonitor.h>
#include <gio/giomodule.h>

G_BEGIN_DECLS

#define G_TYPE_KQUEUE_DIRECTORY_MONITOR		     (_g_kqueue_directory_monitor_get_type ())
#define G_KQUEUE_DIRECTORY_MONITOR(o)			     (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_KQUEUE_DIRECTORY_MONITOR, GKqueueDirectoryMonitor))
#define G_KQUEUE_DIRECTORY_MONITOR_CLASS(k)		 (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_KQUEUE_DIRECTORY_MONITOR, GKqueueDirectoryMonitorClass))
#define G_IS_KQUEUE_DIRECTORY_MONITOR(o)		   (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_KQUEUE_DIRECTORY_MONITOR))
#define G_IS_KQUEUE_DIRECTORY_MONITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_KQUEUE_DIRECTORY_MONITOR))

typedef struct _GKqueueDirectoryMonitor      GKqueueDirectoryMonitor;
typedef struct _GKqueueDirectoryMonitorClass GKqueueDirectoryMonitorClass;

struct _GKqueueDirectoryMonitorClass {
  GLocalDirectoryMonitorClass parent_class;
};

GType _g_kqueue_directory_monitor_get_type (void);

G_END_DECLS

#endif /* __G_KQUEUE_DIRECTORY_MONITOR_H__ */
