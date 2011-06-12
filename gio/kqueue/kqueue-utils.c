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

void
kevents_init (kevents *kv)
{
  g_assert (kv != NULL);
  memset (kv, 0, sizeof (kevents));
}


void
kevents_init_sz (kevents *kv, gsize n_initial)
{
  g_assert (kv != NULL);
  g_assert (n_initial > 0);

  kevents_init (kv);
  kv->memory = g_new0 (struct kevent, n_initial);
  kv->kq_allocated = n_initial;
}


void
kevents_extend (kevents *kv)
{
  g_assert (kv != NULL);

  if (kv->kq_allocated == 0)
    {
      kv->memory = g_new0 (struct kevent, KEVENTS_EXTEND_COUNT);
      return;
    }
    
  if (kv->kq_size == kv->kq_allocated)
    {
      kv->kq_allocated += KEVENTS_EXTEND_COUNT;
      kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
    }
}


void
kevents_extend_sz (kevents *kv, gsize n_new)
{
  g_assert (kv != NULL);

  if (kv->kq_size + n_new <= kv->kq_allocated)
    {
      return;
    }

  kv->kq_allocated += (n_new + KEVENTS_EXTEND_COUNT);
  kv->memory = g_renew (struct kevent, kv->memory, kv->kq_allocated);
}


void
kevents_free (kevents *kv)
{
  g_assert (kv != NULL);

  g_free (kv->memory);
  memset (kv, 0, sizeof (kevents));
}


void
kevents_swap_by (kevents *dst, const kevents *src)
{
  g_assert (dst != NULL);
  g_assert (src != NULL);

  kevents_free (dst);
  *dst = *src;
}
