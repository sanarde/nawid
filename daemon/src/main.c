/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "app.h"

static int open_signal_fd(void)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0)
        return -1;
    return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int parse_backend(const char *name, enum nawid_backend *backend)
{
    if (strcmp(name, "auto") == 0)
        *backend = NAWID_BACKEND_AUTO;
    else if (strcmp(name, "wayland") == 0)
        *backend = NAWID_BACKEND_WAYLAND;
    else if (strcmp(name, "x11") == 0)
        *backend = NAWID_BACKEND_X11;
    else
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    struct nawid_source signal_source = {NAWID_SRC_SIGNAL};
    enum nawid_backend backend = NAWID_BACKEND_AUTO;
    struct nawid_app app;
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = &signal_source};
    int sig_fd;
    int epfd;
    int status = 0;

    if (argc == 3 && strcmp(argv[1], "-b") == 0 && parse_backend(argv[2], &backend) == 0) {
    } else if (argc != 1) {
        fprintf(stderr, "usage: nawid [-b auto|wayland|x11]\n");
        return 2;
    }

    sig_fd = open_signal_fd();
    epfd = epoll_create1(EPOLL_CLOEXEC);
    if (sig_fd < 0 || epfd < 0 || epoll_ctl(epfd, EPOLL_CTL_ADD, sig_fd, &ev) != 0) {
        fprintf(stderr, "nawid: %s\n", strerror(errno));
        return 1;
    }

    if (nawid_app_open(&app, epfd, backend) != 0) {
        nawid_app_close(&app);
        return 1;
    }

    for (;;) {
        struct epoll_event event;
        struct nawid_source *source;
        int n;

        if (nawid_app_prepare(&app) != 0) {
            fprintf(stderr, "nawid: lost the display connection\n");
            status = 1;
            break;
        }
        n = epoll_wait(epfd, &event, 1, -1);
        if (n < 0 && errno != EINTR) {
            fprintf(stderr, "nawid: epoll_wait: %s\n", strerror(errno));
            status = 1;
            break;
        }
        source = n > 0 ? event.data.ptr : NULL;
        if (nawid_app_finish(&app, source && source->kind == NAWID_SRC_DISPLAY) != 0) {
            fprintf(stderr, "nawid: lost the display connection\n");
            status = 1;
            break;
        }
        if (!source)
            continue;

        if (source->kind == NAWID_SRC_SIGNAL) {
            struct signalfd_siginfo info;

            if (read(sig_fd, &info, sizeof(info)) == sizeof(info))
                break;
        } else if (source->kind == NAWID_SRC_CONFIG) {
            nawid_app_config_event(&app);
        } else if (source->kind == NAWID_SRC_BUS) {
            nawid_bus_event(&app.bus, (struct nawid_bus_watch *)source, event.events);
        }
    }

    nawid_app_close(&app);
    return status;
}
