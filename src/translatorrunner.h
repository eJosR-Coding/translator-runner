#pragma once

#include <KRunner/AbstractRunner>
#include <QHash>

class TranslatorRunner : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    TranslatorRunner(QObject *parent, const KPluginMetaData &metaData);

    void match(KRunner::RunnerContext &context) override;

    void run(const KRunner::RunnerContext &context,
             const KRunner::QueryMatch &match) override;

private:
    // Translation
    void matchTranslation(KRunner::RunnerContext &context,
                          const QString &text, const QString &targetLang);
    QString resolveLanguage(const QString &code) const;

    // Local transforms (fx-)
    void matchTransform(KRunner::RunnerContext &context,
                        const QString &mode, const QString &text);
    QString fxBinary(const QString &text) const;
    QString fxHex(const QString &text) const;
    QString fxBase64(const QString &text) const;
    QString fxMorse(const QString &text) const;
    QString fxReverse(const QString &text) const;

    // Fun filters (fun-)
    void matchFun(KRunner::RunnerContext &context,
                  const QString &mode, const QString &text);
    QString funUwu(const QString &text) const;
    QString funCheems(const QString &text) const;

    // Clipboard + notifications
    void copyToClipboard(const QString &text);
    void showNotification(const QString &title, const QString &message);

    static constexpr const char *TRIGGER_TRANSLATE = "tr:";
    static constexpr const char *TRIGGER_TRANSLATE_LANG = "tr-";
    static constexpr const char *TRIGGER_FX = "fx-";
    static constexpr const char *TRIGGER_FUN = "fun-";
    static constexpr int TRANSLATE_TIMEOUT_MS = 3000;

    static const QHash<QString, QString> LANG_ALIASES;
    static const QHash<QString, QString> MORSE_TABLE;
};
