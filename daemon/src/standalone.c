/* SPDX-License-Identifier: GPL-3.0-only */
#include "standalone.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

struct standalone_watch {
    struct standalone_watch *next;
    int fd;
    nawid_watch_fn fn;
    void *user;
};

static int loop_watch(struct nawid_loop *loop, int fd, uint32_t events, nawid_watch_fn fn,
                      void *user)
{
    struct nawid_standalone *standalone = (struct nawid_standalone *)loop;
    struct standalone_watch *watch = calloc(1, sizeof(*watch));
    struct epoll_event ev = {.events = events};

    if (!watch)
        return -1;
    watch->fd = fd;
    watch->fn = fn;
    watch->user = user;
    ev.data.ptr = watch;
    if (epoll_ctl(standalone->epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        free(watch);
        return -1;
    }
    watch->next = standalone->watches;
    standalone->watches = watch;
    return 0;
}

static void loop_unwatch(struct nawid_loop *loop, int fd)
{
    struct nawid_standalone *standalone = (struct nawid_standalone *)loop;

    for (struct standalone_watch **link = &standalone->watches; *link;
         link = &(*link)->next) {
        struct standalone_watch *watch = *link;

        if (watch->fd != fd)
            continue;
        epoll_ctl(standalone->epfd, EPOLL_CTL_DEL, fd, NULL);
        *link = watch->next;
        free(watch);
        return;
    }
}

static int loop_subscribe(struct nawid_loop *loop, const char *prefix, cfgd_changed_fn fn,
                          void *user)
{
    return cfgd_subscribe(loop->cfgd, prefix, fn, user);
}

static void loop_unsubscribe(struct nawid_loop *loop, int id)
{
    cfgd_unsubscribe(loop->cfgd, id);
}

static void config_ready(int fd, uint32_t events, void *user)
{
    (void)fd;
    (void)events;
    cfgd_dispatch(user);
}

int nawid_standalone_open(struct nawid_standalone *standalone)
{
    memset(standalone, 0, sizeof(*standalone));
    standalone->loop.watch = loop_watch;
    standalone->loop.unwatch = loop_unwatch;
    standalone->loop.subscribe = loop_subscribe;
    standalone->loop.unsubscribe = loop_unsubscribe;
    standalone->epfd = epoll_create1(EPOLL_CLOEXEC);
    standalone->loop.cfgd = cfgd_open();
    if (standalone->epfd < 0 || !standalone->loop.cfgd ||
        loop_watch(&standalone->loop, cfgd_notify_fd(standalone->loop.cfgd), EPOLLIN,
                   config_ready, standalone->loop.cfgd) != 0) {
        fprintf(stderr, "nawid: cannot open the event loop or the settings\n");
        nawid_standalone_close(standalone);
        return -1;
    }
    return 0;
}

void nawid_standalone_close(struct nawid_standalone *standalone)
{
    while (standalone->watches)
        loop_unwatch(&standalone->loop, standalone->watches->fd);
    cfgd_close(standalone->loop.cfgd);
    if (standalone->epfd >= 0)
        close(standalone->epfd);
    memset(standalone, 0, sizeof(*standalone));
    standalone->epfd = -1;
}

int nawid_standalone_run(struct nawid_standalone *standalone, bool (*done)(void *user),
                         void *user)
{
    while (!done(user)) {
        struct epoll_event event;
        int n = epoll_wait(standalone->epfd, &event, 1, -1);

        if (n < 0 && errno != EINTR) {
            fprintf(stderr, "nawid: epoll_wait: %s\n", strerror(errno));
            return -1;
        }
        if (n > 0) {
            struct standalone_watch *watch = event.data.ptr;

            watch->fn(watch->fd, event.events, watch->user);
        }
    }
    return 0;
}
