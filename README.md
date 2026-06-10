<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/status-stable-green.svg" alt="Status">
</p>
<h1 align="center">hyprland-csd-minimize</h1>
</p>
<p align="center">
  <strong>Minimize your CSD windows in Hyprland.</strong>
</p>

Hyprland is a tiling window manager, so it doesn't do window minimization natively. If you press the minimize button (`_`) on a window with Client-Side Decorations (like Chrome, VSCode or Discord), the window usually do nothing, freezes or gets stuck because the app expects the compositor to handle it.

This plugin intercepts that button click, tells the application that it isn't minimized and lets you run your own custom command.

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

Configure the custom command in your `~/.config/hypr/hyprland.lua`:

```lua
hl.config({
    plugin = {
        csd_minimize = {
            -- Example:
            -- Move the window to the special workspace silently
            command = "hyprctl dispatch \"hl.dsp.window.move({ workspace = \'special\', follow = false, })\""
            
            -- or

            -- Example: Toggle floating for the window
            command = "hyprctl dispatch 'hl.dsp.window.float({action = \"toggle\"})'"
        }
    }
})
```

---

<div align="center">

  *🖋️ the pen hasn't lifted*

</div>

