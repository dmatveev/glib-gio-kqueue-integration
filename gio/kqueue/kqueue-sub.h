#ifndef __KQUEUE_SUB_H
#define __KQUEUE_SUB_H

typedef struct
{
	gchar*   filename;
	gboolean cancelled;
	gpointer user_data;
  gboolean pair_moves;
  int      fd;
} kqueue_sub;

kqueue_sub* _kh_sub_new  (const gchar* filename, gboolean pair_moves, gpointer user_data);
void        _kh_sub_free (kqueue_sub* sub);

#endif /* __KQUEUE_SUB_H */
