// Source code editor: QScintilla when available, otherwise a QPlainTextEdit
// fallback with a line-number / breakpoint margin.
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include <QSet>
#include <QString>
#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QString;
class CFindBar;

#include <QColor>

// The shared code font (Fixedsys Excelsior 3.01 @12pt with fallbacks).
QFont CaslCodeFont();

// The theme-aware editor palette (dark system vs VC6-light classic).
class CEditorTheme {
public:
    QColor m_clrPaper, m_clrText;
    QColor m_clrMarginBack, m_clrMarginText;
    QColor m_clrSelectionBack, m_clrSelectionText;
    QColor m_clrCaretLine;
    QColor m_clrExecLine, m_clrExecText; // execution line band + its text
};
const CEditorTheme& EditorTheme();

// A read-only, syntax-colored CASL code snippet widget (used by the help
// dialog). Create via CCaslSnippet::Create(); the instance owns its widget.
class CCaslSnippet {
public:
    static CCaslSnippet* Create(QWidget* pParent = nullptr);
    virtual ~CCaslSnippet() = default;
    virtual QWidget* Widget() = 0;
    virtual void SetCode(const QString& strCode) = 0;
};

#ifdef CASL_HAVE_QSCINTILLA
#include <Qsci/qsciscintilla.h>
using CEditorBase = QsciScintilla;
#else
#include <QPlainTextEdit>
using CEditorBase = QPlainTextEdit;
#endif

class CCodeEditor : public CEditorBase {
    Q_OBJECT

public:
    explicit CCodeEditor(QWidget* pParent = nullptr);

    // tooltip provider: given the hovered word, returns the text to show (an
    // empty string suppresses the tooltip). Called on the UI thread.
    using TFnTooltipProvider = std::function<QString(const QString&)>;
    void SetTooltipProvider(TFnTooltipProvider fn) { m_fnTooltip = fn; }

    void SetSourceText(const QString& strText);
    QString GetSourceText() const;
    int CurrentLine() const;               // 1-based cursor line
    QString CurrentWord() const;           // word under the cursor (may be empty)
    void GoToLine(int nLine);              // 1-based
    void HighlightLine(int nLine);         // execution marker, -1 clears
    bool IsBreakpointSet(int nLine) const; // 1-based
    void ToggleBreakpoint(int nLine);
    // VS Code style find bar (Ctrl+F)
    void ShowFindBar();
    // ---- find-bar test hooks ------------------------------------------------
    int GetFindMatchCountForTest() const { return (int)m_arrFindMatches.size(); }
    int GetFindIndexForTest() const { return m_nFindCur; }
    CFindBar* GetFindBarForTest() const { return m_pFindBar; }

signals:
    void BreakpointsChanged();

#ifndef CASL_HAVE_QSCINTILLA
protected:
    void resizeEvent(QResizeEvent* pEvent) override;
    void mouseMoveEvent(QMouseEvent* pEvent) override;

private slots:
    void OnUpdateMarginWidth(int nNewBlockCount);
    void OnHighlightCurrentLine();
    void OnMarginClicked(const class QPoint& ptPos);

private:
    class CLineNumberArea;
    CLineNumberArea* m_pMarginArea = nullptr;
    int MarginWidth() const;
    void PaintMargin(CLineNumberArea* pArea, QPaintEvent* pEvent);
#else
protected:
    void resizeEvent(QResizeEvent* pEvent) override;

private:
#endif

private:
    QSet<int> m_setBreakLines; // 1-based source lines with a breakpoint
    int m_nExecLine = -1;      // highlighted execution line (1-based)
    TFnTooltipProvider m_fnTooltip; // register/symbol value lookup
    QString m_strLastHoverWord;     // avoid re-showing the same tooltip

    // ---- find bar (shared UI, backend-specific highlight) -------------
    CFindBar* m_pFindBar = nullptr;
    std::vector<std::pair<int, int>> m_arrFindMatches; // (pos, len)
    int m_nFindCur = -1;
    void PositionFindBar();
    void RerunFind(bool bFromCursor);
    void GotoFindMatch(int nIndex);
    void ApplyFindHighlights();
    void ClearFindHighlights();

#ifndef CASL_HAVE_QSCINTILLA
    // find matches merged into extra selections on the fallback backend
    class QList<class QTextEdit::ExtraSelection> m_arrFindSelections;
#endif

    void ShowHoverTooltip(const QString& strWord, const QPoint& ptGlobal);
};
