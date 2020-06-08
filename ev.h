#ifndef _EV_H_
#define _EV_H_

struct eventLoop;

typedef void(*ev_callback)(int fd, const void *);

struct eventLoop *ev_create(void);
void ev_loop(struct eventLoop *ev);
void ev_start(struct eventLoop *ev);
void ev_stop(struct eventLoop *ev);

int ev_addEvent(struct eventLoop *ev, ev_callback cb, int fd, const void *argv);
int ev_addTimer(struct eventLoop *ev, ev_callback cb, unsigned int firstMsec, unsigned int cycleMsec, const void *argv);
int ev_addAlarmTimer(struct eventLoop *ev, ev_callback cb, unsigned int firstMsec, unsigned int cycleMsec, const void *argv);
#endif
