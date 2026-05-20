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
    void copyToClipboard(const QString &text);
    void showNotification(const QString &title, const QString &message);
    QString resolveLanguage(const QString &code) const;

    static constexpr const char *TRIGGER_DEFAULT = "tr:";
    static constexpr const char *TRIGGER_LANG_PREFIX = "tr-";
    static constexpr int TRANSLATE_TIMEOUT_MS = 3000;

    // Common shortcuts that deviate from ISO 639-1
    static const QHash<QString, QString> LANG_ALIASES;
};
