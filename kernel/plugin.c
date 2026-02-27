#include "kernel.h"
#include "plugin.h"

static plugin_t *plugins[32];
static int plugin_count = 0;

void register_plugin(plugin_t *p) {
    if (plugin_count < 32 && p) plugins[plugin_count++] = p;
}

void plugins_init(void) {
    for (int i = 0; i < plugin_count; i++) {
        if (plugins[i] && plugins[i]->init) plugins[i]->init();
    }
}
