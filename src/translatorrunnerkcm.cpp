#include "translatorrunnerkcm.h"

#include <KLocalizedString>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KConfigGroup>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

K_PLUGIN_CLASS_WITH_JSON(TranslatorRunnerKCM, "kcm_translatorrunner.json")

TranslatorRunnerKCM::TranslatorRunnerKCM(QObject *parent, const KPluginMetaData &metaData)
    : KCModule(parent, metaData)
{
    setupUi();
    load();
}

void TranslatorRunnerKCM::setupUi()
{
    QWidget *w = widget();
    auto *mainLayout = new QVBoxLayout(w);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);

    // ── Default language group ──────────────────────────────────────────────
    auto *group = new QGroupBox(i18n("Translation Settings"), w);
    auto *form  = new QFormLayout(group);
    form->setSpacing(10);

    m_langCombo = new QComboBox(group);
    m_langCombo->addItem(i18n("English"),            QStringLiteral("en"));
    m_langCombo->addItem(i18n("Spanish"),            QStringLiteral("es"));
    m_langCombo->addItem(i18n("French"),             QStringLiteral("fr"));
    m_langCombo->addItem(i18n("German"),             QStringLiteral("de"));
    m_langCombo->addItem(i18n("Italian"),            QStringLiteral("it"));
    m_langCombo->addItem(i18n("Portuguese"),         QStringLiteral("pt"));
    m_langCombo->addItem(i18n("Russian"),            QStringLiteral("ru"));
    m_langCombo->addItem(i18n("Chinese Simplified"), QStringLiteral("zh"));
    m_langCombo->addItem(i18n("Chinese Traditional"),QStringLiteral("zh-TW"));
    m_langCombo->addItem(i18n("Japanese"),           QStringLiteral("ja"));
    m_langCombo->addItem(i18n("Korean"),             QStringLiteral("ko"));
    m_langCombo->addItem(i18n("Arabic"),             QStringLiteral("ar"));
    m_langCombo->addItem(i18n("Hindi"),              QStringLiteral("hi"));
    m_langCombo->addItem(i18n("Dutch"),              QStringLiteral("nl"));
    m_langCombo->addItem(i18n("Polish"),             QStringLiteral("pl"));
    m_langCombo->addItem(i18n("Swedish"),            QStringLiteral("sv"));
    m_langCombo->addItem(i18n("Turkish"),            QStringLiteral("tr"));
    m_langCombo->addItem(i18n("Quechua"),            QStringLiteral("qu"));
    m_langCombo->addItem(i18n("Aymara"),             QStringLiteral("ay"));
    m_langCombo->addItem(i18n("Custom…"),            QStringLiteral("__custom__"));

    m_customLang = new QLineEdit(group);
    m_customLang->setPlaceholderText(QStringLiteral("e.g. uk, fi, ko, pt-BR"));
    m_customLang->setMaxLength(10);
    m_customLang->setVisible(false);

    form->addRow(i18n("Default target language:"), m_langCombo);
    form->addRow(i18n("Custom language code:"),    m_customLang);

    auto *hint = new QLabel(
        i18n("Used when typing <b>tr:</b> without specifying a language. "
             "Any ISO 639-1 code is valid (e.g. <b>fr</b>, <b>pt-BR</b>, <b>zh-TW</b>)."),
        group);
    hint->setWordWrap(true);
    hint->setEnabled(false);
    form->addRow(hint);

    mainLayout->addWidget(group);
    mainLayout->addStretch();

    // Show/hide custom field based on combo selection
    connect(m_langCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        const bool isCustom = (m_langCombo->currentData().toString() == QLatin1String("__custom__"));
        m_customLang->setVisible(isCustom);
        setNeedsSave(true);
    });

    connect(m_customLang, &QLineEdit::textChanged, this, [this] {
        setNeedsSave(true);
    });
}

void TranslatorRunnerKCM::load()
{
    const auto cfg  = KSharedConfig::openConfig(QLatin1String(CONFIG_FILE));
    const auto grp  = cfg->group(QLatin1String(CONFIG_GROUP));
    const QString lang = grp.readEntry(CONFIG_KEY, QString::fromLatin1(DEFAULT_LANG));

    // Try to select from combo
    const int idx = m_langCombo->findData(lang);
    if (idx >= 0) {
        m_langCombo->setCurrentIndex(idx);
        m_customLang->setVisible(false);
    } else {
        // Custom code — select last item and fill the line edit
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
    if (data == QLatin1String("__custom__")) {
        return m_customLang->text().trimmed().toLower();
    }
    return data;
}

#include "translatorrunnerkcm.moc"
