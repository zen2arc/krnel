#ifndef PLUGIN_H
#define PLUGIN_H

typedef struct {
    const char *name;
    void (*init)(void);
} plugin_t;

void register_plugin(plugin_t *p);
void plugins_init(void);

/* plugin autoregistration. to import your plugin, please add it here */
#define REGISTER_PLUGIN(name, initfunc) \
    plugin_t name##_plugin = {#name, initfunc}; \
    __attribute__((constructor)) static void name##_auto_register(void) { register_plugin(&name##_plugin); }

#endif
