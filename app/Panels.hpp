// Dock panels: registers, memory, symbols, console (input/output), errors.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include "casl/Assembler.hpp"
#include "casl/Machine.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>

// ---------------------------------------------------------------------------
// registers + flags
// ---------------------------------------------------------------------------
class CRegistersPanel : public QWidget {
    Q_OBJECT
public:
    explicit CRegistersPanel(QWidget* pParent = nullptr);
    void UpdateState(const casl::CMachineState& state);
    void Clear();

    // test hook: label text for register n (0..7 = GR0..GR7, 8 = SP, 9 = PR)
    QString GetRegisterTextForTest(int nIndex) const {
        return m_arrRegLabels[nIndex]->text();
    }

private:
    void AddRow(const QString& strName, int nRow);
    QLabel* m_arrRegLabels[10]; // GR0..GR7, SP, PR
    QLabel* m_plblOF = nullptr;
    QLabel* m_plblSF = nullptr;
    QLabel* m_plblZF = nullptr;
    QLabel* m_plblSteps = nullptr;
    QLabel* m_plblState = nullptr;
};

// ---------------------------------------------------------------------------
// memory viewer: 8x8 words starting at a base address
// ---------------------------------------------------------------------------
class CMemoryPanel : public QWidget {
    Q_OBJECT
public:
    explicit CMemoryPanel(QWidget* pParent = nullptr);
    void UpdateMemory(const std::vector<uint16_t>& arrWords,
                      uint16_t wHighlightAddr);
    void Clear();

signals:
    // the user edited the cell at nAddress (address of the word)
    void WordEditRequested(int nAddress, const QString& strText);

private:
    void Rebuild();
    QSpinBox* m_pspinBase = nullptr;
    QTableWidget* m_ptable = nullptr;
    int m_nBase = 0;
    uint16_t m_wHighlightAddr = 0xFFFF;
    bool m_bUpdating = false;
    static const int WORDS_PER_ROW = 8;
    static const int ROWS = 8;
};

// ---------------------------------------------------------------------------
// symbol table / watch window (VC6 style: name, address, live value; the
// value column is editable at runtime)
// ---------------------------------------------------------------------------
class CSymbolsPanel : public QWidget {
    Q_OBJECT
public:
    explicit CSymbolsPanel(QWidget* pParent = nullptr);
    // (re)builds the row set after assemble (name + address columns)
    void UpdateSymbols(const std::map<std::string, int>& mapSymbols);
    // refreshes the value columns from the latest memory snapshot
    void UpdateValues(const std::vector<uint16_t>& arrWords);

    // test hooks
    QString GetCellTextForTest(int nRow, int nCol) const;
    int GetRowCountForTest() const { return m_ptable->rowCount(); }
    void EditValueForTest(int nRow, const QString& strText); // simulate edit

signals:
    // the user edited the value cell of the symbol at nAddress
    void ValueEditRequested(int nAddress, const QString& strText);

private:
    QTableWidget* m_ptable = nullptr;
    bool m_bUpdating = false; // suppress itemChanged during refresh
};

// ---------------------------------------------------------------------------
// console: output text + input line
// ---------------------------------------------------------------------------
class CConsolePanel : public QWidget {
    Q_OBJECT
public:
    explicit CConsolePanel(QWidget* pParent = nullptr);
    void AppendOutput(const QString& strText);
    void ClearOutput();
    void MarkWaitingForInput(bool bWaiting);

signals:
    void InputSubmitted(const QString& strLine);

private:
    QPlainTextEdit* m_ptxtOutput = nullptr;
    QLineEdit* m_peditInput = nullptr;
    QPushButton* m_pbtnSend = nullptr;
    QLabel* m_plblHint = nullptr;
};

// ---------------------------------------------------------------------------
// assembler errors
// ---------------------------------------------------------------------------
class CErrorsPanel : public QWidget {
    Q_OBJECT
public:
    explicit CErrorsPanel(QWidget* pParent = nullptr);
    void UpdateErrors(const std::vector<casl::CAsmError>& arrErrors);

signals:
    void ErrorSelected(int nLine); // 1-based source line

private:
    QListWidget* m_plistErrors = nullptr;
};
