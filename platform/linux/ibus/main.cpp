#include "engine.h"

#include <ibus.h>

#include <atomic>
#include <cstring>

namespace
{

IBusEngine *CreateEngine(IBusFactory *factory, const gchar *engine_name, gpointer user_data)
{
    static_cast<void>(user_data);
    if (engine_name == nullptr ||
        (std::strcmp(engine_name, "nakl-telex") != 0 &&
         std::strcmp(engine_name, "nakl-vni") != 0))
    {
        return nullptr;
    }
    static std::atomic<unsigned int> next_identifier{1};
    const gchar *format = "/org/freedesktop/IBus/Engine/NAKL/%u";
    gchar *object_path = g_strdup_printf(format, next_identifier.fetch_add(1));
    auto *engine = IBUS_ENGINE(g_object_new(
        NAKL_TYPE_IBUS_ENGINE, "engine-name", engine_name, "object-path", object_path, "connection",
        ibus_service_get_connection(IBUS_SERVICE(factory)), "has-focus-id", TRUE,
        "active-surrounding-text", TRUE, nullptr));
    g_free(object_path);
    return engine;
}

} // namespace

int main(int argc, char **argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);
    ibus_init();

    IBusBus *bus = ibus_bus_new();
    if (bus == nullptr || !ibus_bus_is_connected(bus))
    {
        if (bus != nullptr)
        {
            g_object_unref(bus);
        }
        return 1;
    }

    IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
    if (factory == nullptr)
    {
        g_object_unref(bus);
        return 1;
    }
    g_signal_connect(factory, "create-engine", G_CALLBACK(CreateEngine), nullptr);
    if (!ibus_bus_request_name(bus, "org.freedesktop.IBus.NAKL", 0))
    {
        g_object_unref(factory);
        g_object_unref(bus);
        return 1;
    }
    ibus_main();

    g_object_unref(factory);
    g_object_unref(bus);
    return 0;
}
