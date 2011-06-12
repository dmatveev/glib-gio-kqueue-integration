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

#include <glib.h>

#include "kqueue-sub.h"

static gboolean is_debug_enabled = FALSE;
#define KS_W if (is_debug_enabled) g_warning

kqueue_sub*
_kh_sub_new (const gchar *filename,
             gboolean     pair_moves,
             gpointer     user_data)
{
  kqueue_sub *sub = NULL;
  
  sub = g_new0 (kqueue_sub, 1);
  sub->filename = g_strdup (filename);
  sub->pair_moves = pair_moves;
  sub->user_data = user_data;
  sub->fd = -1;

  KS_W ("new subscription for %s being setup\n", sub->filename);
  
  return sub;
}

void
_kh_sub_free (kqueue_sub *sub)
{
  g_free (sub->filename);
  g_free (sub);
}
