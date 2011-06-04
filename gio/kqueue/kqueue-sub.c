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
