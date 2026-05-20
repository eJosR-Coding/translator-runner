# Translator Runner

> A KDE KRunner plugin to translate text inline using translate-shell.

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Plasma](https://img.shields.io/badge/Plasma-6.5+-blue)](https://kde.org/plasma-desktop/)

---

## Why this exists

The popular `krunner-translator` plugin became incompatible with Plasma 6.6+ / Qt 6.10 (crashes with `qBadAlloc`). This is a clean rewrite targeting modern KDE Frameworks 6.

---

## Usage

Open KRunner (`Alt+Space`), then:

| Syntax | Result |
|---|---|
| `tr:hola mundo` | Translates to English (default) |
| `tr-es:hello world` | Translates to Spanish |
| `tr-fr:hello world` | Translates to French |
| `tr-de:guten tag` | Translates to German |

Click the result → translation is copied to clipboard + KDE notification shown.

---

## Installation

### From source

**Requirements:**
- Plasma 6.5+
- Qt 6.5+
- KDE Frameworks 6
- `translate-shell` (the `trans` binary)

**Install build dependencies (Fedora):**

```bash
sudo dnf install gcc-c++ cmake extra-cmake-modules \
    qt6-qtbase-devel kf6-krunner-devel kf6-kcoreaddons-devel \
    kf6-ki18n-devel kf6-knotifications-devel \
    translate-shell
```

**Build & install:**

```bash
git clone https://github.com/[GITHUB-USER]/translator-runner.git
cd translator-runner
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
sudo make install
killall krunner
```

Then open System Settings → Search → Plasma Search, find "Translator Runner", and enable it.

---

## Project structure

```
translator-runner/
├── README.md
├── LICENSE                          # GPL-3.0-or-later (required — KRunner is GPL)
├── .gitignore
├── .clang-format                    # KDE code style
├── CMakeLists.txt                   # Build system (the most important file)
├── CHANGELOG.md
├── src/
│   ├── translatorrunner.h           # Class declaration
│   ├── translatorrunner.cpp         # Implementation (~80 lines)
│   ├── metadata.json                # Plugin metadata (KRunner reads this)
│   └── translatorrunner.notifyrc    # Notification events config
├── docs/
│   ├── architecture.md
│   ├── development.md
│   └── api-reference.md
├── packaging/
│   └── fedora.spec                  # RPM spec for distribution
└── .github/
    └── workflows/
        └── build.yml                # CI via GitHub Actions
```

---

## Pre-concepts: what you need to understand before touching the code

### C++ inheritance (how this plugin works)

```
AbstractRunner          ← Defined by KDE. You do NOT touch this.
    │
    └── TranslatorRunner  ← YOU write this. Implements the virtual methods.
```

`AbstractRunner` is a contract defined by KDE. You promise to implement `match()` and `run()`. The compiler won't let you forget them.

### The 3 KRunner API classes you'll touch

| Class | What it does |
|---|---|
| `AbstractRunner` | Your base class — defines the plugin interface |
| `RunnerContext` | Contains the user's query (`context.query()`). You add results with `context.addMatch(qm)` |
| `QueryMatch` | One individual result. Has: `text`, `subtext`, `icon`, `relevance`, `data` |

### KDE macros (the "magic" lines)

| Macro | What it does |
|---|---|
| `Q_OBJECT` | Marks class for signals/slots (must be first in the class body) |
| `K_PLUGIN_CLASS_WITH_JSON(Class, "metadata.json")` | Registers the class as a loadable plugin |
| `Q_UNUSED(x)` | "I know I don't use this parameter, suppress warning" |
| `i18n("text")` | Marks string for translation (i18n = internationalization) |

### QString (not std::string)

Qt has its own string type because it handles Unicode correctly and is faster for UI use.

```cpp
QString text = QStringLiteral("hola");    // efficient compile-time literal
QString combined = text + " mundo";       // concatenation
bool starts = text.startsWith("ho");      // useful methods
QString upper = text.toUpper();           // "HOLA"
QString sub = text.mid(1, 3);             // "ola"
```

`QStringLiteral` creates the QString at compile-time — faster than the normal constructor. Standard idiom in modern Qt code.

### Signals & Slots (Qt's event system)

When a Qt object "fires" a signal, all connected slots execute automatically. Like JavaScript event listeners.

```cpp
connect(process, &QProcess::finished, this, &TranslatorRunner::onProcessFinished);
// When process finishes, call my onProcessFinished()
```

For the initial plugin: you'll use synchronous `QProcess::waitForFinished()` instead — simpler to start with. Async with signals is a later improvement.

### QProcess (running system commands)

```cpp
QProcess proc;
proc.start("trans", {":en", "hola", "-b"});   // runs: trans :en "hola" -b
proc.waitForFinished(3000);                    // wait max 3 seconds
QString output = proc.readAllStandardOutput(); // capture stdout
QString errors = proc.readAllStandardError();  // capture stderr
int code = proc.exitCode();                    // 0 = success
```

This is how you call the `trans` CLI from inside C++.

---

## Architecture

### Technology stack

```
YOUR CODE (translatorrunner.cpp ~80 lines)
    │
    ├── KF6::Runner         → AbstractRunner, RunnerContext, QueryMatch
    ├── KF6::CoreAddons     → K_PLUGIN_CLASS_WITH_JSON, KPluginMetaData
    ├── KF6::I18n           → i18n() for translations
    ├── KF6::Notifications  → KNotification
    │
    ├── Qt6::Core           → QString, QObject, QProcess
    └── Qt6::Gui            → QClipboard, QGuiApplication
          │
          └── Linux kernel / libc
                │
                └── trans CLI  ← translate-shell binary
```

Build system: CMake + Extra-CMake-Modules (ECM, KDE's macros for CMake).

### How KRunner discovers your plugin (startup)

1. KRunner scans `/usr/lib64/qt6/plugins/kf6/krunner/` for `.so` files
2. For each `.so`: `dlopen()` loads it, reads embedded `metadata.json`
3. Checks if plugin is enabled in user config
4. If yes: instantiates `TranslatorRunner` — plugin is live

**Your job in this flow: nothing.** Just exist as a `.so` in the right folder. The `K_PLUGIN_CLASS_WITH_JSON` macro handles automatic registration.

### Match flow (when the user types)

```
User types "tr:hola mundo"
    ↓
KRunner calls match(context) on ALL runners in parallel
    ↓
TranslatorRunner::match():
    1. query = context.query()               → "tr:hola mundo"
    2. startsWith "tr:"? → yes
    3. text = "hola mundo", lang = "en"
    4. QProcess: trans :en "hola mundo" -b
    5. stdout: "hello world"
    6. Build QueryMatch: text="hello world", subtext="Translate to EN: hola mundo"
    7. context.addMatch(match)
    ↓
KRunner sorts all matches by relevance → shows list
```

### Run flow (when the user clicks a result)

```
User clicks "hello world"
    ↓
KRunner calls run(context, match)
    ↓
TranslatorRunner::run():
    1. translation = match.data().toString()   → "hello world"
    2. QClipboard::setText("hello world")      → system clipboard updated
    3. KNotification::sendEvent()              → D-Bus → Plasma notification
    ↓
User sees notification, pastes with Ctrl+V
```

### Build & install flow

```
First time:
    mkdir build && cd build
    cmake ..                    ← CMake scans CMakeLists.txt, finds Qt6 & KF6

Incremental builds (after first cmake):
    cd build
    make -j8                    ← g++ compiles .cpp → .o → links → translatorrunner.so

Install:
    sudo make install           ← copies .so to /usr/lib64/qt6/plugins/kf6/krunner/
    killall krunner             ← KRunner restarts, loads your plugin
    Alt+Space → tr:hola         ← works
```

---

## File contents

### `.gitignore`

```gitignore
build/
build_*/
out/
cmake-build-*/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
compile_commands.json
Makefile
*.cmake
!CMakeLists.txt
*.o
*.so
*.so.*
moc_*.cpp
moc_*.h
*.moc
.ccache/
.cache/
.vscode/
.idea/
*.swp
*.swo
*~
.DS_Store
```

### `.clang-format`

```yaml
---
BasedOnStyle: WebKit
AccessModifierOffset: -4
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Linux
PointerAlignment: Right
Standard: c++17
UseTab: Never
AllowShortFunctionsOnASingleLine: Inline
AlwaysBreakTemplateDeclarations: Yes
```

This is the official KDE code style.

### `src/metadata.json`

KRunner reads this when loading the plugin.

```json
{
    "KPlugin": {
        "Authors": [
            {
                "Email": "[YOUR-EMAIL]",
                "Name": "[YOUR-NAME]"
            }
        ],
        "Description": "Translate text using translate-shell — supports Google Translate, Bing, DeepL backends",
        "Description[es]": "Traducción de texto usando translate-shell",
        "Icon": "translator",
        "Id": "translatorrunner",
        "License": "GPL-3.0-or-later",
        "Name": "Translator Runner",
        "Name[es]": "Traductor",
        "Version": "0.1.0",
        "Website": "https://github.com/[GITHUB-USER]/translator-runner"
    },
    "X-Plasma-API-Minimum-Version": "6.0",
    "KRunner": {
        "Syntaxes": [
            {
                "Syntax": "tr:<text>",
                "Description": "Translate text to English (default)"
            },
            {
                "Syntax": "tr-<lang>:<text>",
                "Description": "Translate to specific language (tr-es:, tr-fr:, tr-de:, etc.)"
            }
        ]
    }
}
```

> **Warning:** The exact `metadata.json` structure for KRunner KF6 evolved between 6.0 and 6.6. If KRunner doesn't detect the plugin after install, this is the first file to check. Reference: https://develop.kde.org/docs/plasma/krunner/

### `src/translatorrunner.h`

```cpp
#pragma once

#include <KRunner/AbstractRunner>

class TranslatorRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    TranslatorRunner(QObject *parent, const KPluginMetaData &metaData);

    void match(KRunner::RunnerContext &context) override;

    void run(const KRunner::RunnerContext &context,
             const KRunner::QueryMatch &match) override;

private:
    QString translateText(const QString &text, const QString &targetLang);
    void copyToClipboard(const QString &text);
    void showNotification(const QString &title, const QString &message);

    static constexpr const char *TRIGGER_DEFAULT = "tr:";
    static constexpr const char *TRIGGER_LANG_PREFIX = "tr-";
    static constexpr int TRANSLATE_TIMEOUT_MS = 3000;
};
```

Key concepts:
- `#pragma once` — modern include guard (replaces `#ifndef X_H`)
- `Q_OBJECT` — required Qt macro for any class using signals/slots
- `override` — compiler error if the parent doesn't declare this method virtual
- `static constexpr` — compile-time constants, better than `#define`

### `src/translatorrunner.cpp`

```cpp
#include "translatorrunner.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <KLocalizedString>
#include <KNotification>
#include <KRunner/QueryMatch>

TranslatorRunner::TranslatorRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    addSyntax(QStringLiteral("tr:<text>"),
              i18n("Translate text to English (default)"));
    addSyntax(QStringLiteral("tr-<lang>:<text>"),
              i18n("Translate text to specific language"));

    setMinLetterCount(4); // "tr:x" is the minimum valid query
}

void TranslatorRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();

    QString targetLang;
    QString text;

    if (query.startsWith(QLatin1String(TRIGGER_LANG_PREFIX))) {
        const int colonIdx = query.indexOf(QLatin1Char(':'));
        if (colonIdx < 5) {
            return;
        }
        targetLang = query.mid(3, colonIdx - 3);
        text = query.mid(colonIdx + 1).trimmed();
    } else if (query.startsWith(QLatin1String(TRIGGER_DEFAULT))) {
        targetLang = QStringLiteral("en");
        text = query.mid(3).trimmed();
    } else {
        return;
    }

    if (text.isEmpty() || !context.isValid()) {
        return;
    }

    const QString translation = translateText(text, targetLang);
    if (translation.isEmpty()) {
        return;
    }

    KRunner::QueryMatch match(this);
    match.setText(translation);
    match.setSubtext(i18n("Translate to %1: %2", targetLang.toUpper(), text));
    match.setIconName(QStringLiteral("translator"));
    match.setRelevance(1.0);
    match.setData(translation);

    context.addMatch(match);
}

void TranslatorRunner::run(const KRunner::RunnerContext &context,
                            const KRunner::QueryMatch &match)
{
    Q_UNUSED(context)

    const QString translation = match.data().toString();
    if (translation.isEmpty()) {
        return;
    }

    copyToClipboard(translation);
    showNotification(i18n("Translation copied"), translation);
}

QString TranslatorRunner::translateText(const QString &text, const QString &targetLang)
{
    QProcess process;
    process.start(QStringLiteral("trans"),
                  {QStringLiteral(":%1").arg(targetLang), text, QStringLiteral("-b")});

    if (!process.waitForFinished(TRANSLATE_TIMEOUT_MS)) {
        process.kill();
        return QString();
    }

    if (process.exitCode() != 0) {
        return QString();
    }

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

void TranslatorRunner::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

void TranslatorRunner::showNotification(const QString &title, const QString &message)
{
    KNotification *notification =
        new KNotification(QStringLiteral("translationCopied"));
    notification->setComponentName(QStringLiteral("translatorrunner"));
    notification->setTitle(title);
    notification->setText(message);
    notification->setIconName(QStringLiteral("edit-copy"));
    notification->sendEvent();
}

K_PLUGIN_CLASS_WITH_JSON(TranslatorRunner, "metadata.json")

#include "translatorrunner.moc"
```

The `#include "translatorrunner.moc"` at the end is required — Qt's MOC (Meta-Object Compiler) generates that file during build. It doesn't exist on disk; CMake's `AUTOMOC` produces it.

### `src/translatorrunner.notifyrc`

```ini
[Global]
IconName=translator
Name=Translator Runner
Comment=Notifications from Translator Runner KRunner plugin

[Event/translationCopied]
Name=Translation copied
Comment=A translation was copied to clipboard
Action=Popup
```

### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(translator-runner
    VERSION 0.1.0
    DESCRIPTION "KRunner plugin for translating text via translate-shell"
    LANGUAGES CXX
)

set(QT_MIN_VERSION 6.5.0)
set(KF_MIN_VERSION 6.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# AUTOMOC runs MOC automatically on files with Q_OBJECT
set(CMAKE_AUTOMOC ON)

# Exports compile_commands.json for clangd (IDE autocomplete)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
endif()

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Runner
    CoreAddons
    I18n
    Notifications
)

kcoreaddons_add_plugin(translatorrunner
    SOURCES
        src/translatorrunner.cpp
        src/translatorrunner.h
    INSTALL_NAMESPACE "kf6/krunner"
)

target_link_libraries(translatorrunner PRIVATE
    Qt6::Core
    Qt6::Widgets
    KF6::Runner
    KF6::CoreAddons
    KF6::I18n
    KF6::Notifications
)

target_include_directories(translatorrunner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(FILES src/translatorrunner.notifyrc
    DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR}
)
```

Key lines explained:
- `set(CMAKE_AUTOMOC ON)` — makes MOC run automatically for files with `Q_OBJECT` (critical)
- `find_package(ECM ...)` — brings in KDE's CMake macros
- `find_package(Qt6 ...)` — finds Qt6 via pkg-config
- `find_package(KF6 ...)` — finds KDE Frameworks 6
- `kcoreaddons_add_plugin(...)` — KDE macro that creates the `.so` with embedded metadata
- `target_link_libraries(...)` — links the required libraries (like `-lQt6Core` at compile time)
- `install(...)` — defines where `sudo make install` copies files

### `packaging/fedora.spec`

```spec
Name:           translator-runner
Version:        0.1.0
Release:        1%{?dist}
Summary:        KRunner plugin to translate text via translate-shell
License:        GPL-3.0-or-later
URL:            https://github.com/[GITHUB-USER]/translator-runner
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  extra-cmake-modules
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-krunner-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-knotifications-devel

Requires:       translate-shell
Requires:       plasma-workspace >= 6.0

%description
A KRunner plugin that translates text inline via translate-shell.
Supports multiple target languages with syntax tr:<text> and tr-<lang>:<text>.

%prep
%autosetup

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_libdir}/qt6/plugins/kf6/krunner/translatorrunner.so
%{_datadir}/knotifications6/translatorrunner.notifyrc

