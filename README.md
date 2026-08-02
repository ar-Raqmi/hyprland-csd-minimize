<h1 align="center">hyprland-csd-minimize</h1>
<p align="center">
  <video src="https://github.com/user-attachments/assets/8108796e-7ed2-49df-837b-c5f2f472b16e" controls muted width="100%"></video>
</p>
<p align="center">
  <strong>Make your CSD Minimize button works in Hyprland.</strong>
</p>

Hyprland is a tiling window manager, so it doesn't do window minimization natively. If you press the minimize button (`_`) on a window with Client-Side Decorations (like Chrome, VSCode or Discord), the window usually do nothing, freezes or gets stuck because the app expects the compositor to handle it.

This plugin intercepts that button click, tells the application that it isn't minimized and lets you run your own custom command.

It also supports the **maximize** and **fullscreen** CSD buttons. When you set a command for either of them, the native maximize/fullscreen action is suppressed so *only* your command runs; leave them empty to keep the native behaviour.

---

## Installation

Install using `hyprpm`:

```bash
# Add the repository
hyprpm add https://github.com/ar-Raqmi/hyprland-csd-minimize

# Enable the plugin
hyprpm enable csd-minimize

# Load/Update the plugins
hyprpm reload
```
## Configuration (`hyprland.lua`)

To ensure the plugin loads automatically when Hyprland starts

Add this to your `~/.config/hypr/hyprland.lua`:

```lua
hl.on("hyprland.start", function ()
    hl.exec_cmd("hyprpm reload")
end)
```
---

Configure the custom commands in your `~/.config/hypr/hyprland.lua`:

```lua
hl.config({
    plugin = {
        csd_minimize = {
            -- Run when the minimize button is pressed.
            -- Example: move the window to a special workspace silently.
            command = "hyprctl dispatch \"hl.dsp.window.move({ workspace = \'special\', follow = false, })\""

            -- Run when the maximize button is pressed.
            -- When set, native maximize is suppressed and only this command runs.
            -- Example: toggle floating instead of maximizing.
            maximize_command = "hyprctl dispatch 'hl.dsp.window.float({action = \"toggle\"})'"

            -- Run when the fullscreen button is pressed.
            -- When set, native fullscreen is suppressed and only this command runs.
            -- Leave empty (or unset) to keep the native fullscreen behaviour.
            fullscreen_command = ""
        }
    }
})
```

---

<div align="center">

  *🖋️ the pen hasn't lifted*

</div>

