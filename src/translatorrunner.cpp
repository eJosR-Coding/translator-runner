#include "translatorrunner.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KNotification>
#include <KSharedConfig>
#include <KRunner/QueryMatch>

// ── Static tables ──────────────────────────────────────────────────────────

const QHash<QString, QString> TranslatorRunner::LANG_ALIASES = {
    {QStringLiteral("cn"), QStringLiteral("zh")},
    {QStringLiteral("jp"), QStringLiteral("ja")},
    {QStringLiteral("br"), QStringLiteral("pt")},
    {QStringLiteral("kr"), QStringLiteral("ko")},
    {QStringLiteral("ua"), QStringLiteral("uk")},
    {QStringLiteral("gr"), QStringLiteral("el")},
};

const QHash<QString, QString> TranslatorRunner::MORSE_TABLE = {
    {QStringLiteral("a"), QStringLiteral(".-")},
    {QStringLiteral("b"), QStringLiteral("-...")},
    {QStringLiteral("c"), QStringLiteral("-.-.")},
    {QStringLiteral("d"), QStringLiteral("-..")},
    {QStringLiteral("e"), QStringLiteral(".")},
    {QStringLiteral("f"), QStringLiteral("..-.")},
    {QStringLiteral("g"), QStringLiteral("--.")},
    {QStringLiteral("h"), QStringLiteral("....")},
    {QStringLiteral("i"), QStringLiteral("..")},
    {QStringLiteral("j"), QStringLiteral(".---")},
    {QStringLiteral("k"), QStringLiteral("-.-")},
    {QStringLiteral("l"), QStringLiteral(".-..")},
    {QStringLiteral("m"), QStringLiteral("--")},
    {QStringLiteral("n"), QStringLiteral("-.")},
    {QStringLiteral("o"), QStringLiteral("---")},
    {QStringLiteral("p"), QStringLiteral(".--.")},
    {QStringLiteral("q"), QStringLiteral("--.-")},
    {QStringLiteral("r"), QStringLiteral(".-.")},
    {QStringLiteral("s"), QStringLiteral("...")},
    {QStringLiteral("t"), QStringLiteral("-")},
    {QStringLiteral("u"), QStringLiteral("..-")},
    {QStringLiteral("v"), QStringLiteral("...-")},
    {QStringLiteral("w"), QStringLiteral(".--")},
    {QStringLiteral("x"), QStringLiteral("-..-")},
    {QStringLiteral("y"), QStringLiteral("-.--")},
    {QStringLiteral("z"), QStringLiteral("--..")},
    {QStringLiteral("0"), QStringLiteral("-----")},
    {QStringLiteral("1"), QStringLiteral(".----")},
    {QStringLiteral("2"), QStringLiteral("..---")},
    {QStringLiteral("3"), QStringLiteral("...--")},
    {QStringLiteral("4"), QStringLiteral("....-")},
    {QStringLiteral("5"), QStringLiteral(".....")},
    {QStringLiteral("6"), QStringLiteral("-....")},
    {QStringLiteral("7"), QStringLiteral("--...")},
    {QStringLiteral("8"), QStringLiteral("---..")},
    {QStringLiteral("9"), QStringLiteral("----.")},
};

// ── Constructor ────────────────────────────────────────────────────────────

TranslatorRunner::TranslatorRunner(QObject *parent, const KPluginMetaData &metaData)
    : KRunner::AbstractRunner(parent, metaData)
{
    addSyntax(QStringLiteral("tr:<text>"),   i18n("Translate text to English"));
    addSyntax(QStringLiteral("tr-<lang>:<text>"), i18n("Translate to specific language"));
    addSyntax(QStringLiteral("fx-<mode>:<text>"), i18n("Local text transform (bin, hex, b64, morse, rev)"));
    addSyntax(QStringLiteral("fun-<mode>:<text>"), i18n("Fun text filter (uwu, cheems)"));

    setMinLetterCount(4);
}

// ── match() ────────────────────────────────────────────────────────────────

