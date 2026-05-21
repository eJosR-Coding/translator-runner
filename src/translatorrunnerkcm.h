#pragma once

#include <KCModule>
#include <QComboBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

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
    void setupSettingsTab(QWidget *parent);
    void setupHistoryTab(QWidget *parent);
    QString currentLangCode() const;

    // History helpers
    QString historyFilePath() const;
    QJsonArray loadHistory() const;
    void writeHistory(const QJsonArray &history) const;
    void refreshHistoryList();

    // Settings tab
    QComboBox   *m_langCombo   = nullptr;
    QLineEdit   *m_customLang  = nullptr;

    // History tab
    QListWidget *m_historyList = nullptr;
    QPushButton *m_deleteBtn   = nullptr;
    QPushButton *m_clearBtn    = nullptr;

    static constexpr const char *DEFAULT_LANG  = "en";
    static constexpr const char *CONFIG_FILE   = "translatorrunnerrc";
    static constexpr const char *CONFIG_GROUP  = "General";
    static constexpr const char *CONFIG_KEY    = "DefaultLanguage";
    static constexpr int         HISTORY_MAX   = 50;
};
