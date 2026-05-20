#include "translatorrunner.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QProcess>
#include <QTimer>
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

    setMinLetterCount(4);
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

    auto *process = new QProcess(this);

    // Kill the process if it exceeds the timeout
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
