#include "config.h"
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h> /* TODO: {c,re}alloc. Probably should use smt from Glib instead? */
#include <glib.h>

#include "kqueue-thread.h"
#include "kqueue-sub.h"


static gboolean kt_debug_enabled = TRUE;
#define KT_W if (kt_debug_enabled) g_warning

static GSList *g_pick_up_fds = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (pick_up_lock);

const uint32_t KQUEUE_VNODE_FLAGS =
  NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK |
  NOTE_RENAME | NOTE_REVOKE;

/* TODO: Probably it would be better to pass it as a thread param? */
extern int g_kqueue;


static void
_kqueue_thread_update_fds (struct kevent **events, size_t *kq_size)
{
  g_assert (events != NULL);
  g_assert (*events != NULL);
  g_assert (kq_size != NULL);

  G_LOCK (pick_up_lock);
  if (g_pick_up_fds)
    {
      GSList *head = g_pick_up_fds;
      guint count = g_slist_length (g_pick_up_fds);
      *events = realloc (*events, count * sizeof (struct kevent));
      while (head)
      {
        struct kevent *pevent = &(*events)[(*kq_size)++];
        EV_SET (pevent,
                GPOINTER_TO_INT (head->data),
                EVFILT_VNODE,
                EV_ADD | EV_ENABLE | EV_ONESHOT,
                KQUEUE_VNODE_FLAGS,
                0,
                0);
        head = head->next;
      }
      g_slist_free (g_pick_up_fds);
      g_pick_up_fds = NULL;
    }
  G_UNLOCK (pick_up_lock);
}


void*
_kqueue_thread_func (void *arg)
{
  int fd;
  struct kevent *waiting;
  size_t kq_size = 1;

  /* TODO: A better memory allocation strategy */
  waiting = calloc (1, sizeof (struct kevent));

  fd = *(int *) arg;

  if (g_kqueue == -1)
  {
    KT_W ("fatal: kqueue is not initialized!\n");
    return NULL;
  }

  EV_SET (&waiting[0],
          fd,
          EVFILT_READ,
          EV_ADD | EV_ENABLE | EV_ONESHOT,
          NOTE_LOWAT,
          1,
          0);

  for (;;) {
    /* TODO: Provide more items in the `eventlist' to kqueue(2).
     * Currently the backend takes notifications from the kernel one
     * by one, i.e. there will be a lot of system calls and context
     * switches when the application will monitor a lot of files with
     * high filesystem activity on each. */
     
    struct kevent received;
    int ret = kevent (g_kqueue, waiting, kq_size, &received, 1, NULL);

    if (ret == -1) {
      KT_W ("kevent failed\n");
      continue;
    }

    if (received.ident == fd)
      {
        char c;
        read (fd, &c, 1);
        if (c == 'A')
          {
            _kqueue_thread_update_fds (&waiting, &kq_size);
          }
      }
    else if (!(received.fflags & EV_ERROR))
      {
        struct kqueue_notification kn;
        kn.fd = received.ident;
        kn.flags = received.fflags;

        write (fd, &kn, sizeof (struct kqueue_notification));
      }
  }

  return NULL;
}


void
_kqueue_thread_push_fd (int fd)
{
  G_LOCK (pick_up_lock);
  g_pick_up_fds = g_slist_prepend (g_pick_up_fds, GINT_TO_POINTER (fd));
  G_UNLOCK (pick_up_lock);
}
