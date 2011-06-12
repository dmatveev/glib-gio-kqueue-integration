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
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <gio/glocalfile.h>
#include <gio/gfilemonitor.h>
#include <gio/gfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include "kqueue-helper.h"
#include "kqueue-thread.h"
#include "kqueue-missing.h"

static gboolean kh_debug_enabled = TRUE;
#define KH_W if (kh_debug_enabled) g_warning

G_GNUC_INTERNAL G_LOCK_DEFINE (kqueue_lock);

static GHashTable *g_sub_hash = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (hash_lock);

int g_kqueue = -1;
static int g_sockpair[] = {-1, -1};
static pthread_t g_kqueue_thread;


void _kh_file_appeared_cb (kqueue_sub *sub);


static GFileMonitorEvent
convert_kqueue_events_to_gio (uint32_t flags)
{
  static struct {
    uint32_t kqueue_code;
    GFileMonitorEvent gio_code;
  } translations[] = {
    {NOTE_DELETE, G_FILE_MONITOR_EVENT_DELETED},
    {NOTE_ATTRIB, G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED},
    {NOTE_WRITE,  G_FILE_MONITOR_EVENT_CHANGED},
    {NOTE_EXTEND, G_FILE_MONITOR_EVENT_CHANGED},
    {NOTE_RENAME, G_FILE_MONITOR_EVENT_MOVED}
  };
  /* TODO: The following notifications should be emulated, if possible:
   *   G_FILE_MONITOR_EVENT_PRE_UNMOUNT
   *   G_FILE_MONITOR_EVENT_UNMOUNTED */
  
  GFileMonitorEvent result = 0;
  int i;

  for (i = 0; i < sizeof (translations) / sizeof (translations[0]); i++)
    {
      if (flags & translations[i].kqueue_code)
      {
        result |= translations[i].gio_code;
      }
    }
  return result;
}


static gboolean
process_kqueue_notifications (GIOChannel  *gioc,
                              GIOCondition cond,
                              gpointer     data)
{
  struct kqueue_notification n;
  kqueue_sub *sub = NULL;
  GFileMonitor *monitor = NULL;
  GFile *child = NULL;
  GFile *other = NULL;
  GFileMonitorEvent mask = 0;
  
  g_assert (g_sockpair[0] != NULL);
  read (g_sockpair[0], &n, sizeof (struct kqueue_notification));

  sub = (kqueue_sub *) g_hash_table_lookup (g_sub_hash, GINT_TO_POINTER (n.fd));
  g_assert (sub != NULL);

  monitor = G_FILE_MONITOR(sub->user_data);
  g_assert (monitor != NULL);

  child = g_file_new_for_path (sub->filename);
  other = NULL; /* TODO: Do something. */

  if (n.flags & (NOTE_DELETE | NOTE_REVOKE))
  {
    _km_add_missing (sub);
    _kh_cancel_sub (sub);
  }
  mask  = convert_kqueue_events_to_gio (n.flags);

  g_file_monitor_emit_event (monitor, child, other, mask);
  return TRUE;
}



gboolean
_kh_startup (void)
{
  static gboolean initialized = FALSE;
  static gboolean result = FALSE;

  GIOChannel *channel;

  if (initialized == TRUE)
  {
    return result;
  }

  G_LOCK (kqueue_lock);

  if (initialized == TRUE)
    {
      /* it was a double-checked locking */
      G_UNLOCK (kqueue_lock);
      return result;
    }

  g_kqueue = kqueue();
  result = (-1 != g_kqueue);
  if (!result)
    {
      G_UNLOCK (kqueue_lock);
      KH_W ("Failed to initialize kqueue\n!");
      return FALSE;
    }

  result = (0 == socketpair(AF_UNIX, SOCK_STREAM, 0, g_sockpair));
  if (!result)
    {
      G_UNLOCK (kqueue_lock);
      KH_W ("Failed to create socket pair\n!");
      return FALSE;
    }

  result = (0 == pthread_create (&g_kqueue_thread,
                                 NULL,
                                 _kqueue_thread_func,
                                 &g_sockpair[1]));
  if (!result)
    {
      G_UNLOCK (kqueue_lock);
      KH_W ("Failed to run kqueue thread\n!");
      return FALSE;
    }

  _km_init (_kh_file_appeared_cb);

  channel = g_io_channel_unix_new (g_sockpair[0]);
  g_io_add_watch (channel, G_IO_IN, process_kqueue_notifications, NULL);

  g_sub_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  KH_W ("started gio kqueue backend\n");
  initialized = TRUE;

  G_UNLOCK (kqueue_lock);

  return TRUE;
}


gboolean
_kh_start_watching (kqueue_sub *sub)
{
  g_assert (g_sockpair[0] != -1);
  g_assert (sub != NULL);
  g_assert (sub->filename != NULL);

  /* kqueue requires a file descriptor to monitor. Sad but true */
  sub->fd = open (sub->filename, O_RDONLY);

  if (sub->fd == -1)
    {
      KH_W ("failed to open file %s\n", sub->filename);
      return FALSE;
    }

  G_LOCK (hash_lock);
  g_hash_table_insert (g_sub_hash, GINT_TO_POINTER(sub->fd), sub);
  G_UNLOCK (hash_lock);

  _kqueue_thread_push_fd (sub->fd);
  
  /* Bump the kqueue thread. It will pick up a new sub entry to monitor */
  write(g_sockpair[0], "A", 1);
  return TRUE;
}


gboolean
_kh_add_sub (kqueue_sub *sub)
{
  g_assert (sub != NULL);

  if (!_kh_start_watching (sub))
    _km_add_missing (sub);

  return TRUE;
}


gboolean
_kh_cancel_sub (kqueue_sub *sub)
{
  gboolean missing = FALSE;
  g_assert (g_sockpair[0] != -1);
  g_assert (sub != NULL);

  G_LOCK (hash_lock);
  missing = !g_hash_table_remove (g_sub_hash, GINT_TO_POINTER(sub->fd));
  G_UNLOCK (hash_lock);

  if (missing)
    {
      /* If there were no fd for this subscription, file is still
       * missing. */
      KH_W ("Removing subscription from missing");
      _km_remove (sub);
    }
  else
    {
      /* fd will be closed in the kqueue thread */
      _kqueue_thread_remove_fd (sub->fd);

      /* Bump the kqueue thread. It will pick up a new sub entry to remove*/
      write(g_sockpair[0], "R", 1);
    }

  return TRUE;
}


void
_kh_file_appeared_cb (kqueue_sub *sub)
{
  GFileMonitorEvent eflags;
  GFile* child;

  g_assert (sub != NULL);
  g_assert (sub->filename);

  if (!g_file_test (sub->filename, G_FILE_TEST_EXISTS))
    {
      return;
    }

  child = g_file_new_for_path (sub->filename);

  g_file_monitor_emit_event (G_FILE_MONITOR (sub->user_data),
                             child,
                             NULL,
                             G_FILE_MONITOR_EVENT_CREATED);

  g_object_unref (child);
}
