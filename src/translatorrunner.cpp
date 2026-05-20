#include "translatorrunner.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <KLocalizedString>
#include <KNotification>
#include <KRunner/QueryMatch>

const QHash<QString, QString> TranslatorRunner::LANG_ALIASES = {
    {QStringLiteral("cn"), QStringLiteral("zh")},   // Chinese (common shortcut)
    {QStringLiteral("jp"), QStringLiteral("ja")},   // Japanese (common shortcut)
    {QStringLiteral("br"), QStringLiteral("pt")},   // Brazilian Portuguese
    {QStringLiteral("kr"), QStringLiteral("ko")},   // Korean (common shortcut)
    {QStringLiteral("ua"), QStringLiteral("uk")},   // Ukrainian
    {QStringLiteral("gr"), QStringLiteral("el")},   // Greek
};

TranslatorRunner::TranslatorRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    addSyntax(QStringLiteral("tr:<text>"),
              i18n("Translate text to English (default)"));
    addSyntax(QStringLiteral("tr-<lang>:<text>"),
              i18n("Translate text to specific language"));

    setMinLetterCount(4);
}

QString TranslatorRunner::resolveLanguage(const QString &code) const
{
    const QString lower = code.toLower();

    // Resolve common aliases first
    if (LANG_ALIASES.contains(lower)) {
        return LANG_ALIASES.value(lower);
    }

    // Validate: accept ISO 639-1 (2 chars), ISO 639-2 (3 chars),
    // and regional variants like zh-TW, pt-BR, zh-Hans
    static const QRegularExpression validCode(
        QStringLiteral("^[a-z]{2,3}(-[a-zA-Z]{2,4})?$"));

    if (!validCode.match(lower).hasMatch()) {
        return QString();
    }

    return lower;
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
        const QString rawLang = query.mid(3, colonIdx - 3);
        targetLang = resolveLanguage(rawLang);

        if (targetLang.isEmpty()) {
            return;
        }

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

    auto *process = new QProcess(this);

    QTimer::singleShot(TRANSLATE_TIMEOUT_MS, process, [process]() {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });

    connect(process, &QProcess::finished, this,
            [this, process, context, text, targetLang](int exitCode) mutable {
                process->deleteLater();

                if (exitCode != 0 || !context.isValid()) {
                    return;
                }

                const QString translation =
                    QString::fromUtf8(process->readAllStandardOutput()).trimmed();

                if (translation.isEmpty() || !context.isValid()) {
                    return;
                }

                KRunner::QueryMatch match(this);
                match.setText(translation);
                match.setSubtext(i18n("Translate to %1: %2", targetLang.toUpper(), text));
                match.setIconName(QStringLiteral("translator"));
                match.setRelevance(1.0);
                match.setData(translation);

                context.addMatch(match);
            });

    process->start(QStringLiteral("trans"),
                   {QStringLiteral(":%1").arg(targetLang), text, QStringLiteral("-b")});
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
