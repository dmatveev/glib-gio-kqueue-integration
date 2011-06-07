#ifndef __KQUEUE_HELPER_H
#define __KQUEUE_HELPER_H

#include "kqueue-sub.h"

gboolean _kh_startup    (void);
gboolean _kh_add_sub    (kqueue_sub *sub);
gboolean _kh_cancel_sub (kqueue_sub *sub);

#endif /* __KQUEUE_HELPER_H */
