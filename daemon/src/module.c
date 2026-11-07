/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdlib.h>

#include <v9nd/module.h>

#include "app.h"

struct nawid_module {
    struct nawid_loop loop;
    v9nd_host_t *host;
    struct nawid_app app;
};

static int loop_watch(struct nawid_loop *loop, int fd, uint32_t events, nawid_watch_fn fn,
                      void *user)
{
    return v9nd_host_watch(((struct nawid_module *)loop)->host, fd, events, fn, user);
}

static void loop_unwatch(struct nawid_loop *loop, int fd)
{
    v9nd_host_unwatch(((struct nawid_module *)loop)->host, fd);
}

static int loop_subscribe(struct nawid_loop *loop, const char *prefix, cfgd_changed_fn fn,
                          void *user)
{
    return v9nd_host_subscribe(((struct nawid_module *)loop)->host, prefix, fn, user);
}

static void loop_unsubscribe(struct nawid_loop *loop, int id)
{
    v9nd_host_unsubscribe(((struct nawid_module *)loop)->host, id);
}

static int module_start(v9nd_host_t *host, void **instance)
{
    struct nawid_module *module = calloc(1, sizeof(*module));

    if (!module)
        return -1;
    module->host = host;
    module->loop.cfgd = v9nd_host_cfgd(host);
    module->loop.watch = loop_watch;
    module->loop.unwatch = loop_unwatch;
    module->loop.subscribe = loop_subscribe;
    module->loop.unsubscribe = loop_unsubscribe;
    if (nawid_app_open(&module->app, &module->loop, NAWID_BACKEND_AUTO) != 0) {
        nawid_app_close(&module->app);
        free(module);
        return -1;
    }
    *instance = module;
    return 0;
}

static void module_stop(void *instance)
{
    struct nawid_module *module = instance;

    nawid_app_close(&module->app);
    free(module);
}

__attribute__((visibility("default"))) const struct v9nd_module v9nd_module = {
    .api = V9ND_MODULE_API,
    .name = "nawid",
    .start = module_start,
    .stop = module_stop,
};
