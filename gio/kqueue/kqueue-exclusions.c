/*******************************************************************************
  Copyright (c) 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>

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

#include <string.h>
#include <glib.h>
#include "kqueue-exclusions.h"

static gboolean ke_debug_enabled = TRUE;
#define KE_W if (ke_debug_enabled) g_warning

static GSList *exclude_list = NULL;
G_GNUC_INTERNAL G_LOCK_DEFINE (exclude_list_lock);

#define CFG_FILE "gio-kqueue.conf"

static void
_ke_free (GSList *list)
{
  g_assert (list != NULL);
  g_slist_free_full (list, g_free);
}

static gchar*
ke_system_config ()
{
  return strdup ("/etc/" CFG_FILE);
}

static gchar*
ke_local_config ()
{
  return g_build_filename (g_get_user_config_dir (), CFG_FILE, NULL); 
}

static void _ke_fill (GSList **list, const gchar *cfg_path)
{
  g_assert (list != NULL);
  g_assert (cfg_path != NULL);

  GIOChannel *ch = NULL;
  gchar *line = NULL;
  gsize len = 0;
  gsize term = 0;
  GIOStatus st = G_IO_STATUS_NORMAL;
 
  ch = g_io_channel_new_file (cfg_path, "r", NULL);

  if (ch == NULL) 
    {
      KE_W ("Failed to open config file %s\n", cfg_path);
      return;
    }

  do
    {
      st = g_io_channel_read_line (ch, &line, &len, &term, NULL);
      if (line != NULL && len > 0)
        {
          line[term] = '\0';
          *list = g_slist_prepend (*list, line);
        }
    }
  while (st != G_IO_STATUS_EOF);
}

void
_ke_rebuild ()
{
  G_LOCK (exclude_list_lock);

  if (exclude_list != NULL)
    _ke_free (exclude_list);

  gchar *local_cfg_path = ke_local_config ();
  gchar *system_cfg_path = ke_system_config ();

  _ke_fill (&exclude_list, local_cfg_path );
  _ke_fill (&exclude_list, system_cfg_path );

  g_free (local_cfg_path);
  g_free (system_cfg_path); 

  G_UNLOCK (exclude_list_lock);
}

gboolean
_ke_is_excluded (const char *full_path)
{
  gboolean retval = FALSE;
  GSList *head = NULL; 
  
  G_LOCK (exclude_list_lock);

  head = exclude_list;

  while (head != NULL)
    {
      if (head->data != NULL && g_str_has_prefix (full_path, head->data))
        {
          retval = TRUE;
          break;
        }
      head = g_slist_next (head);
    }

  G_UNLOCK (exclude_list_lock);

  return retval;
}