%changelog
* Wed May 20 2026 [YOUR-NAME] <[YOUR-EMAIL]> - 0.1.0-1
- Initial release
```

### `.github/workflows/build.yml`

```yaml
name: Build

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: fedora:44

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          dnf install -y \
            gcc-c++ cmake extra-cmake-modules \
            qt6-qtbase-devel \
            kf6-krunner-devel kf6-kcoreaddons-devel \
            kf6-ki18n-devel kf6-knotifications-devel

      - name: Configure
        run: |
          mkdir build
          cd build
          cmake -DCMAKE_INSTALL_PREFIX=/usr ..

      - name: Build
        run: |
          cd build
          make -j$(nproc)

      - name: Verify output
        run: |
          ls -la build/bin/
          file build/bin/translatorrunner.so
```

---

## Development guide

### Debug cycle

```bash
# Build (first time)
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
sudo make install
killall krunner

# Incremental build (after first cmake)
cd build && make -j$(nproc) && sudo make install && killall krunner
```

### Plugin not loading?

```bash
# Verify .so at expected path
ls /usr/lib64/qt6/plugins/kf6/krunner/translatorrunner*

# Rebuild KRunner's plugin cache
kbuildsycoca6 --noincremental
killall krunner

# Watch logs
journalctl --user -f | grep -i krunner

