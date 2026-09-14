// VS Code-style single-line find bar: [find input |Aa|ab] [N of M] [^][v] [x]
// Pure UI widget; matching/navigation logic lives in CCodeEditor.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QToolButton;

#include <QToolButton> // complete type for the inline test hooks

class CFindBar : public QWidget {
    Q_OBJECT

public:
    explicit CFindBar(QWidget* pParent = nullptr);

    bool IsCaseSensitive() const;
    bool IsWholeWord() const;

    // test hooks: programmatically flip the Aa / ab toggles
    void SetCaseSensitiveForTest(bool bOn) { m_pbtnAa->setChecked(bOn); }
    void SetWholeWordForTest(bool bOn) { m_pbtnAb->setChecked(bOn); }

signals:
    void FindTextChanged(const QString& strText);
    void OptionsChanged();
    void NextRequested();
    void PrevRequested();
    void CloseRequested();

public slots:
    void ShowAndFocus();
    void UpdateCount(int nCur, int nTotal); // nCur: 0-based, -1 = none

protected:
    bool eventFilter(QObject* pWatched, QEvent* pEvent) override;

private:
    QString MakeCountText(int nCur, int nTotal) const;

    QLineEdit* m_peditFind = nullptr;
    QToolButton* m_pbtnAa = nullptr; // case sensitive
    QToolButton* m_pbtnAb = nullptr; // whole word
    QLabel* m_plblCount = nullptr;
    QToolButton* m_pbtnPrev = nullptr;
    QToolButton* m_pbtnNext = nullptr;
    QToolButton* m_pbtnClose = nullptr;
};
