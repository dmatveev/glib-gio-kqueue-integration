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
#include <errno.h>
#include <pthread.h>
#include "kqueue-helper.h"
#include "kqueue-utils.h"
#include "kqueue-thread.h"
#include "kqueue-missing.h"

static gboolean kh_debug_enabled = FALSE;
#define KH_W if (kh_debug_enabled) g_warning

G_GNUC_INTERNAL G_LOCK_DEFINE (kqueue_lock);

static GHashTable *subs_hash_table = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (hash_lock);

int kqueue_descriptor = -1;
static int kqueue_socket_pair[] = {-1, -1};
static pthread_t kqueue_thread;


void _kh_file_appeared_cb (kqueue_sub *sub);

/**
 * convert_kqueue_events_to_gio:
 * @flags: a set of kqueue filter flags
 *
 * Translates kqueue filter flags into GIO event flags.
 *
 * Returns: a set of GIO flags (see #GFileMonitorEvent)
 **/
static GFileMonitorEvent
convert_kqueue_events_to_gio (uint32_t flags)
{
  GFileMonitorEvent result = 0;

  /* TODO: The following notifications should be emulated, if possible:
   *   G_FILE_MONITOR_EVENT_PRE_UNMOUNT
   *   G_FILE_MONITOR_EVENT_UNMOUNTED */
  if (flags & NOTE_DELETE)
    result |= G_FILE_MONITOR_EVENT_DELETED;
  if (flags & NOTE_ATTRIB)
    result |= G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED;
  if (flags & (NOTE_WRITE | NOTE_EXTEND))
    result |= G_FILE_MONITOR_EVENT_CHANGED;
  if (flags & NOTE_RENAME)
    result |= G_FILE_MONITOR_EVENT_MOVED;

  return result;
}


/**
 * process_kqueue_notifications:
 * @gioc: unused.
 * @cond: unused.
 * @data: unused.
 *
 * Processes notifications, coming from the kqueue thread.
 *
 * Reads notifications from the command file descriptor, emits the
 * "changed" event on the appropriate monitor.
 *
 * A typical GIO Channel callback function.
 *
 * Returns: %TRUE
 **/
static gboolean
process_kqueue_notifications (GIOChannel   *gioc,
                              GIOCondition  cond,
                              gpointer      data)
{
  struct kqueue_notification n;
  kqueue_sub *sub = NULL;
  GFileMonitor *monitor = NULL;
  GFile *child = NULL;
  GFile *other = NULL;
  GFileMonitorEvent mask = 0;
  
  g_assert (kqueue_socket_pair[0] != -1);
  if (!_ku_read (kqueue_socket_pair[0], &n, sizeof (struct kqueue_notification)))
    {
      KH_W ("Failed to read a kqueue notification, error %d", errno);
      return TRUE;
    }

  sub = (kqueue_sub *) g_hash_table_lookup (subs_hash_table, GINT_TO_POINTER (n.fd));
  g_assert (sub != NULL);

  monitor = G_FILE_MONITOR (sub->user_data);
  g_assert (monitor != NULL);

  child = g_file_new_for_path (sub->filename);
  other = NULL; /* No pair moves, always NULL */

  if (n.flags & (NOTE_DELETE | NOTE_REVOKE))
    {
      _km_add_missing (sub);
      _kh_cancel_sub (sub);
    }
  mask = convert_kqueue_events_to_gio (n.flags);

  g_file_monitor_emit_event (monitor, child, other, mask);
  return TRUE;
}


