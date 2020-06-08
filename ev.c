#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>

#include "ev.h"

#define EV_MAX_FD 1024
#define EV_WAIT_CNT 64

typedef enum
{
    EV_TYPE_NONE,
    EV_TYPE_TIMER,
    EV_TYPE_SOCKET,
} evType;

typedef struct
{
    int fd;
    uint32_t events;
    evType enType;
    ev_callback evCallback;
    const void *argv;
} evContext;

struct eventLoop
{
    int maxfd;
    int epfd;
    int size;
    evContext *eventList;
    int stop;
};

static void ltEvnetHandler(struct eventLoop *ev, int fd)
{
    evContext *_context = &ev->eventList[fd];
    uint64_t data;

    if (_context->events != EPOLLIN)
    {
        return;
    }

    switch (_context->enType)
    {
    case EV_TYPE_TIMER:
        read(fd, &data, sizeof(uint64_t));

        if (_context->evCallback != NULL)
        {
            _context->evCallback(fd, _context->argv);
        }

        break;

    case EV_TYPE_SOCKET:
        if (_context->evCallback != NULL)
        {
            _context->evCallback(fd, _context->argv);
        }

        break;

    default:
        break;
    }
}

static int addEvent(struct eventLoop *ev, evContext *context)
{
    struct epoll_event _event;

    _event.events = context->events;
    _event.data.fd = context->fd;

    return epoll_ctl(ev->epfd, EPOLL_CTL_ADD, _event.data.fd, &_event);
}

int ev_addTimer(struct eventLoop *ev, ev_callback cb, unsigned int firstMsec, unsigned int cycleMsec, const void *argv)
{
    int fd;
    struct itimerspec timeValue;
    evContext *_context;

    if ((fd = timerfd_create(CLOCK_REALTIME, 0)) < 0 || (fd >= EV_MAX_FD))
    {
        return -1;
    }

    timeValue.it_value.tv_sec = (time_t)firstMsec / 1000;
    timeValue.it_value.tv_nsec = (long)(firstMsec % 1000) * 1000000;

    timeValue.it_interval.tv_sec = (time_t)cycleMsec / 1000;
    timeValue.it_interval.tv_nsec = (long)(cycleMsec % 1000) * 1000000;

    if (timerfd_settime(fd, 0, &timeValue, NULL) < 0)
    {
        return -1;
    }

    _context = &ev->eventList[fd];

    _context->fd = fd;
    _context->enType = EV_TYPE_TIMER;
    _context->events = EPOLLIN;
    _context->evCallback = cb;
    _context->argv = argv;

    return addEvent(ev, _context) < 0 ? -1 : fd;
}

int ev_addAlarmTimer(struct eventLoop *ev, ev_callback cb, unsigned int firstMsec, unsigned int cycleMsec, const void *argv)
{
    int fd;
    struct itimerspec timeValue;
    evContext *_context;

    if ((fd = timerfd_create(CLOCK_REALTIME_ALARM, 0)) < 0 || (fd >= EV_MAX_FD))
    {
        return -1;
    }

    timeValue.it_value.tv_sec = (time_t)firstMsec / 1000;
    timeValue.it_value.tv_nsec = (long)(firstMsec % 1000) * 1000000;

    timeValue.it_interval.tv_sec = (time_t)cycleMsec / 1000;
    timeValue.it_interval.tv_nsec = (long)(cycleMsec % 1000) * 1000000;

    if (timerfd_settime(fd, 0, &timeValue, NULL) < 0)
    {
        return -1;
    }

    _context = &ev->eventList[fd];

    _context->fd = fd;
    _context->enType = EV_TYPE_TIMER;
    _context->events = EPOLLIN;
    _context->evCallback = cb;
    _context->argv = argv;

    return addEvent(ev, _context) < 0 ? -1 : fd;
}

int ev_addEvent(struct eventLoop *ev, ev_callback cb, int fd, const void *argv)
{
    evContext *_context;

    if (fd < 0 || fd >= EV_MAX_FD)
    {
        return -1;
    }

    _context = &ev->eventList[fd];

    _context->fd = fd;
    _context->enType = EV_TYPE_SOCKET;
    _context->events = EPOLLIN;
    _context->evCallback = cb;
    _context->argv = argv;

    return addEvent(ev, _context) < 0 ? -1 : fd;
}

struct eventLoop *ev_create(void)
{
    struct eventLoop *_evLoop;

    if ((_evLoop = (struct eventLoop *)malloc(sizeof(struct eventLoop))) == NULL)
    {
        return NULL;
    }

    if ((_evLoop->eventList = (evContext *)malloc(sizeof(evContext) * EV_MAX_FD)) == NULL)
    {
        free(_evLoop);
        return NULL;
    }

    if ((_evLoop->epfd = epoll_create(EV_MAX_FD)) < 0)
    {
        free(_evLoop->eventList);
        free(_evLoop);
        return NULL;
    }

    _evLoop->maxfd = -1;
    _evLoop->size = EV_MAX_FD;
    _evLoop->stop = 0;

    return _evLoop;
}

void ev_start(struct eventLoop *ev)
{
    ev->stop = 0;
}

void ev_stop(struct eventLoop *ev)
{
    ev->stop = 1;
}

void ev_loop(struct eventLoop *ev)
{
    int nfds;
    int i;
    struct epoll_event _event[EV_WAIT_CNT];

    while (!ev->stop)
    {
        nfds = epoll_wait(ev->epfd, _event, EV_WAIT_CNT, -1);

        if (nfds > 0)
        {
            for (i = 0; i < nfds; i++)
            {
                if (_event[i].events & EPOLLIN)
                {
                    ltEvnetHandler(ev, _event[i].data.fd);
                }
            }
        }
    }
}
