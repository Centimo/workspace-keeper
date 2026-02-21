local wezterm = require 'wezterm'
local config = wezterm.config_builder()

-- Shell
config.default_prog = { '/bin/zsh' }

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
}

-- Opacity
config.window_background_opacity = 0.93

-- Scrollback
config.scrollback_lines = 10000

-- Window decorations (no title bar)
config.window_decorations = 'RESIZE'

-- Tab bar
config.use_fancy_tab_bar = false
config.hide_tab_bar_if_only_one_tab = true
config.tab_bar_at_bottom = true

-- Window
config.window_padding = {
  left = 0,
  right = 0,
  top = 0,
  bottom = 0,
}

-- Keys: Ctrl = text, Alt = signals
config.keys = {
  { key = 'LeftArrow',  mods = 'ALT', action = wezterm.action.SendKey { key = 'LeftArrow', mods = 'ALT' } },
  { key = 'RightArrow', mods = 'ALT', action = wezterm.action.SendKey { key = 'RightArrow', mods = 'ALT' } },

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

-- Mouse: intercept drag for native selection even when tmux has mouse reporting on
local act = wezterm.action
config.mouse_bindings = {
  -- Single click
  {
    event = { Down = { streak = 1, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.SelectTextAtMouseCursor('Cell'),
  },
  {
    event = { Drag = { streak = 1, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.ExtendSelectionToMouseCursor('Cell'),
  },
  {
    event = { Up = { streak = 1, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.CompleteSelectionOrOpenLinkAtMouseCursor('ClipboardAndPrimarySelection'),
  },
  -- Double click — select word
  {
    event = { Down = { streak = 2, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.SelectTextAtMouseCursor('Word'),
  },
  {
    event = { Drag = { streak = 2, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.ExtendSelectionToMouseCursor('Word'),
  },
  {
    event = { Up = { streak = 2, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.CompleteSelection('ClipboardAndPrimarySelection'),
  },
  -- Triple click — select line
  {
    event = { Down = { streak = 3, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.SelectTextAtMouseCursor('Line'),
  },
  {
    event = { Drag = { streak = 3, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.ExtendSelectionToMouseCursor('Line'),
  },
  {
    event = { Up = { streak = 3, button = 'Left' } },
    mods = 'NONE',
    mouse_reporting = true,
    action = act.CompleteSelection('ClipboardAndPrimarySelection'),
  },
}

return config
