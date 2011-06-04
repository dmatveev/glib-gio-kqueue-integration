#ifndef __KQUEUE_THREAD_H
#define __KQUEUE_THREAD_H


/* TODO: Field comments */
struct kqueue_notification {
  int fd;
  uint32_t flags;
};


void* _kqueue_thread_func     (void *arg);
void  _kqueue_thread_push_fd  (int fd);

#endif /* __KQUEUE_SUB_H */