void TranslatorRunner::match(KRunner::RunnerContext &context)
{
    const QString query = context.query();

    if (!context.isValid()) {
        return;
    }

    // trh: → open history window
    if (query.startsWith(QLatin1String(TRIGGER_HISTORY))) {
        KRunner::QueryMatch match(this);
        match.setText(i18n("Translation History"));
        match.setSubtext(i18n("Open history viewer and settings"));
        match.setIconName(QStringLiteral("deep-history"));
        match.setRelevance(1.0);
        match.setData(QStringLiteral("open_history"));
        context.addMatch(match);
        return;
    }

    // fx-<mode>:<text>
    if (query.startsWith(QLatin1String(TRIGGER_FX))) {
        const int colonIdx = query.indexOf(QLatin1Char(':'));
        if (colonIdx < 4) return;
        const QString mode = query.mid(3, colonIdx - 3).toLower();
        const QString text = query.mid(colonIdx + 1).trimmed();
        if (!text.isEmpty()) matchTransform(context, mode, text);
        return;
    }

    // fun-<mode>:<text>
    if (query.startsWith(QLatin1String(TRIGGER_FUN))) {
        const int colonIdx = query.indexOf(QLatin1Char(':'));
        if (colonIdx < 5) return;
        const QString mode = query.mid(4, colonIdx - 4).toLower();
        const QString text = query.mid(colonIdx + 1).trimmed();
        if (!text.isEmpty()) matchFun(context, mode, text);
        return;
    }

    // tr-<lang>:<text>
    if (query.startsWith(QLatin1String(TRIGGER_TRANSLATE_LANG))) {
        const int colonIdx = query.indexOf(QLatin1Char(':'));
        if (colonIdx < 5) return;
        const QString rawLang = query.mid(3, colonIdx - 3);
        const QString lang = resolveLanguage(rawLang);
        if (lang.isEmpty()) return;
        const QString text = query.mid(colonIdx + 1).trimmed();
        if (!text.isEmpty()) matchTranslation(context, text, lang);
        return;
    }

    // tr:<text>
    if (query.startsWith(QLatin1String(TRIGGER_TRANSLATE))) {
        const QString text = query.mid(3).trimmed();
        if (!text.isEmpty()) {
            const auto cfg  = KSharedConfig::openConfig(QStringLiteral("translatorrunnerrc"));
            const auto grp  = cfg->group(QStringLiteral("General"));
            const QString defaultLang = grp.readEntry("DefaultLanguage", QStringLiteral("en"));
            matchTranslation(context, text, defaultLang);
        }
        return;
    }
}

// ── Translation ────────────────────────────────────────────────────────────

void TranslatorRunner::matchTranslation(KRunner::RunnerContext &context,
                                         const QString &text,
                                         const QString &targetLang)
{
    auto *process = new QProcess(this);

    QTimer::singleShot(TRANSLATE_TIMEOUT_MS, process, [process]() {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });

    connect(process, &QProcess::finished, this,
            [this, process, context, text, targetLang](int exitCode) mutable {
                process->deleteLater();
                if (exitCode != 0 || !context.isValid()) return;

                const QString translation =
                    QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (translation.isEmpty() || !context.isValid()) return;

                // Store original text + lang + result so run() can save to history on click
                QJsonObject info;
                info[QStringLiteral("result")] = translation;
                info[QStringLiteral("text")]   = text;
                info[QStringLiteral("lang")]   = targetLang;
                const QString data = QString::fromUtf8(
                    QJsonDocument(info).toJson(QJsonDocument::Compact));

                KRunner::QueryMatch match(this);
                match.setText(translation);
                match.setSubtext(i18n("Translate to %1: %2", targetLang.toUpper(), text));
                match.setIconName(QStringLiteral("translator"));
                match.setRelevance(1.0);
                match.setData(data);
                context.addMatch(match);
            });

    process->start(QStringLiteral("trans"),
                   {QStringLiteral(":%1").arg(targetLang), text, QStringLiteral("-b")});
}

