#include "translatorrunnerkcm.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(TranslatorRunnerKCM, "kcm_translatorrunner.json")

TranslatorRunnerKCM::TranslatorRunnerKCM(QObject *parent, const KPluginMetaData &metaData)
    : KCModule(parent, metaData)
{
    setupUi();
    load();
}

// ── UI setup ───────────────────────────────────────────────────────────────

void TranslatorRunnerKCM::setupUi()
{
    QWidget *w = widget();
    auto *mainLayout = new QVBoxLayout(w);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *tabs = new QTabWidget(w);
    mainLayout->addWidget(tabs);

    auto *settingsPage = new QWidget(tabs);
    setupSettingsTab(settingsPage);
    tabs->addTab(settingsPage, i18n("Settings"));

    auto *historyPage = new QWidget(tabs);
    setupHistoryTab(historyPage);
    tabs->addTab(historyPage, i18n("History"));

    // Refresh history list when the History tab is opened
    connect(tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (idx == 1) refreshHistoryList();
    });
}

void TranslatorRunnerKCM::setupSettingsTab(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto *group = new QGroupBox(i18n("Translation Settings"), parent);
    auto *form  = new QFormLayout(group);
    form->setSpacing(10);

    m_langCombo = new QComboBox(group);
    m_langCombo->addItem(i18n("English"),             QStringLiteral("en"));
    m_langCombo->addItem(i18n("Spanish"),             QStringLiteral("es"));
    m_langCombo->addItem(i18n("French"),              QStringLiteral("fr"));
    m_langCombo->addItem(i18n("German"),              QStringLiteral("de"));
    m_langCombo->addItem(i18n("Italian"),             QStringLiteral("it"));
    m_langCombo->addItem(i18n("Portuguese"),          QStringLiteral("pt"));
    m_langCombo->addItem(i18n("Russian"),             QStringLiteral("ru"));
    m_langCombo->addItem(i18n("Chinese Simplified"),  QStringLiteral("zh"));
    m_langCombo->addItem(i18n("Chinese Traditional"), QStringLiteral("zh-TW"));
    m_langCombo->addItem(i18n("Japanese"),            QStringLiteral("ja"));
    m_langCombo->addItem(i18n("Korean"),              QStringLiteral("ko"));
    m_langCombo->addItem(i18n("Arabic"),              QStringLiteral("ar"));
    m_langCombo->addItem(i18n("Hindi"),               QStringLiteral("hi"));
    m_langCombo->addItem(i18n("Dutch"),               QStringLiteral("nl"));
    m_langCombo->addItem(i18n("Polish"),              QStringLiteral("pl"));
    m_langCombo->addItem(i18n("Swedish"),             QStringLiteral("sv"));
    m_langCombo->addItem(i18n("Turkish"),             QStringLiteral("tr"));
    m_langCombo->addItem(i18n("Quechua"),             QStringLiteral("qu"));
    m_langCombo->addItem(i18n("Aymara"),              QStringLiteral("ay"));
    m_langCombo->addItem(i18n("Custom…"),             QStringLiteral("__custom__"));

    m_customLang = new QLineEdit(group);
    m_customLang->setPlaceholderText(QStringLiteral("e.g. uk, fi, pt-BR"));
    m_customLang->setMaxLength(10);
    m_customLang->setVisible(false);

    form->addRow(i18n("Default target language:"), m_langCombo);
    form->addRow(i18n("Custom language code:"),    m_customLang);

    auto *hint = new QLabel(
        i18n("Used when typing <b>tr:</b> without specifying a language."),
        group);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    form->addRow(hint);

    layout->addWidget(group);
    layout->addStretch();

    connect(m_langCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const bool isCustom = (m_langCombo->currentData().toString() == QLatin1String("__custom__"));
        m_customLang->setVisible(isCustom);
        setNeedsSave(true);
    });
    connect(m_customLang, &QLineEdit::textChanged, this, [this] {
        setNeedsSave(true);
    });
}

