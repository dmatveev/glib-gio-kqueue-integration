/*******************************************************************************
  Copyright (c) 2011 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include "config.h"

#include "gkqueuedirectorymonitor.h"
#include "kqueue-helper.h"
#include <gio/giomodule.h>

struct _GKqueueDirectoryMonitor
{
  GLocalDirectoryMonitor parent_instance;
  kqueue_sub *sub;
  gboolean pair_moves;
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
  kqueue_sub *sub = kqueue_monitor->sub;
  
  if (sub)
    {
      _kh_cancel_sub (sub);
      _kh_sub_free (sub);
      kqueue_monitor->sub = NULL;
    }

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
  kqueue_sub *sub = NULL;
  gboolean ret_kh_startup;

  klass = G_KQUEUE_DIRECTORY_MONITOR_CLASS (g_type_class_peek (G_TYPE_KQUEUE_DIRECTORY_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  kqueue_monitor = G_KQUEUE_DIRECTORY_MONITOR (obj);

  ret_kh_startup = _kh_startup ();
  g_assert (ret_kh_startup);

  /* Pair moves notifications are unavailable for now, because kqueue does not
   * provide us enough info and we cannot *gracefully* determine a new file
   * path by a file descriptor.
   */
  sub = _kh_sub_new (G_LOCAL_DIRECTORY_MONITOR (obj)->dirname,
                     FALSE,
                     kqueue_monitor);

  /* FIXME: what to do about errors here? we can't return NULL or another
   * kind of error and an assertion is probably too hard (same issue as in
   * the inotify backend) */
  g_assert (sub != NULL);

  _kh_add_sub (sub);
  kqueue_monitor->sub = sub;

  return obj;
}

static gboolean
g_kqueue_directory_monitor_is_supported (void)
{
  return _kh_startup ();
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
  kqueue_sub *sub = kqueue_monitor->sub;

  if (sub)
    {
      _kh_cancel_sub (sub);
      _kh_sub_free (sub);
      kqueue_monitor->sub = NULL;
    }

  if (G_FILE_MONITOR_CLASS (g_kqueue_directory_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_kqueue_directory_monitor_parent_class)->cancel) (monitor);
}
