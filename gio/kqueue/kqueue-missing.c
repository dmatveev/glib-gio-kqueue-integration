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

#include "kqueue-helper.h"
#include "kqueue-sub.h"
#include "kqueue-missing.h"


#define SCAN_MISSING_TIME 4 /* 1/4 Hz */

static gboolean km_scan_missing (gpointer user_data);

static gboolean km_debug_enabled = TRUE;
#define KM_W if (km_debug_enabled) g_warning

static GSList *g_missing_subs = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (missing_lock);

static gboolean scan_missing_running = FALSE;
static on_create_cb g_cb;

void
_km_init (on_create_cb cb)
{
  g_cb = cb;
}


void
_km_add_missing (kqueue_sub *sub)
{
  G_LOCK (missing_lock);
  if (g_slist_find (g_missing_subs, sub))
    {
      KM_W ("asked to add %s to missing list but it's already on the list!\n", sub->filename);
      return;
    }

  KM_W ("adding %s to missing list\n", sub->filename);
  g_missing_subs = g_slist_prepend (g_missing_subs, sub);
  G_UNLOCK (missing_lock);

  if (!scan_missing_running)
    {
      scan_missing_running = TRUE;
      g_timeout_add_seconds (SCAN_MISSING_TIME, km_scan_missing, NULL);
    }

  return TRUE;
}


static gboolean
km_scan_missing (gpointer user_data)
{
  GSList *head;
  GSList *not_missing = NULL;
  gboolean retval = FALSE;
  
  G_LOCK (missing_lock);

  if (g_missing_subs)
    KM_W ("we have a job");

  for (head = g_missing_subs; head; head = head->next)
    {
      kqueue_sub *sub = (kqueue_sub *) head->data;
      g_assert (sub != NULL);
      g_assert (sub->filename != NULL);

      if (_kh_start_watching (sub))
        {
          KM_W ("file %s now exists, starting watching", sub->filename);
          if (g_cb)
            g_cb (sub);
          not_missing = g_slist_prepend (not_missing, head);
        }
    }

  for (head = not_missing; head; head = head->next)
    {
      GSList *link = (GSList *) head->data;
      g_missing_subs = g_slist_remove_link (g_missing_subs, link);
    }
  g_slist_free (not_missing);

  if (g_missing_subs == NULL)
    {
      scan_missing_running = FALSE;
      retval = FALSE;
    }
  else
    retval = TRUE;

  G_UNLOCK (missing_lock);
  return retval;
}


void
_km_remove (kqueue_sub *sub)
{
  G_LOCK (missing_lock);
  g_missing_subs = g_slist_remove (g_missing_subs, sub);
  G_UNLOCK (missing_lock);
}
