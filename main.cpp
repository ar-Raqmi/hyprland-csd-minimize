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

        m_maximizeCommandVal = makeShared<Config::Values::CStringValue>(
            "plugin:csd-minimize:maximize_command",
            "Command to execute when the CSD maximize button is pressed (native maximize still applies). Empty by default.",
            ""
        );
        HyprlandAPI::addConfigValueV2(m_handle, m_maximizeCommandVal);

        m_fullscreenCommandVal = makeShared<Config::Values::CStringValue>(
            "plugin:csd-minimize:fullscreen_command",
            "Command to execute when the CSD fullscreen button is pressed (native fullscreen still applies). Empty by default.",
            ""
        );
        HyprlandAPI::addConfigValueV2(m_handle, m_fullscreenCommandVal);

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
    SP<Config::Values::CStringValue> m_maximizeCommandVal;
    SP<Config::Values::CStringValue> m_fullscreenCommandVal;
    std::unordered_map<Desktop::View::CWindow*, SWindowListener> m_windowListeners;
    CHyprSignalListener m_windowOpenListener;
    CHyprSignalListener m_windowDestroyListener;

    void runCommand(const std::string& cmd) {
        if (cmd.empty())
            return;
        std::thread([cmd]() {
            system(cmd.c_str());
        }).detach();
    }

    void handleMinimize(PHLWINDOW pWindow) {
        if (!pWindow)
            return;
        runCommand(m_minimizeCommandVal->value());
    }

    void handleMaximize(PHLWINDOW pWindow) {
        if (!pWindow)
            return;
        runCommand(m_maximizeCommandVal->value());
    }

    void handleFullscreen(PHLWINDOW pWindow) {
        if (!pWindow)
            return;
        runCommand(m_fullscreenCommandVal->value());
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
                    if (!xdg)
                        return;
                    auto toplevel = xdg->m_toplevel.lock();
                    if (!toplevel)
                        return;
                    if (toplevel->m_state.requestsMinimize.value_or(false))
                        handleMinimize(pWindow);
                    if (toplevel->m_state.requestsMaximize.value_or(false))
                        handleMaximize(pWindow);
                    if (toplevel->m_state.requestsFullscreen.value_or(false))
                        handleFullscreen(pWindow);
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
                if (!xwayland)
                    return;
                if (xwayland->m_state.requestsMinimize.value_or(false)) {
                    xwayland->m_state.requestsMinimize.reset();
                    xwayland->setMinimized(false);
                    handleMinimize(pWindow);
                }
                if (xwayland->m_state.requestsMaximize.value_or(false)) {
                    xwayland->m_state.requestsMaximize.reset();
                    handleMaximize(pWindow);
                }
                if (xwayland->m_state.requestsFullscreen.value_or(false)) {
                    xwayland->m_state.requestsFullscreen.reset();
                    handleFullscreen(pWindow);
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
