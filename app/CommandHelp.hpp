// Non-modal CASL command help dialog: search box + command list + usage.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include <QDialog>

class QListWidget;
class QLineEdit;
class QTextBrowser;
class CCaslSnippet;

class CCommandHelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit CCommandHelpDialog(QWidget* pParent = nullptr);

    // Select the entry for a mnemonic; returns false when unknown.
    bool LocateCommand(const QString& strKeyword);

protected:
    void keyPressEvent(QKeyEvent* pEvent) override;

private slots:
    void OnSearchChanged(const QString& strText);
    void OnCommandSelected(int nRow);
    void OnExamSelected(int nRow);

private:
    void BuildCommandList(const QString& strFilter = QString());
    void ShowEntry(int nRow);
    void ShowExam(int nRow); // past-exam question display

    QLineEdit* m_peditSearch = nullptr;
    QListWidget* m_plistCommands = nullptr;
    QListWidget* m_plistExams = nullptr; // 历年真题
    QTextBrowser* m_ptxtDetail = nullptr;
    CCaslSnippet* m_pSnippet = nullptr; // colored example code editor
    // full names in list order (parallel to the static entry table order)
    QStringList m_arrNames;
    int m_nActiveRow = -1;
};
