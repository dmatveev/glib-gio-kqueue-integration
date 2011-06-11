#ifndef __G_KQUEUE_MISSING_H
#define __G_KQUEUE_MISSING_H

typedef void (*on_create_cb) (kqueue_sub *);

void _km_init        (on_create_cb cb);
void _km_add_missing (kqueue_sub *sub);
void _km_remove      (kqueue_sub *sub);

#endif /* __G_KQUEUE_MISSING_H */
