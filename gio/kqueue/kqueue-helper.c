#include "config.h"
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <gio/glocalfile.h>
#include <gio/gfilemonitor.h>
#include <gio/gfile.h>
#include <pthread.h>
#include "kqueue-helper.h"

static gboolean kh_debug_enabled = FALSE;
#define KH_W if (kh_debug_enabled) g_warning

G_GNUC_INTERNAL G_LOCK_DEFINE (kqueue_lock);

static int g_kqueue = -1;
static int g_sockpair[2];
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

    g_printf ("Got a command!\n");
  }
}

gboolean
_kh_startup (void)
{
  static gboolean initialized = FALSE;
  static gboolean result = FALSE;

  /* TODO: This logic has been taken from the original inotify plugin.
   * I assume that it would be faster to check the `initialized' flag
   * before locking the `kqueue_lock' (the double-checked locking
   * pattern)
   */
  G_LOCK (kqueue_lock);

  if (initialized == TRUE)
    {
      G_UNLOCK (kqueue_lock);
      return result;
    }

   /* TODO: Need something cuter here?
    * inotify backend is decomposed into modules like inotify-path,
    * inotify-lernel and so on. For now I will do all the init9n stuff
    * right here.
    */
  g_kqueue = kqueue();
  result = (g_kqueue != -1);
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

  KH_W ("started gio kqueue backend\n");

  initialized = TRUE;

  G_UNLOCK (kqueue_lock);

  return TRUE;
}