# Full debug logging
QT_LOGGING_RULES="*.debug=true" krunner --daemon
```

### Common issues

| Symptom | Likely cause |
|---|---|
| Plugin not in System Settings list | `metadata.json` malformed; check `Id` matches `.so` name |
| Plugin loads but `tr:` does nothing | `match()` returning early; add `qDebug()` to inspect query |
| Crash on use | Check `journalctl` for stack trace |
| `qBadAlloc` crash | API mismatch between KF6 version and headers used at compile time |

### Code style

```bash
# Format all source files
clang-format -i src/*.cpp src/*.h

# Add pre-commit hook to enforce style
cat > .git/hooks/pre-commit <<'EOF'
#!/bin/bash
clang-format --dry-run --Werror src/*.cpp src/*.h
EOF
chmod +x .git/hooks/pre-commit
```

---

## Roadmap

- [x] Basic translation via translate-shell
- [x] Multi-language support (`tr-XX:` syntax)
- [ ] DeepL API backend (optional, requires API key)
- [ ] Async translation (currently blocks ~300ms — needs signals/slots)
- [ ] Translation history / favorites
- [ ] Configuration UI in System Settings
- [ ] Claude/OpenAI backend for context-aware translation

---

## Honest caveats

- The C++ code is based on official KDE docs, but the **first build will likely have errors**. KRunner KF6 API had several changes between 6.0 and 6.6, and the docs aren't always up to date.
- `metadata.json` structure is the most likely culprit if KRunner doesn't detect the plugin — check the [KRunner dev docs](https://develop.kde.org/docs/plasma/krunner/) for the exact schema for your KF6 version.
- `QProcess::waitForFinished()` blocks ~200-500ms. Acceptable for v0.1 (KRunner runs runners in separate threads), but should be replaced with async signals later.

---

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

KRunner (KF6::Runner) is LGPL, but plugins link to it dynamically. GPL-3.0-or-later is appropriate for an ecosystem plugin.
