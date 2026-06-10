#include <plugins/PluginAPI.hpp>
#include <desktop/view/Window.hpp>
#include <Compositor.hpp>
#include <event/EventBus.hpp>
#include <config/values/types/StringValue.hpp>
#include <protocols/XDGShell.hpp>
#include <xwayland/XSurface.hpp>
#include <thread>
#include <cstdlib>
#include <unordered_map>
#include <chrono>

class CCSDMinimizePlugin {
public:
    CCSDMinimizePlugin() = default;
    ~CCSDMinimizePlugin() = default;

    void init(HANDLE handle) {
        m_handle = handle;

        m_minimizeCommandVal = makeShared<Config::Values::CStringValue>(
            "plugin:csd-minimize:command",
            "Command to execute when minimizing",
            "hyprctl dispatch togglespecialworkspace"
        );
        HyprlandAPI::addConfigValueV2(m_handle, m_minimizeCommandVal);

        for (auto const& w : g_pCompositor->m_windows) {
            registerWindow(w);
        }

        m_windowOpenListener = Event::bus()->m_events.window.open.listen([this](PHLWINDOW w) {
            registerWindow(w);
        });

        m_windowDestroyListener = Event::bus()->m_events.window.destroy.listen([this](PHLWINDOW w) {
            unregisterWindow(w.get());
        });
    }

    void exit() {
        m_windowListeners.clear();
        m_windowOpenListener.reset();
        m_windowDestroyListener.reset();
    }

private:
    struct SWindowListener {
        CHyprSignalListener stateChanged;
    };

    HANDLE m_handle = nullptr;
    SP<Config::Values::CStringValue> m_minimizeCommandVal;
    std::unordered_map<Desktop::View::CWindow*, SWindowListener> m_windowListeners;
    CHyprSignalListener m_windowOpenListener;
    CHyprSignalListener m_windowDestroyListener;

    void handleMinimize(PHLWINDOW pWindow) {
        if (!pWindow)
            return;

        std::string cmd = m_minimizeCommandVal->value();
        std::thread([cmd]() {
            system(cmd.c_str());
        }).detach();
    }

    void registerWindow(PHLWINDOW pWindow) {
        if (!pWindow)
            return;

        Desktop::View::CWindow* w = pWindow.get();
        if (m_windowListeners.contains(w))
            return;

        SWindowListener listener;
        PHLWINDOWREF pWindowRef = pWindow;

        auto xdg = pWindow->m_xdgSurface.lock();
        if (xdg) {
            auto toplevel = xdg->m_toplevel.lock();
            if (toplevel) {
                listener.stateChanged = toplevel->m_events.stateChanged.listen([this, pWindowRef]() {
                    auto pWindow = pWindowRef.lock();
                    if (!pWindow)
                        return;
                    auto xdg = pWindow->m_xdgSurface.lock();
                    if (xdg) {
                        auto toplevel = xdg->m_toplevel.lock();
                        if (toplevel && toplevel->m_state.requestsMinimize.value_or(false)) {
                            handleMinimize(pWindow);
                        }
                    }
                });
                m_windowListeners[w] = std::move(listener);
                return;
            }
        }

        auto xwayland = pWindow->m_xwaylandSurface.lock();
        if (xwayland) {
            listener.stateChanged = xwayland->m_events.stateChanged.listen([this, pWindowRef]() {
                auto pWindow = pWindowRef.lock();
                if (!pWindow)
                    return;
                auto xwayland = pWindow->m_xwaylandSurface.lock();
                if (xwayland && xwayland->m_state.requestsMinimize.value_or(false)) {
                    xwayland->m_state.requestsMinimize.reset();
                    xwayland->setMinimized(false);
                    handleMinimize(pWindow);
                }
            });
            m_windowListeners[w] = std::move(listener);
            return;
        }
    }

    void unregisterWindow(Desktop::View::CWindow* w) {
        m_windowListeners.erase(w);
    }
};

inline UP<CCSDMinimizePlugin> g_pCSDMinimizePlugin;

APICALL EXPORT std::string pluginAPIVersion() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE handle) {
    g_pCSDMinimizePlugin = makeUnique<CCSDMinimizePlugin>();
    g_pCSDMinimizePlugin->init(handle);

    return PLUGIN_DESCRIPTION_INFO{
        .name = "csd-minimize",
        .description = "Configurable Client-Side Decoration (CSD) minimize button handler",
        .author = "ar-Raqmi"
    };
}

APICALL EXPORT void pluginExit() {
    g_pCSDMinimizePlugin->exit();
    g_pCSDMinimizePlugin.reset();
}