/**
 * _kh_startup_impl:
 * @unused: unused
 *
 * Kqueue backend startup code. Should be called only once.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
static gpointer
_kh_startup_impl (gpointer unused)
{
  GIOChannel *channel = NULL;
  gboolean result = FALSE;

  kqueue_descriptor = kqueue ();
  result = (kqueue_descriptor != -1);
  if (!result)
    {
      KH_W ("Failed to initialize kqueue\n!");
      return GINT_TO_POINTER (FALSE);
    }

  result = socketpair (AF_UNIX, SOCK_STREAM, 0, kqueue_socket_pair);
  if (result != 0)
    {
      KH_W ("Failed to create socket pair\n!");
      return GINT_TO_POINTER (FALSE) ;
    }

  result = pthread_create (&kqueue_thread,
                           NULL,
                           _kqueue_thread_func,
                           &kqueue_socket_pair[1]);
  if (result != 0)
    {
      KH_W ("Failed to run kqueue thread\n!");
      return GINT_TO_POINTER (FALSE);
    }

  _km_init (_kh_file_appeared_cb);

  channel = g_io_channel_unix_new (kqueue_socket_pair[0]);
  g_io_add_watch (channel, G_IO_IN, process_kqueue_notifications, NULL);

  subs_hash_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  KH_W ("started gio kqueue backend\n");
  return GINT_TO_POINTER (TRUE);
}


/**
 * _kh_startup:
 * Kqueue backend initialization.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
_kh_startup (void)
{
  static GOnce init_once = G_ONCE_INIT;
  g_once (&init_once, _kh_startup_impl, NULL);
  return GPOINTER_TO_INT (init_once.retval);
}


/**
 * _kh_start_watching:
 * @sub: a #kqueue_sub
 *
 * Starts watching on a subscription.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
_kh_start_watching (kqueue_sub *sub)
{
  g_assert (kqueue_socket_pair[0] != -1);
  g_assert (sub != NULL);
  g_assert (sub->filename != NULL);

  /* kqueue requires a file descriptor to monitor. Sad but true */
  sub->fd = open (sub->filename, O_RDONLY);

  if (sub->fd == -1)
    {
      KH_W ("failed to open file %s (error %d)", sub->filename, errno);
      return FALSE;
    }

  G_LOCK (hash_lock);
  g_hash_table_insert (subs_hash_table, GINT_TO_POINTER (sub->fd), sub);
  G_UNLOCK (hash_lock);

  _kqueue_thread_push_fd (sub->fd);
  
  /* Bump the kqueue thread. It will pick up a new sub entry to monitor */
  if (!_ku_write (kqueue_socket_pair[0], "A", 1))
    KH_W ("Failed to bump the kqueue thread (add fd, error %d)", errno);
  return TRUE;
}


/**
 * _kh_add_sub:
 * @sub: a #kqueue_sub
 *
 * Adds a subscription for monitoring.
 *
 * This funciton tries to start watching a subscription with
 * _kh_start_watching(). On failure, i.e. when a file does not exist yet,
 * the subscription will be added to a list of missing files to continue
 * watching when the file will appear.
 *
 * Returns: %TRUE
 **/
gboolean
_kh_add_sub (kqueue_sub *sub)
{
  g_assert (sub != NULL);

  if (!_kh_start_watching (sub))
    _km_add_missing (sub);

  return TRUE;
}


/**
 * _kh_cancel_sub:
 * @sub a #kqueue_sub
 *
 * Stops monitoring on a subscription.
 *
 * Returns: %TRUE
 **/
gboolean
_kh_cancel_sub (kqueue_sub *sub)
{
  gboolean missing = FALSE;
  g_assert (kqueue_socket_pair[0] != -1);
  g_assert (sub != NULL);

  G_LOCK (hash_lock);
  missing = !g_hash_table_remove (subs_hash_table, GINT_TO_POINTER (sub->fd));
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
      if (!_ku_write (kqueue_socket_pair[0], "R", 1))
        KH_W ("Failed to bump the kqueue thread (remove fd, error %d)", errno);
    }

  return TRUE;
}


/**
 * _kh_file_appeared_cb:
 * @sub: a #kqueue_sub
 *
 * A callback function for kqueue-missing subsystem.
 *
 * Signals that a missing file has finally appeared in the filesystem.
 * Emits %G_FILE_MONITOR_EVENT_CREATED.
 **/
void
_kh_file_appeared_cb (kqueue_sub *sub)
{
  GFile* child;

  g_assert (sub != NULL);
  g_assert (sub->filename);

  if (!g_file_test (sub->filename, G_FILE_TEST_EXISTS))
    return;

  child = g_file_new_for_path (sub->filename);

  g_file_monitor_emit_event (G_FILE_MONITOR (sub->user_data),
                             child,
                             NULL,
                             G_FILE_MONITOR_EVENT_CREATED);

  g_object_unref (child);
}