QString TranslatorRunner::resolveLanguage(const QString &code) const
{
    const QString lower = code.toLower();
    if (LANG_ALIASES.contains(lower))
        return LANG_ALIASES.value(lower);

    static const QRegularExpression validCode(
        QStringLiteral("^[a-z]{2,3}(-[a-zA-Z]{2,4})?$"));
    if (!validCode.match(lower).hasMatch())
        return QString();

    return lower;
}

// ── Local transforms (fx-) ─────────────────────────────────────────────────

void TranslatorRunner::matchTransform(KRunner::RunnerContext &context,
                                       const QString &mode, const QString &text)
{
    QString result;
    QString label;

    if (mode == QLatin1String("bin")) {
        result = fxBinary(text);
        label = i18n("Binary: %1", text);
    } else if (mode == QLatin1String("hex")) {
        result = fxHex(text);
        label = i18n("Hex: %1", text);
    } else if (mode == QLatin1String("b64")) {
        result = fxBase64(text);
        label = i18n("Base64: %1", text);
    } else if (mode == QLatin1String("morse")) {
        result = fxMorse(text);
        label = i18n("Morse: %1", text);
    } else if (mode == QLatin1String("rev")) {
        result = fxReverse(text);
        label = i18n("Reversed: %1", text);
    } else {
        return;
    }

    if (result.isEmpty() || !context.isValid()) return;

    KRunner::QueryMatch match(this);
    match.setText(result);
    match.setSubtext(label);
    match.setIconName(QStringLiteral("accessories-text-editor"));
    match.setRelevance(1.0);
    match.setData(result);
    context.addMatch(match);
}

QString TranslatorRunner::fxBinary(const QString &text) const
{
    QStringList parts;
    for (const QChar &ch : text) {
        parts << QString::number(ch.unicode(), 2).rightJustified(8, QLatin1Char('0'));
    }
    return parts.join(QLatin1Char(' '));
}

QString TranslatorRunner::fxHex(const QString &text) const
{
    QStringList parts;
    for (const QChar &ch : text) {
        parts << QString::number(ch.unicode(), 16).toUpper().rightJustified(2, QLatin1Char('0'));
    }
    return parts.join(QLatin1Char(' '));
}

QString TranslatorRunner::fxBase64(const QString &text) const
{
    return QString::fromLatin1(text.toUtf8().toBase64());
}

QString TranslatorRunner::fxMorse(const QString &text) const
{
    QStringList parts;
    for (const QChar &ch : text.toLower()) {
        if (ch == QLatin1Char(' ')) {
            parts << QStringLiteral("/");
        } else {
            const QString key = QString(ch);
            if (MORSE_TABLE.contains(key))
                parts << MORSE_TABLE.value(key);
        }
    }
    return parts.join(QLatin1Char(' '));
}

QString TranslatorRunner::fxReverse(const QString &text) const
{
    QString result = text;
    std::reverse(result.begin(), result.end());
    return result;
}

// ── Fun filters (fun-) ─────────────────────────────────────────────────────

void TranslatorRunner::matchFun(KRunner::RunnerContext &context,
                                 const QString &mode, const QString &text)
{
    QString result;
    QString label;

    if (mode == QLatin1String("uwu")) {
        result = funUwu(text);
        label = i18n("UwU: %1", text);
    } else if (mode == QLatin1String("cheems")) {
        result = funCheems(text);
        label = i18n("Cheems: %1", text);
    } else {
        return;
    }

    if (result.isEmpty() || !context.isValid()) return;

    KRunner::QueryMatch match(this);
    match.setText(result);
    match.setSubtext(label);
    match.setIconName(QStringLiteral("face-smile"));
    match.setRelevance(1.0);
    match.setData(result);
    context.addMatch(match);
}

