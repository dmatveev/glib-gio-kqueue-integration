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
#include <glib.h>

#include "kqueue-thread.h"
#include "kqueue-sub.h"
#include "kqueue-utils.h"

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


/**
 * Pick up new file descriptors for monitoring.
 *
 * This function will pick up file descriptors from a global list
 * for monitoring. The list will be freed then.
 *
 * To add new items to the list, use _kqueue_thread_push_fd().
 *
 * @param A list of events to monitor. Will be extended with
 *        new items.
 */
static void
_kqueue_thread_collect_fds (kevents *events)
{
  g_assert (events != NULL);

  G_LOCK (pick_up_lock);
  if (g_pick_up_fds)
    {
      GSList *head = g_pick_up_fds;
      guint count = g_slist_length (g_pick_up_fds);
      kevents_extend_sz (events, count);
      while (head)
      {
        struct kevent *pevent = &events->memory[events->kq_size++];
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


/**
 * Remove file descriptors from monitoring.
 *
 * This function will pick up file descriptors from a global list
 * to cancel monitoring on them. The list will be freed then.
 *
 * To add new items to the list, use _kqueue_thread_remove_fd().
 *
 * @param A list of events to monitor. Cancelled subscriptions will be
 *        removed from it, and its size probably will be reduced.
 */
static void
_kqueue_thread_cleanup_fds (kevents *events)
{
  g_assert (events != NULL);

  G_LOCK (remove_lock);
  if (g_remove_fds)
    {
      size_t oldsize = events->kq_size;
      size_t newsize = oldsize - g_slist_length (g_remove_fds);
      int i, j;

      if (newsize < 1)
          newsize = 1;

      for (i = 1, j = 1; i < oldsize; i++)
        {
          int fd = events->memory[i].ident;
          GSList *elem = g_slist_find (g_remove_fds, GINT_TO_POINTER (fd));
          if (elem == NULL)
            {
              if (j != i)
                  events->memory[j++] = events->memory[i];
            }
          else
              close (fd);
        }
      events->kq_size = j;
      kevents_reduce (events);
      g_slist_free (g_remove_fds);
      g_remove_fds = NULL;
    }
  G_UNLOCK (remove_lock);
}


/**
 * The core kqueue monitoring routine.
 *
 * The thread communicates with the outside world through a so-called
 * command file descriptor. The thread reads control commands from it
 * and writes the notifications into it.
 *
 * Control commands are single-byte characters:
 *   'A' - pick up new file descriptors to monitor
 *   'R' - remove some descriptors from monitoring.
 * For details, @see _kqueue_thread_collect_fds() and
 * _kqueue_thread_cleanup_fds().
 *
 * Notifications, that thread writes into the command file descriptor,
 * are represented with \struct kqueue_notification objects.
 *
 * @param A pointer to int, the command file descriptor.
 * @returns NULL.
 */
void*
_kqueue_thread_func (void *arg)
{
  int fd;
  kevents waiting;
  kevents_init_sz (&waiting, 1);

  fd = *(int *) arg;

  if (g_kqueue == -1)
  {
    KT_W ("fatal: kqueue is not initialized!\n");
    return NULL;
  }

  EV_SET (&waiting.memory[0],
          fd,
          EVFILT_READ,
          EV_ADD | EV_ENABLE | EV_ONESHOT,
          NOTE_LOWAT,
          1,
          0);
  waiting.kq_size = 1;

  for (;;) {
    /* TODO: Provide more items in the `eventlist' to kqueue(2).
     * Currently the backend takes notifications from the kernel one
     * by one, i.e. there will be a lot of system calls and context
     * switches when the application will monitor a lot of files with
     * high filesystem activity on each. */
     
    struct kevent received;
    int ret = kevent (g_kqueue, waiting.memory, waiting.kq_size, &received, 1, NULL);

    if (ret == -1) {
      KT_W ("kevent failed\n");
      continue;
    }

    if (received.ident == fd)
      {
        char c;
        read (fd, &c, 1);
        if (c == 'A')
            _kqueue_thread_collect_fds (&waiting);
        else if (c == 'R')
            _kqueue_thread_cleanup_fds (&waiting);
      }
    else if (!(received.fflags & EV_ERROR))
      {
        struct kqueue_notification kn;
        kn.fd = received.ident;
        kn.flags = received.fflags;

        write (fd, &kn, sizeof (struct kqueue_notification));
      }
  }
  kevents_free (&waiting);
  return NULL;
}


/**
 * Put a new file descriptor into the pick up list for monitroing.
 *
 * The kqueue thread will not start monitoring on it immediately, it
 * should be bumped via its command file descriptor manually.
 * @see kqueue_thread() and _kqueue_thread_collect_fds() for details.
 *
 * @param A file descriptor to put.
 */
void
_kqueue_thread_push_fd (int fd)
{
  G_LOCK (pick_up_lock);
  g_pick_up_fds = g_slist_prepend (g_pick_up_fds, GINT_TO_POINTER (fd));
  G_UNLOCK (pick_up_lock);
}


/**
 * Put a new file descriptor into the remove list to cancel monitoring
 * on it.
 *
 * The kqueue thread will not stop monitoring on it immediately, it
 * should be bumped via its command file descriptor manually.
 * @see kqueue_thread() and _kqueue_thread_collect_fds() for details.
 *
 * @param A file descriptor to remove.
 */
void
_kqueue_thread_remove_fd (int fd)
{
  G_LOCK (remove_lock);
  g_remove_fds = g_slist_prepend (g_remove_fds, GINT_TO_POINTER (fd));
  G_UNLOCK (remove_lock);
}
