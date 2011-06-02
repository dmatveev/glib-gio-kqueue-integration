#ifndef __G_KQUEUE_FILE_MONITOR_H__
#define __G_KQUEUE_FILE_MONITOR_H__

#include <glib-object.h>
#include <string.h>
#include <gio/gfilemonitor.h>
#include <gio/glocalfilemonitor.h>
#include <gio/giomodule.h>

G_BEGIN_DECLS

#define G_TYPE_KQUEUE_FILE_MONITOR        (_g_kqueue_file_monitor_get_type ())
#define G_KQUEUE_FILE_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_KQUEUE_FILE_MONITOR, GKqueueFileMonitor))
#define G_KQUEUE_FILE_MONITOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_KQUEUE_FILE_MONITOR, GKqueueFileMonitorClass))
#define G_IS_KQUEUE_FILE_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_KQUEUE_FILE_MONITOR))
#define G_IS_KQUEUE_FILE_MONITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_KQUEUE_FILE_MONITOR))

typedef struct _GKqueueFileMonitor      GKqueueFileMonitor;
typedef struct _GKqueueFileMonitorClass GKqueueFileMonitorClass;

struct _GKqueueFileMonitorClass {
  GLocalFileMonitorClass parent_class;
};

GType _g_kqueue_file_monitor_get_type (void);

G_END_DECLS

#endif /* __G_KQUEUE_FILE_MONITOR_H__ */