void TranslatorRunnerKCM::setupHistoryTab(QWidget *parent)
{
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(10);

    auto *label = new QLabel(i18n("Recent translations (up to %1 entries):", HISTORY_MAX), parent);
    layout->addWidget(label);

    m_historyList = new QListWidget(parent);
    m_historyList->setAlternatingRowColors(true);
    m_historyList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_historyList);

    auto *btnRow = new QHBoxLayout();

    m_deleteBtn = new QPushButton(i18n("Delete selected"), parent);
    m_deleteBtn->setEnabled(false);

    m_clearBtn = new QPushButton(i18n("Clear all history"), parent);

    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_clearBtn);
    layout->addLayout(btnRow);

    connect(m_historyList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_deleteBtn->setEnabled(!m_historyList->selectedItems().isEmpty());
    });

    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_historyList->currentRow();
        if (row < 0) return;

        QJsonArray history = loadHistory();
        if (row < history.size()) {
            history.removeAt(row);
            writeHistory(history);
            refreshHistoryList();
        }
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        const auto reply = QMessageBox::question(
            widget(),
            i18n("Clear History"),
            i18n("Delete all translation history?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            writeHistory(QJsonArray());
            refreshHistoryList();
        }
    });
}

// ── History helpers ────────────────────────────────────────────────────────

QString TranslatorRunnerKCM::historyFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/history.json");
}

QJsonArray TranslatorRunnerKCM::loadHistory() const
{
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly)) return QJsonArray();
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isArray() ? doc.array() : QJsonArray();
}

void TranslatorRunnerKCM::writeHistory(const QJsonArray &history) const
{
    const QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(history).toJson(QJsonDocument::Compact));
}

void TranslatorRunnerKCM::refreshHistoryList()
{
    m_historyList->clear();
    const QJsonArray history = loadHistory();

    if (history.isEmpty()) {
        auto *empty = new QListWidgetItem(i18n("No translations yet."), m_historyList);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }

    for (const auto &item : history) {
        const QJsonObject entry = item.toObject();
        const QString text   = entry[QStringLiteral("text")].toString();
        const QString lang   = entry[QStringLiteral("lang")].toString();
        const QString result = entry[QStringLiteral("result")].toString();
        const QString ts     = entry[QStringLiteral("timestamp")].toString();
        const QString date   = QDateTime::fromString(ts, Qt::ISODate)
                                   .toString(QStringLiteral("MMM d, hh:mm"));

        const QString display = QStringLiteral("%1 → %2  [%3]  ·  %4")
                                    .arg(text, result, lang.toUpper(), date);
        m_historyList->addItem(display);
    }
}

// ── KCModule overrides ─────────────────────────────────────────────────────

void TranslatorRunnerKCM::load()
{
    const auto cfg  = KSharedConfig::openConfig(QLatin1String(CONFIG_FILE));
    const auto grp  = cfg->group(QLatin1String(CONFIG_GROUP));
    const QString lang = grp.readEntry(CONFIG_KEY, QString::fromLatin1(DEFAULT_LANG));

    const int idx = m_langCombo->findData(lang);
    if (idx >= 0) {
        m_langCombo->setCurrentIndex(idx);
        m_customLang->setVisible(false);
    } else {
        m_langCombo->setCurrentIndex(m_langCombo->count() - 1);
        m_customLang->setText(lang);
        m_customLang->setVisible(true);
    }

    setNeedsSave(false);
}

void TranslatorRunnerKCM::save()
{
    const QString lang = currentLangCode();
    if (lang.isEmpty()) return;

    auto cfg = KSharedConfig::openConfig(QLatin1String(CONFIG_FILE));
    auto grp = cfg->group(QLatin1String(CONFIG_GROUP));
    grp.writeEntry(CONFIG_KEY, lang);
    cfg->sync();

    setNeedsSave(false);
}

void TranslatorRunnerKCM::defaults()
{
    const int idx = m_langCombo->findData(QLatin1String(DEFAULT_LANG));
    if (idx >= 0) m_langCombo->setCurrentIndex(idx);
    m_customLang->clear();
    m_customLang->setVisible(false);
    setNeedsSave(true);
}

QString TranslatorRunnerKCM::currentLangCode() const
{
    const QString data = m_langCombo->currentData().toString();
    if (data == QLatin1String("__custom__"))
        return m_customLang->text().trimmed().toLower();
    return data;
}

#include "translatorrunnerkcm.moc"
