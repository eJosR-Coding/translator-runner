# Changelog

## [Unreleased]

## [0.4.0] - 2026-05-20
### Added
- Configuration UI in System Settings (default target language selector)
- Custom language code input for any ISO 639-1 code
- Main plugin reads default language from user config

## [0.3.0] - 2026-05-20
### Added
- Local text transforms: `fx-bin:`, `fx-hex:`, `fx-b64:`, `fx-morse:`, `fx-rev:`
- Fun filters: `fun-uwu:`, `fun-cheems:` (based on cheemsify algorithm)

## [0.2.0] - 2026-05-20
### Added
- Async translation via QProcess signals (non-blocking)
- Multi-script support: Cyrillic, CJK, Arabic, Hebrew, Thai, Devanagari and more
- Language aliases: `tr-cn:`, `tr-jp:`, `tr-br:`, `tr-kr:`
- Regional variants: `tr-zh-TW:`, `tr-pt-BR:`
- Quechua (`tr-qu:`) and Aymara (`tr-ay:`) support
- ISO 639-1 language code validation

## [0.1.0] - 2026-05-20
### Added
- Basic translation via translate-shell (`trans` CLI)
- Multi-language support (`tr-XX:` syntax)
- Clipboard copy on match selection
- KDE notification on copy
