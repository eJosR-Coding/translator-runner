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
    void copyToClipboard(const QString &text);
    void showNotification(const QString &title, const QString &message);

    static constexpr const char *TRIGGER_DEFAULT = "tr:";
    static constexpr const char *TRIGGER_LANG_PREFIX = "tr-";
    static constexpr int TRANSLATE_TIMEOUT_MS = 3000;
};
