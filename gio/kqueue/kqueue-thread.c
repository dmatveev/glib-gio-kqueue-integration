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
#include <unistd.h>
#include <stdlib.h> /* TODO: {c,re}alloc. Probably should use smt from Glib instead? */
#include <glib.h>

#include "kqueue-thread.h"
#include "kqueue-sub.h"


static gboolean kt_debug_enabled = TRUE;
#define KT_W if (kt_debug_enabled) g_warning

static GSList *g_pick_up_fds = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (pick_up_lock);

static GSList *g_remove_fds = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (remove_lock);


/* GIO does not have analogues for NOTE_LINK and(?) NOTE_REVOKE, so
 * we do not ask kqueue() to watch for these events for now. */

const uint32_t KQUEUE_VNODE_FLAGS =
  NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME;


/* TODO: Probably it would be better to pass it as a thread param? */
extern int g_kqueue;


static void
_kqueue_thread_collect_fds (struct kevent **events, size_t *kq_size)
{
  g_assert (events != NULL);
  g_assert (*events != NULL);
  g_assert (kq_size != NULL);

  G_LOCK (pick_up_lock);
  if (g_pick_up_fds)
    {
      GSList *head = g_pick_up_fds;
      guint count = g_slist_length (g_pick_up_fds);
      *events = realloc (*events, (*kq_size + count) * sizeof (struct kevent));
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


static void
_kqueue_thread_cleanup_fds (struct kevent **events, size_t *kq_size)
{
  g_assert (events != NULL);
  g_assert (*events != NULL);
  g_assert (kq_size != NULL);

  G_LOCK (remove_lock);
  if (g_remove_fds)
    {
      /* kevent(2) expects a continuous piece of memory passed as
       * `eventlist' argument. So, I do not see any other solution
       * than just to reallocate and filter out the existing kevents.
       * Yes, it is slow. */

      guint count = g_slist_length (g_remove_fds);
      struct kevent *kold = *events;
      size_t oldsize = *kq_size;
      size_t newsize = oldsize - count;
      int i, j;

      if (newsize < 1)
        {
          newsize = 1;
        }

      *events = calloc (newsize, sizeof (struct kevent));
      *events[0] = kold[0];

      for (i = 1, j = 1; i < oldsize; i++)
        {
          GSList *elem = g_slist_find (g_remove_fds, GINT_TO_POINTER (kold[i].ident));
          if (elem == NULL)
            {
              /* TODO: Here we are copying a complete structure contents.
               * On x86 system, a sizeof(struct kqueue) should be 28 bytes long.
               * Probably it would be better to use a memory pool and copy just
               * pointers here, not an actual data. */
              *events[j++] = kold[i];
            }
          else
            {
              close (kold[i].ident);
            }
        }
      g_slist_free (g_remove_fds);
      free (kold);
      g_remove_fds = NULL;
      *kq_size = newsize;
    }
  G_UNLOCK (remove_lock);
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
            _kqueue_thread_collect_fds (&waiting, &kq_size);
          }
        else if (c == 'R')
          {
            _kqueue_thread_cleanup_fds (&waiting, &kq_size);
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


void
_kqueue_thread_remove_fd (int fd)
{
  G_LOCK (remove_lock);
  g_remove_fds = g_slist_prepend (g_remove_fds, GINT_TO_POINTER (fd));
  G_UNLOCK (remove_lock);
}