QString TranslatorRunner::funUwu(const QString &text) const
{
    QString result = text;
    result.replace(QStringLiteral("r"),  QStringLiteral("w"),  Qt::CaseInsensitive);
    result.replace(QStringLiteral("l"),  QStringLiteral("w"),  Qt::CaseInsensitive);
    result.replace(QStringLiteral("na"), QStringLiteral("nya"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("no"), QStringLiteral("nyo"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("ne"), QStringLiteral("nye"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("ni"), QStringLiteral("nyi"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("nu"), QStringLiteral("nyu"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("!"),  QStringLiteral("! owo"), Qt::CaseInsensitive);
    result.replace(QStringLiteral("?"),  QStringLiteral("? uwu"), Qt::CaseInsensitive);
    return result;
}

QString TranslatorRunner::funCheems(const QString &text) const
{
    // Regex: one or more vowels (incl. accented) followed by a consonant
    // that is NOT m, n, ñ, r, y — same logic as github.com/Xeroth-20/cheemsify
    static const QRegularExpression expr(
        QStringLiteral("[aáeéiíouúó]+"
                       "[^aáeéiíouúómnñry]"),
        QRegularExpression::CaseInsensitiveOption);

    static const QString VOWELS = QStringLiteral("aáeéiíoóuú");

    QStringList words = text.split(QLatin1Char(' '));
    QStringList result;
    result.reserve(words.size());

    for (const QString &word : words) {
        // Count uppercase vs lowercase vowels to decide 'm' or 'M'
        int upperVowels = 0, lowerVowels = 0;
        for (const QChar &ch : word) {
            if (VOWELS.contains(ch.toLower())) {
                ch.isUpper() ? ++upperVowels : ++lowerVowels;
            }
        }
        const QChar ins = (upperVowels > lowerVowels) ? QLatin1Char('M') : QLatin1Char('m');

        // Collect all matches, then apply right-to-left so indices stay valid
        QList<QRegularExpressionMatch> matches;
        QRegularExpressionMatchIterator it = expr.globalMatch(word);
        while (it.hasNext())
            matches.append(it.next());

        QString cheemsWord = word;
        for (int i = matches.size() - 1; i >= 0; --i) {
            const int insertPos = matches[i].capturedStart() + matches[i].capturedLength() - 1;
            cheemsWord.insert(insertPos, ins);
        }

        result.append(cheemsWord);
    }

    return result.join(QLatin1Char(' '));
}

// ── run() ──────────────────────────────────────────────────────────────────

void TranslatorRunner::run(const KRunner::RunnerContext &context,
                            const KRunner::QueryMatch &match)
{
    Q_UNUSED(context)

    const QString data = match.data().toString();
    if (data.isEmpty()) return;

    if (data == QLatin1String("open_history")) {
        QProcess::startDetached(QStringLiteral("translatorrunner-history"), {});
        return;
    }

    // Translation result stored as JSON — save to history and copy
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const QString result = obj[QStringLiteral("result")].toString();
        const QString text   = obj[QStringLiteral("text")].toString();
        const QString lang   = obj[QStringLiteral("lang")].toString();
        saveToHistory(text, lang, result);
        copyToClipboard(result);
        showNotification(i18n("Translation copied"), result);
        return;
    }

    // fx- and fun- results — just copy, no history
    copyToClipboard(data);
    showNotification(i18n("Copied to clipboard"), data);
}

void TranslatorRunner::saveToHistory(const QString &text,
                                      const QString &lang,
                                      const QString &result)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + QStringLiteral("/translatorrunner/history.json");
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray history;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray()) history = doc.array();
        file.close();
    }

    // Remove duplicate
    for (int i = 0; i < history.size(); ++i) {
        const QJsonObject e = history[i].toObject();
        if (e[QStringLiteral("text")].toString() == text &&
            e[QStringLiteral("lang")].toString() == lang) {
            history.removeAt(i);
            break;
        }
    }

    QJsonObject entry;
    entry[QStringLiteral("text")]      = text;
    entry[QStringLiteral("lang")]      = lang;
    entry[QStringLiteral("result")]    = result;
    entry[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray updated;
    updated.append(entry);
    for (const auto &item : std::as_const(history))
        updated.append(item);

    while (updated.size() > 50)
        updated.removeLast();

    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(updated).toJson(QJsonDocument::Compact));
}

void TranslatorRunner::copyToClipboard(const QString &text)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(text);
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
