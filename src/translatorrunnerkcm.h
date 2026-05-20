#pragma once

#include <KCModule>
#include <QComboBox>
#include <QLineEdit>

class TranslatorRunnerKCM : public KCModule
{
    Q_OBJECT

public:
    TranslatorRunnerKCM(QObject *parent, const KPluginMetaData &metaData);

    void load() override;
    void save() override;
    void defaults() override;

private:
    void setupUi();
    QString currentLangCode() const;

    QComboBox *m_langCombo = nullptr;
    QLineEdit *m_customLang = nullptr;

    static constexpr const char *DEFAULT_LANG = "en";
    static constexpr const char *CONFIG_FILE  = "translatorrunnerrc";
    static constexpr const char *CONFIG_GROUP = "General";
    static constexpr const char *CONFIG_KEY   = "DefaultLanguage";
};
