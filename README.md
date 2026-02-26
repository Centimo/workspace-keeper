# workspace-keeper

KDE Plasma workspace manager — одна команда создаёт изолированное рабочее пространство:
виртуальный десктоп + CLion + WezTerm + Floorp (браузер).

Монорепо объединяет:
- **bin/workspace** — bash-скрипт управления workspace'ами
- **daemon/** — Qt6 C++ демон popup-меню (мгновенный отклик через Unix domain socket)
- **config/** — конфигурации WezTerm, KDE, Floorp

## Структура

```
workspace-keeper/
├── bin/
│   └── workspace                  # основной bash-скрипт
├── config/
│   ├── wezterm/wezterm.lua        # конфиг терминала
│   ├── kde/
│   │   ├── kwinrulesrc            # правила окон KWin
│   │   ├── workspace-menu.desktop # launcher для хоткея
│   │   └── workspace-menu-daemon.desktop  # autostart демона
│   ├── autostart/
│   │   └── org.wezfurlong.wezterm.desktop
│   └── floorp/
│       └── brotab_mediator.json   # BroTab native messaging
├── daemon/
│   ├── CMakeLists.txt
│   ├── Dockerfile
│   └── src/                       # C++20 Qt6 Widgets
├── docs/
│   └── design.md                  # дизайн-документ демона
└── install.sh
```

## Идея

Каждый проект живёт на отдельном виртуальном рабочем столе KDE с фиксированной раскладкой окон.
Переключение между проектами — через popup-меню (`Alt+Tab`) или стандартными средствами KDE (Meta+1..9).
Вкладки браузера сохраняются при закрытии и восстанавливаются при открытии.
Терминальные сессии сохраняются при закрытии GUI-окна через WezTerm mux-server.

## Раскладка монитора

Dual-монитор 2560x1440 (два по 1280x1440):

```
┌──────────────────┬──────────────────┐
│                  │                  │
│  Floorp          │  WezTerm          │
│  0,0 1280x1440   │  1280,0 1280x1440│
│                  │                  │
│                  │                  │
└──────────────────┴──────────────────┘
         CLion — fullscreen поверх обоих мониторов
```

## Команды

```
workspace create <path> [name]   — создать пространство (или переключиться на существующее)
workspace close [name]           — сохранить вкладки, закрыть окна, удалить десктоп
workspace save [name]            — сохранить вкладки Floorp (без закрытия)
workspace pin                    — закрепить активное окно Floorp на всех десктопах
workspace list                   — список пространств
workspace menu                   — popup-меню для переключения/создания/закрытия
```

## Popup-меню

Qt6 Widgets демон, запускается при входе в систему. Отзывчивость <200ms.

| Клавиша | Действие |
|---|---|
| Enter | Переключиться на активный workspace или создать неактивный |
| Tab | Дополнить путь к директории (zsh-style completion) |
| Alt+Del | Закрыть выбранный workspace |
| Esc | Отмена |

Path browsing: ввод пути, начинающегося с `/`, динамически показывает список поддиректорий.

## Зависимости

| Пакет | Назначение |
|---|---|
| wezterm | Терминал |
| jq | Обработка JSON |
| floorp | Браузер (форк Firefox) |
| wmctrl | Управление X11-окнами |
| xdotool | Получение активного окна |
| xprop | Чтение свойств X11-окон |
| qdbus | D-Bus CLI (KDE) |
| bt (brotab) | CLI для управления вкладками браузера |
| CLion | IDE (через JetBrains Toolbox) |
| docker | Сборка демона |

Сборка демона (автоматически через install.sh):
- Qt6 (Core, Gui, Widgets, Network)
- XCB
- CMake 3.20+, C++20

## Установка

```bash
git clone git@github.com:Centimo/workspace-keeper.git
cd workspace-keeper
./install.sh
```

`install.sh`:
1. Создаёт симлинки конфигов в домашнюю директорию (существующие файлы → `*.bak`)
2. Собирает демон popup-меню через Docker (ubuntu:24.04 + qt6-base-dev)
3. Устанавливает бинарник в `~/.local/bin/workspace-menu`
4. Настраивает KDE шорткаты (Alt+Tab → workspace menu, Ctrl+Tab → переключение окон)

## Глобальные горячие клавиши

| Комбинация | Действие |
|---|---|
| Alt+Tab | Workspace menu (popup) |
| Ctrl+Tab | Переключение окон KDE (стандартное Walk Through Windows) |
| Ctrl+Shift+Tab | Переключение окон KDE (обратное) |
| Meta+1..9 | Переключение на десктоп по номеру |
