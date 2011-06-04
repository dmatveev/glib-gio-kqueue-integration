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

static gboolean kh_debug_enabled = TRUE;
#define KH_W if (kh_debug_enabled) g_warning

G_GNUC_INTERNAL G_LOCK_DEFINE (kqueue_lock);

static GList *g_pick_up_fds = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (pick_up_lock);

static int g_kqueue = -1;
static int g_sockpair[] = {-1, -1};
static pthread_t g_kqueue_thread;


void*
g_kqueue_thread_func (void *arg)
{
  int fd;
  struct kevent waiting;

  fd = *(int *) arg;

  if (g_kqueue == -1)
  {
    KH_W ("fatal: kqueue is not initialized!\n");
    return NULL;
  }

  EV_SET (&waiting,
          fd,
          EVFILT_READ,
          EV_ADD | EV_ENABLE | EV_ONESHOT,
          NOTE_LOWAT,
          1,
          0);

  for (;;) {
    int ret;
    struct kevent got;

    ret = kevent (g_kqueue, &waiting, 1, &got, 1, NULL);

    if (ret == -1) {
      KH_W ("kevent failed\n");
      continue;
    }

    { char c;
      read (fd, &c, 1);
      if (c == 'A') {
        G_LOCK (pick_up_lock);
        if (g_pick_up_fds)
          {
            GList *head = g_pick_up_fds;
            while (head)
              {
                head = head->next;
              }
            g_list_free (g_pick_up_fds);
            g_pick_up_fds = NULL;
          }
        G_UNLOCK (pick_up_lock);
      }
    }
  }

  return NULL;
}


static gboolean
process_kqueue_notifications (GIOChannel  *gioc,
                              GIOCondition cond,
                              gpointer     data)
{
  return FALSE;
}



gboolean
_kh_startup (void)
{
  static gboolean initialized = FALSE;
  static gboolean result = FALSE;

  GIOChannel *channel;
  GSource *source;

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
                                 g_kqueue_thread_func,
                                 &g_sockpair[1]));
  if (!result)
    {
      G_UNLOCK (kqueue_lock);
      KH_W ("Failed to run kqueue thread\n!");
      return FALSE;
    }

  /* TODO: Non-blocking options for sockets? */
  channel = g_io_channel_unix_new (g_sockpair[0]);
  source = g_io_create_watch (channel, G_IO_IN);
  g_source_set_callback (source,
                         (GSourceFunc) process_kqueue_notifications,
                         NULL,
                         NULL);

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

  G_LOCK (pick_up_lock);
  g_pick_up_fds = g_list_prepend (g_pick_up_fds, GINT_TO_POINTER (sub->fd));
  G_UNLOCK (pick_up_lock);

  /* Bump the kqueue thread. It will pick up a new sub entry */
  write(g_sockpair[0], "A", 1);
  return TRUE;
}
