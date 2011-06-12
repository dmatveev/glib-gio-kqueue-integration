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

#include "gkqueuefilemonitor.h"
#include "kqueue-helper.h"
#include <gio/giomodule.h>
#include <assert.h>

/* TODO: This code is also written as in the inotify backend, but wait.
 * kqueue_sub already contains `filename' and `pair_moves' fields. Do
 * we need it in the `GKqueueFileMonitor', since we already have a `sub'
 * field? Probably I will drop it. */
struct _GKqueueFileMonitor
{
  GLocalFileMonitor parent_instance;
  gchar *filename;
  kqueue_sub *sub;
  gboolean pair_moves;
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
  kqueue_sub *sub = kqueue_monitor->sub;

  if (sub)
    {
      _kh_cancel_sub (sub);
      _kh_sub_free (sub);
      kqueue_monitor->sub = NULL;
    }

  if (kqueue_monitor->filename)
    {
      g_free (kqueue_monitor->filename);
      kqueue_monitor->filename = NULL;
    }

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
  kqueue_sub *sub = NULL;
  gboolean ret_kh_startup;

  klass = G_KQUEUE_FILE_MONITOR_CLASS (g_type_class_peek (G_TYPE_KQUEUE_FILE_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  kqueue_monitor = G_KQUEUE_FILE_MONITOR (obj);
  kqueue_monitor->filename = g_strdup (G_LOCAL_FILE_MONITOR (obj)->filename);

  ret_kh_startup = _kh_startup();
  assert (ret_kh_startup);

  /* TODO: pair moves. */
  sub = _kh_sub_new (kqueue_monitor->filename,
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
g_kqueue_file_monitor_is_supported (void)
{
  return _kh_startup();
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
  kqueue_sub *sub = kqueue_monitor->sub;

  if (sub)
    {
      _kh_cancel_sub (sub);
      _kh_sub_free (sub);
      kqueue_monitor->sub = NULL;
    }

  if (G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel) (monitor);

  return TRUE;
}
