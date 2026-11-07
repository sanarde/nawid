/* SPDX-License-Identifier: GPL-3.0-only */
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "app.h"
#include "standalone.h"

static bool stop;

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

static bool finished(void *user)
{
    const struct nawid_app *app = user;

    return stop || app->failed;
}

static void signal_ready(int fd, uint32_t events, void *user)
{
    struct signalfd_siginfo info;

    (void)events;
    (void)user;
    if (read(fd, &info, sizeof(info)) == sizeof(info))
        stop = true;
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
    enum nawid_backend backend = NAWID_BACKEND_AUTO;
    struct nawid_standalone standalone;
    struct nawid_app app = {0};
    int sig_fd;
    int status = 0;

    if (argc == 3 && strcmp(argv[1], "-b") == 0 && parse_backend(argv[2], &backend) == 0) {
    } else if (argc != 1) {
        fprintf(stderr, "usage: nawid [-b auto|wayland|x11]\n");
        return 2;
    }

    sig_fd = open_signal_fd();
    if (sig_fd < 0) {
        fprintf(stderr, "nawid: %s\n", strerror(errno));
        return 1;
    }
    if (nawid_standalone_open(&standalone) != 0)
        return 1;
    if (standalone.loop.watch(&standalone.loop, sig_fd, EPOLLIN, signal_ready, NULL) != 0 ||
        nawid_app_open(&app, &standalone.loop, backend) != 0) {
        nawid_app_close(&app);
        nawid_standalone_close(&standalone);
        return 1;
    }

    if (nawid_standalone_run(&standalone, finished, &app) != 0 || app.failed)
        status = 1;

    nawid_app_close(&app);
    nawid_standalone_close(&standalone);
    return status;
}
