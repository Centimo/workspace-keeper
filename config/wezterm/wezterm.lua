local wezterm = require 'wezterm'
local config = wezterm.config_builder()

-- Shell
config.default_prog = { '/bin/zsh' }

-- Mux server (unix domain)
config.unix_domains = {
  { name = 'unix' },
}

-- Font
config.font = wezterm.font('Hack')
config.font_size = 16.0

-- Colors (Breeze Dark)
config.colors = {
  foreground = '#fcfcfc',
  background = '#232627',

  cursor_bg = '#fcfcfc',
  cursor_fg = '#232627',
  cursor_border = '#fcfcfc',

  selection_fg = '#232627',
  selection_bg = '#fcfcfc',

  ansi = {
    '#232627', -- black
    '#ed1515', -- red
    '#11d116', -- green
    '#f67400', -- yellow/orange
    '#1d99f3', -- blue
    '#9b59b6', -- magenta
    '#1abc9c', -- cyan
    '#fcfcfc', -- white
  },
  brights = {
    '#7f8c8d', -- bright black
    '#c0392b', -- bright red
    '#1cdc9a', -- bright green
    '#fdbc4b', -- bright yellow
    '#3daee9', -- bright blue
    '#8e44ad', -- bright magenta
    '#16a085', -- bright cyan
    '#ffffff', -- bright white
  },

  tab_bar = {
    background = '#232627',
    active_tab = {
      bg_color = '#1d99f3',
      fg_color = '#ffffff',
      intensity = 'Bold',
    },
    inactive_tab = {
      bg_color = '#232627',
      fg_color = '#fcfcfc',
    },
    inactive_tab_hover = {
      bg_color = '#31363b',
      fg_color = '#fcfcfc',
    },
    new_tab = {
      bg_color = '#232627',
      fg_color = '#7f8c8d',
    },
    new_tab_hover = {
      bg_color = '#31363b',
      fg_color = '#fcfcfc',
    },
  },
}

-- Opacity
config.window_background_opacity = 0.93

-- Scrollback
config.scrollback_lines = 10000

-- Window decorations (no title bar)
config.window_decorations = 'RESIZE'

-- Close GUI without confirmation (panes stay alive in mux-server)
config.window_close_confirmation = 'NeverPrompt'

-- Tab bar
config.use_fancy_tab_bar = false
config.hide_tab_bar_if_only_one_tab = true
config.tab_bar_at_bottom = true
config.show_new_tab_button_in_tab_bar = false

-- Window
config.window_padding = {
  left = 0,
  right = 0,
  top = 0,
  bottom = 0,
}

-- Tab title format: "N:title" (like tmux #I:#W)
wezterm.on('format-tab-title', function(tab)
  local title = tab.active_pane.title
  if #title > 20 then
    title = title:sub(1, 20) .. '…'
  end
  return string.format(' %d:%s ', tab.tab_index + 1, title)
end)

-- Keys: Ctrl = text, Alt = signals
config.keys = {
  -- Tab navigation (native, replaces tmux)
  { key = 'LeftArrow',  mods = 'ALT', action = wezterm.action.ActivateTabRelative(-1) },
  { key = 'RightArrow', mods = 'ALT', action = wezterm.action.ActivateTabRelative(1) },
  { key = 't', mods = 'ALT', action = wezterm.action.SpawnTab('CurrentPaneDomain') },
  { key = 'w', mods = 'ALT', action = wezterm.action.CloseCurrentTab { confirm = true } },

  -- Ctrl+C: always copy, never SIGINT (use Alt+C for SIGINT)
  { key = 'c', mods = 'CTRL', action = wezterm.action.CopyTo('Clipboard') },

  -- Ctrl+V: paste text from clipboard
  { key = 'v', mods = 'CTRL', action = wezterm.action.PasteFrom('Clipboard') },

  -- Ctrl+Shift+V: paste image (forward to Claude Code)
  { key = 'v', mods = 'CTRL|SHIFT', action = wezterm.action.SendKey { key = 'v', mods = 'CTRL' } },

  -- Ctrl+Z: undo (Ctrl+_)
  { key = 'z', mods = 'CTRL', action = wezterm.action.SendKey { key = '_', mods = 'CTRL' } },

  -- Shift+Enter: newline (Claude Code)
  { key = 'Enter', mods = 'SHIFT', action = wezterm.action.SendString('\x1b\r') },

  -- Alt = terminal signals
  { key = 'c', mods = 'ALT', action = wezterm.action.SendKey { key = 'c', mods = 'CTRL' } },  -- SIGINT
  { key = 'z', mods = 'ALT', action = wezterm.action.SendKey { key = 'z', mods = 'CTRL' } },  -- SIGTSTP
  { key = 'd', mods = 'ALT', action = wezterm.action.SendKey { key = 'd', mods = 'CTRL' } },  -- EOF

  -- Disable Alt+Enter fullscreen toggle
  { key = 'Enter', mods = 'ALT', action = wezterm.action.DisableDefaultAssignment },
}

return config
