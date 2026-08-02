#include <plugins/PluginAPI.hpp>
#include <desktop/view/Window.hpp>
#include <desktop/state/WindowState.hpp>
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
    CCSDMinimizePlugin()  = default;
    ~CCSDMinimizePlugin() = default;

    void init(HANDLE handle) {
        m_handle = handle;

        m_minimizeCommandVal = addCommand("plugin:csd-minimize:command", "Command to execute when minimizing", "hyprctl dispatch togglespecialworkspace");
        m_maximizeCommandVal =
            addCommand("plugin:csd-minimize:maximize_command", "Command to execute when the CSD maximize button is pressed (native maximize still applies). Empty by default.", "");
        m_fullscreenCommandVal = addCommand("plugin:csd-minimize:fullscreen_command",
                                            "Command to execute when the CSD fullscreen button is pressed (native fullscreen still applies). Empty by default.", "");

        for (auto const& w : Desktop::windowState()->windows()) {
            registerWindow(w);
        }

        m_windowOpenListener = Event::bus()->m_events.window.open.listen([this](PHLWINDOW w) { registerWindow(w); });

        m_configReloadListener = Event::bus()->m_events.config.reloaded.listen([this]() {
            for (auto const& w : Desktop::windowState()->windows()) {
                applySuppression(w);
            }
        });

        m_windowDestroyListener = Event::bus()->m_events.window.destroy.listen([this](PHLWINDOWREF w) {
            auto pWindow = w.lock();
            if (pWindow)
                unregisterWindow(pWindow.get());
        });
    }

    void exit() {
        m_windows.clear();
        m_windowOpenListener.reset();
        m_windowDestroyListener.reset();
        m_configReloadListener.reset();
    }

  private:
    struct SSuppressState {
        bool maximize   = false;
        bool fullscreen = false;
    };

    struct SWindowState {
        CHyprSignalListener stateChanged;
        SSuppressState      suppress;
        bool                listening = false;
    };

    HANDLE                                                    m_handle = nullptr;
    SP<Config::Values::CStringValue>                          m_minimizeCommandVal;
    SP<Config::Values::CStringValue>                          m_maximizeCommandVal;
    SP<Config::Values::CStringValue>                          m_fullscreenCommandVal;
    std::unordered_map<Desktop::View::CWindow*, SWindowState> m_windows;
    CHyprSignalListener                                       m_windowOpenListener;
    CHyprSignalListener                                       m_windowDestroyListener;
    CHyprSignalListener                                       m_configReloadListener;

    SP<Config::Values::CStringValue>                          addCommand(const char* key, const char* description, const char* fallback) {
        auto value = makeShared<Config::Values::CStringValue>(key, description, fallback);
        HyprlandAPI::addConfigValueV2(m_handle, value);
        return value;
    }

    void runCommand(const std::string& cmd) {
        if (cmd.empty())
            return;
        std::thread([cmd]() { system(cmd.c_str()); }).detach();
    }

    void handleState(PHLWINDOW pWindow, bool minimize, bool maximize, bool fullscreen) {
        if (!pWindow)
            return;
        if (minimize)
            runCommand(m_minimizeCommandVal->value());
        if (maximize)
            runCommand(m_maximizeCommandVal->value());
        if (fullscreen)
            runCommand(m_fullscreenCommandVal->value());
    }

    static void toggleSuppressionBit(uint64_t& flags, bool want, bool prev, uint64_t mask) {
        if (want != prev)
            flags = want ? (flags | mask) : (flags & ~mask);
    }

    void applySuppression(const PHLWINDOW& pWindow) {
        if (!pWindow)
            return;

        SSuppressState&      cur  = m_windows[pWindow.get()].suppress;
        const SSuppressState prev = cur;

        const bool           wantMax = !m_maximizeCommandVal->value().empty();
        const bool           wantFs  = !m_fullscreenCommandVal->value().empty();

        uint64_t&            flags = pWindow->m_suppressedEvents;
        toggleSuppressionBit(flags, wantMax, prev.maximize, Desktop::View::SUPPRESS_MAXIMIZE);
        toggleSuppressionBit(flags, wantFs, prev.fullscreen, Desktop::View::SUPPRESS_FULLSCREEN);

        cur = SSuppressState{wantMax, wantFs};
    }

    void registerWindow(PHLWINDOW pWindow) {
        if (!pWindow)
            return;

        Desktop::View::CWindow* w = pWindow.get();

        applySuppression(pWindow);

        SWindowState& state = m_windows[w];
        if (state.listening)
            return;

        const PHLWINDOWREF pWindowRef = pWindow;

        const auto         xdg      = pWindow->m_xdgSurface.lock();
        const auto         toplevel = xdg ? xdg->m_toplevel.lock() : nullptr;
        if (toplevel) {
            state.stateChanged = toplevel->m_events.stateChanged.listen([this, pWindowRef]() {
                auto pWindow = pWindowRef.lock();
                if (!pWindow)
                    return;
                auto xdg = pWindow->m_xdgSurface.lock();
                if (!xdg)
                    return;
                auto toplevel = xdg->m_toplevel.lock();
                if (!toplevel)
                    return;
                handleState(pWindow, toplevel->m_state.requestsMinimize.value_or(false), toplevel->m_state.requestsMaximize.has_value(),
                            toplevel->m_state.requestsFullscreen.has_value());
            });
            state.listening    = true;
            return;
        }

        const auto xwayland = pWindow->m_xwaylandSurface.lock();
        if (xwayland) {
            state.stateChanged = xwayland->m_events.stateChanged.listen([this, pWindowRef]() {
                auto pWindow = pWindowRef.lock();
                if (!pWindow)
                    return;
                auto xwayland = pWindow->m_xwaylandSurface.lock();
                if (!xwayland)
                    return;

                const bool minimize   = xwayland->m_state.requestsMinimize.value_or(false);
                const bool maximize   = xwayland->m_state.requestsMaximize.has_value();
                const bool fullscreen = xwayland->m_state.requestsFullscreen.has_value();

                if (minimize) {
                    xwayland->m_state.requestsMinimize.reset();
                    xwayland->setMinimized(false);
                }
                if (maximize)
                    xwayland->m_state.requestsMaximize.reset();
                if (fullscreen)
                    xwayland->m_state.requestsFullscreen.reset();

                handleState(pWindow, minimize, maximize, fullscreen);
            });
            state.listening    = true;
            return;
        }
    }

    void unregisterWindow(Desktop::View::CWindow* w) {
        m_windows.erase(w);
    }
};

inline UP<CCSDMinimizePlugin> g_pCSDMinimizePlugin;

APICALL EXPORT std::string pluginAPIVersion() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO pluginInit(HANDLE handle) {
    g_pCSDMinimizePlugin = makeUnique<CCSDMinimizePlugin>();
    g_pCSDMinimizePlugin->init(handle);

    return PLUGIN_DESCRIPTION_INFO{.name = "csd-minimize", .description = "Configurable Client-Side Decoration (CSD) minimize button handler", .author = "ar-Raqmi"};
}

APICALL EXPORT void pluginExit() {
    g_pCSDMinimizePlugin->exit();
    g_pCSDMinimizePlugin.reset();
}
