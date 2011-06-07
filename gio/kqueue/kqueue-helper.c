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

static gboolean kh_debug_enabled = TRUE;
#define KH_W if (kh_debug_enabled) g_warning

G_GNUC_INTERNAL G_LOCK_DEFINE (kqueue_lock);

static GHashTable *g_sub_hash = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (hash_lock);

/* TODO: Too many locks. Isn't it? */

int g_kqueue = -1;
static int g_sockpair[] = {-1, -1};
static pthread_t g_kqueue_thread;


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
  /* TODO: The following notifications should be emulated:
   *   G_FILE_MONITOR_EVENT_CREATED
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

  if (n.flags & (NOTE_LINK | NOTE_REVOKE))
    {
      return TRUE; /* TODO: GIO does not know about it */
    }

  sub = (kqueue_sub *) g_hash_table_lookup (g_sub_hash, GINT_TO_POINTER (n.fd));
  g_assert (sub != NULL);

  monitor = G_FILE_MONITOR(sub->user_data);
  g_assert (monitor != NULL);

  child = g_file_new_for_path (sub->filename);
  other = NULL; /* TODO: Do something. */
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

  /* TODO: This logic has been taken from the original inotify plugin.
   * I assume that it would be faster to check the `initialized' flag
   * before locking the `kqueue_lock' (the double-checked locking
   * pattern). */
  G_LOCK (kqueue_lock);

  if (initialized == TRUE)
    {
      G_UNLOCK (kqueue_lock);
      return result;
    }

   /* TODO: Need something cuter here?
    * inotify backend is decomposed into modules like inotify-path,
    * inotify-lernel and so on. For now I will do all the init9n stuff
    * right here. */
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

  /* TODO: Non-blocking options for sockets? */
  channel = g_io_channel_unix_new (g_sockpair[0]);
  g_io_add_watch (channel, G_IO_IN, process_kqueue_notifications, NULL);

  g_sub_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  KH_W ("started gio kqueue backend\n");
  initialized = TRUE;

  G_UNLOCK (kqueue_lock);

  return TRUE;
}


gboolean
_kh_add_sub (kqueue_sub *sub)
{
  g_assert (g_sockpair[0] != -1);
  g_assert (sub != NULL);
  g_assert (sub->filename != NULL);

  /* kqueue requires a file descriptor to monitor. Sad but true */
  sub->fd = open (sub->filename, O_RDONLY);

  if (sub->fd == -1)
    {
      /* TODO: Implement monitoring for missing files that can appear
       * later, as in the inotify backend */
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
_kh_cancel_sub (kqueue_sub *sub)
{
  g_assert (g_sockpair[0] != -1);
  g_assert (sub != NULL);

  G_LOCK (hash_lock);
  g_hash_table_remove (g_sub_hash, GINT_TO_POINTER(sub->fd));
  G_UNLOCK (hash_lock);

  /* fd will be closed in the kqueue thread */
  _kqueue_thread_remove_fd (sub->fd);

  /* Bump the kqueue thread. It will pick up a new sub entry to remove*/
  write(g_sockpair[0], "R", 1);
  return TRUE;
}
