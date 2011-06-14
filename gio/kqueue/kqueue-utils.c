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

#include <sys/event.h>
#include <string.h>
#include <glib.h>
#include "kqueue-utils.h"

#define KEVENTS_EXTEND_COUNT 10


/**
 * Initialize a kevents object.
 *
 * @param a pointer to kevents object.
 * @param the initial preallocated memory size. If it is less than
 *        KEVENTS_EXTEND_COUNT, this value will be used instead.
 */
void
kevents_init_sz (kevents *kv, gsize n_initial)
{
  g_assert (kv != NULL);

  memset (kv, 0, sizeof (kevents));

  if (n_initial < KEVENTS_EXTEND_COUNT)
    n_initial = KEVENTS_EXTEND_COUNT;

  kv->memory = g_new0 (struct kevent, n_initial);
  kv->kq_allocated = n_initial;
}


/**
 * Extend the allocated memory, if needed.
 *
 * @param a pointer to kevents object.
 * @param a number of new objects to be added.
 */
void
kevents_extend_sz (kevents *kv, gsize n_new)
{
  g_assert (kv != NULL);

  if (kv->kq_size + n_new <= kv->kq_allocated)
    return;

  kv->kq_allocated += (n_new + KEVENTS_EXTEND_COUNT);
  kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
}


/**
 * Reduce the allocated heap size, if needed.
 *
 * If the allocated heap size is >= 3*used
 * and 2*used >= KEVENTS_EXTEND_COUNT, reduce it to 2*used.
 *
 * @param a pointer to kevents object.
 */
void
kevents_reduce (kevents *kv)
{
  g_assert (kv != NULL);
  gsize candidate_sz;

  if (kv->kq_size == 0 || kv->kq_allocated == 0 || kv->memory == NULL)
    return;

  candidate_sz = 2 * kv->kq_size;

  if (((double) kv->kq_allocated / kv->kq_size) >= 3 &&
      candidate_sz >= KEVENTS_EXTEND_COUNT)
    {
      kv->kq_allocated = candidate_sz;
      kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
    }
}


/**
 * Reset the kevents object and free all the associated memory.
 *
 * @param a pointer to kevents object.
 */
void
kevents_free (kevents *kv)
{
  g_assert (kv != NULL);

  g_free (kv->memory);
  memset (kv, 0, sizeof (kevents));
}
