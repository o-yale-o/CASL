#include "CodeEditor.hpp"

#include <QColor>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShortcut>
#include <QToolTip>

#include "FindBar.hpp"

#include <cctype>
#include <string>
#ifndef CASL_HAVE_QSCINTILLA
#include <QAbstractTextDocumentLayout>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTextBlock>
#include <QTextEdit>
#endif

// ---------------------------------------------------------------------------
// shared editor look: Fixedsys Excelsior 3.01 @ 12pt and a color scheme that
// follows the system theme:
//   dark system  -> classic terminal:  #1E1E1E paper, light gray text,
//                                       gray margins, VS-dark navy selection
//   light system -> VC6 classic:       white paper, black text, gray margins,
//                                       navy selection
// The current (caret) line band and the yellow execution line always keep
// text visible: on the yellow band the text is forced to black.
// ---------------------------------------------------------------------------
namespace {

QFont MakeEditorFont() {
    const QFontDatabase fontDb;
    const QString arrCandidates[] = {
        "Fixedsys Excelsior 3.01", "Fixedsys Excelsior", "Consolas",
    };
    QString strFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    for (const QString& strCand : arrCandidates)
        if (fontDb.families().contains(strCand)) {
            strFamily = strCand;
            break;
        }
    QFont fontEditor(strFamily);
    fontEditor.setPointSize(12);
    fontEditor.setFixedPitch(true);
    return fontEditor;
}

} // namespace

const CEditorTheme& EditorTheme() {
    static const CEditorTheme s_theme = [] {
        CEditorTheme t;
        const bool bDark =
            QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
        if (bDark) {
            t.m_clrPaper          = QColor(0x1E, 0x1E, 0x1E);  // VS dark paper
            t.m_clrText           = QColor(0xDC, 0xDC, 0xDC);  // light gray text
            t.m_clrMarginBack     = QColor(0x2D, 0x2D, 0x30);
            t.m_clrMarginText     = QColor(0x85, 0x85, 0x85);
            t.m_clrSelectionBack  = QColor(0x26, 0x4F, 0x78);  // VS dark selection
            t.m_clrSelectionText  = QColor(0xFF, 0xFF, 0xFF);
            t.m_clrCaretLine      = QColor(0x0F, 0x0F, 0x0F);  // darker band
            t.m_clrExecLine       = QColor(0xBF, 0xA8, 0x00);  // dark yellow
            t.m_clrExecText       = QColor(0x00, 0x00, 0x00);
        } else {
            t.m_clrPaper          = QColor(255, 255, 255);     // VC6 white paper
            t.m_clrText           = QColor(0, 0, 0);
            t.m_clrMarginBack     = QColor(192, 192, 192);     // classic gray
            t.m_clrMarginText     = QColor(0, 0, 0);
            t.m_clrSelectionBack  = QColor(0, 0, 128);         // classic navy
            t.m_clrSelectionText  = QColor(255, 255, 255);
            t.m_clrCaretLine      = QColor(0xE8, 0xE8, 0xE8);
            t.m_clrExecLine       = QColor(255, 227, 0);       // yellow band
            t.m_clrExecText       = QColor(0, 0, 0);
        }
        return t;
    }();
    return s_theme;
}

namespace {

const char* s_pszCaslKeywords =
    "START END DS DC IN OUT EXIT NOP LD ST ADDA ADDL SUBA SUBL AND OR XOR "
    "CPA CPL SLA SRA SLL SRL JUMP JPL JMI JNZ JZE JOV PUSH POP CALL RET SVC";

bool IsCaslKeywordShared(const QByteArray& arrWord) {
    const char* arrKeywords[] = {
        "START", "END", "DS", "DC", "IN", "OUT", "EXIT", "NOP", "LD", "ST",
        "ADDA", "ADDL", "SUBA", "SUBL", "AND", "OR", "XOR", "CPA", "CPL",
        "SLA", "SRA", "SLL", "SRL", "JUMP", "JPL", "JMI", "JNZ", "JZE",
        "JOV", "PUSH", "POP", "CALL", "RET", "SVC"};
    for (const char* pszKw : arrKeywords)
        if (arrWord == pszKw) return true;
    return false;
}

} // namespace

#ifdef CASL_HAVE_QSCINTILLA
// ---------------------------------------------------------------------------
// CASL II syntax highlighting (QScintilla custom lexer)
// style 0 default, 1 comment, 2 keyword, 3 string, 4 number, 5 register
// ---------------------------------------------------------------------------
#include <Qsci/qscilexercustom.h>
#include <Qsci/qsciscintillabase.h>

class CCaslLexer : public QsciLexerCustom {
public:
    explicit CCaslLexer(QObject* pParent) : QsciLexerCustom(pParent) {
        const CEditorTheme& th = EditorTheme();
        const bool bDark = th.m_clrPaper.lightness() < 128;
        // Visual Assist C++ palette: keywords pure blue, comments green
        // italic, strings red-brown, numbers blue; dark variants follow VA on
        // dark backgrounds.
        setPaper(th.m_clrPaper, -1);   // default
        setColor(th.m_clrText, 0);
        setColor(bDark ? QColor(0x6A, 0x99, 0x55)      // comment (italic)
                       : QColor(0x00, 0x80, 0x00), 1);
        QFont fontItalic;
        fontItalic.setItalic(true);
        setFont(fontItalic, 1);
        setColor(bDark ? QColor(0x56, 0x9C, 0xD6)      // keyword
                       : QColor(0x00, 0x00, 0xFF), 2);
        setColor(bDark ? QColor(0xCE, 0x91, 0x78)      // string
                       : QColor(0xA3, 0x15, 0x15), 3);
        setColor(bDark ? QColor(0xB5, 0xCE, 0xA8)      // number
                       : QColor(0x00, 0x00, 0xFF), 4);
        setColor(bDark ? QColor(0x4E, 0xC9, 0xB0)      // register GR0..GR7
                       : QColor(0x00, 0x80, 0x80), 5);
    }

    const char* language() const override { return "CASL"; }
    const char* keywords(int) const override { return s_pszCaslKeywords; }
    QString description(int) const override { return QStringLiteral("CASL"); }

    void styleText(int nStart, int nEnd) override {
        if (!editor()) return;
        const int nLen = nEnd - nStart;
        if (nLen <= 0) return;
        std::string strBuf(nLen + 1, '\0');
        editor()->SendScintilla(QsciScintillaBase::SCI_GETTEXTRANGE, nStart,
                                nEnd, strBuf.data());

        startStyling(nStart);
        const char* pch = strBuf.data();
        int i = 0;
        while (i < nLen) {
            char ch = pch[i];
            if (ch == ';') {
                // comment to end of line
                int j = i;
                while (j < nLen && pch[j] != '\n') ++j;
                setStyling(j - i, 1);
                i = j;
            } else if (ch == '\'') {
                // string constant ('' escapes a quote)
                int j = i + 1;
                while (j < nLen) {
                    if (pch[j] == '\'') {
                        if (j + 1 < nLen && pch[j + 1] == '\'') j += 2;
                        else { ++j; break; }
                    } else if (pch[j] == '\n') {
                        break; // unterminated: stop at EOL
                    } else {
                        ++j;
                    }
                }
                setStyling(j - i, 3);
                i = j;
            } else if (ch == '#') {
                int j = i + 1;
                while (j < nLen && std::isxdigit((unsigned char)pch[j])) ++j;
                setStyling(j - i, 4);
                i = j;
            } else if (std::isdigit((unsigned char)ch) ||
                       ((ch == '+' || ch == '-') && i + 1 < nLen &&
                        std::isdigit((unsigned char)pch[i + 1]))) {
                int j = i + 1;
                while (j < nLen && std::isdigit((unsigned char)pch[j])) ++j;
                setStyling(j - i, 4);
                i = j;
            } else if (std::isalpha((unsigned char)ch) || ch == '_') {
                QByteArray arrWord;
                int j = i;
                while (j < nLen &&
                       (std::isalnum((unsigned char)pch[j]) || pch[j] == '_')) {
                    arrWord += pch[j];
                    ++j;
                }
                QByteArray arrUpper = arrWord.toUpper();
                int nStyle = 0;
                if (IsCaslKeywordShared(arrUpper)) nStyle = 2;
                else if (arrUpper.size() == 3 && arrUpper.startsWith("GR") &&
                         arrUpper[2] >= '0' && arrUpper[2] <= '7')
                    nStyle = 5; // register
                setStyling(j - i, nStyle);
                i = j;
            } else {
                setStyling(1, 0);
                ++i;
            }
        }
    }
};

#endif // CASL_HAVE_QSCINTILLA

QFont CaslCodeFont() {
    return MakeEditorFont();
}

// ---------------------------------------------------------------------------
// CCaslSnippet: read-only colored CASL code snippet (shared factory)
// ---------------------------------------------------------------------------
namespace {

#ifdef CASL_HAVE_QSCINTILLA
// read-only display configuration for a snippet editor
void SetupSnippet(QsciScintilla* pEdit) {
    const CEditorTheme& th = EditorTheme();
    QFont fontEditor = MakeEditorFont();
    pEdit->setFont(fontEditor);
    pEdit->setReadOnly(true);
    pEdit->setPaper(th.m_clrPaper);
    pEdit->setColor(th.m_clrText);
    pEdit->setSelectionForegroundColor(th.m_clrSelectionText);
    pEdit->setSelectionBackgroundColor(th.m_clrSelectionBack);
    pEdit->setCaretForegroundColor(th.m_clrText);
    pEdit->setMarginsBackgroundColor(th.m_clrMarginBack);
    pEdit->setMarginsForegroundColor(th.m_clrMarginText);
    pEdit->setMarginLineNumbers(0, false);
    pEdit->setMarginWidth(0, 0);
    pEdit->setMarginWidth(1, 0);
    pEdit->setMarginWidth(2, 0);
    pEdit->setTabWidth(4);
    pEdit->setEdgeMode(QsciScintilla::EdgeNone);
    pEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

class CCaslSnippetQsci : public CCaslSnippet {
public:
    explicit CCaslSnippetQsci(QWidget* pParent) {
        m_pEdit = new QsciScintilla(pParent);
        SetupSnippet(m_pEdit);
        CCaslLexer* pLexer = new CCaslLexer(m_pEdit);
        pLexer->setFont(MakeEditorFont());
        m_pEdit->setLexer(pLexer);
    }
    QWidget* Widget() override { return m_pEdit; }
    void SetCode(const QString& strCode) override { m_pEdit->setText(strCode); }

private:
    QsciScintilla* m_pEdit = nullptr;
};
#else
void SetupSnippet(QPlainTextEdit* pEdit) {
    const CEditorTheme& th = EditorTheme();
    pEdit->setFont(MakeEditorFont());
    pEdit->setReadOnly(true);
    pEdit->setWordWrapMode(QTextOption::NoWrap);
    pEdit->setStyleSheet(QString(
        "QPlainTextEdit { color: %1; background-color: %2; selection-color: %3;"
        " selection-background-color: %4; border: none; }")
        .arg(th.m_clrText.name(), th.m_clrPaper.name(),
             th.m_clrSelectionText.name(), th.m_clrSelectionBack.name()));
}

class CCaslSnippetPlain : public CCaslSnippet {
public:
    explicit CCaslSnippetPlain(QWidget* pParent) {
        m_pEdit = new QPlainTextEdit(pParent);
        SetupSnippet(m_pEdit);
        m_pHighlighter = new CCaslHighlighter(m_pEdit->document());
    }
    QWidget* Widget() override { return m_pEdit; }
    void SetCode(const QString& strCode) override {
        m_pEdit->setPlainText(strCode);
    }

private:
    QPlainTextEdit* m_pEdit = nullptr;
    CCaslHighlighter* m_pHighlighter = nullptr;
};
#endif

} // namespace

CCaslSnippet* CCaslSnippet::Create(QWidget* pParent) {
#ifdef CASL_HAVE_QSCINTILLA
    return new CCaslSnippetQsci(pParent);
#else
    return new CCaslSnippetPlain(pParent);
#endif
}

// ---------------------------------------------------------------------------
// fallback implementation: QPlainTextEdit + custom margin widget
// ---------------------------------------------------------------------------
#ifndef CASL_HAVE_QSCINTILLA

// ---------------------------------------------------------------------------
// CASL II syntax highlighting (QSyntaxHighlighter fallback)
// ---------------------------------------------------------------------------
#include <QSyntaxHighlighter>

class CCaslHighlighter : public QSyntaxHighlighter {
public:
    explicit CCaslHighlighter(QTextDocument* pDoc) : QSyntaxHighlighter(pDoc) {
        const CEditorTheme& th = EditorTheme();
        const bool bDark = th.m_clrPaper.lightness() < 128;
        auto MakeFmt = [](const QColor& clr, bool bItalic = false) {
            QTextCharFormat fmt;
            fmt.setForeground(QBrush(clr));
            if (bItalic) fmt.setFontItalic(true);
            return fmt;
        };
        // Visual Assist C++ palette (keyword/string/number/comment)
        m_fmtKeyword = MakeFmt(bDark ? QColor(0x56, 0x9C, 0xD6)
                                     : QColor(0x00, 0x00, 0xFF));
        m_fmtComment = MakeFmt(bDark ? QColor(0x6A, 0x99, 0x55)
                                     : QColor(0x00, 0x80, 0x00), true);
        m_fmtString = MakeFmt(bDark ? QColor(0xCE, 0x91, 0x78)
                                    : QColor(0xA3, 0x15, 0x15));
        m_fmtNumber = MakeFmt(bDark ? QColor(0xB5, 0xCE, 0xA8)
                                    : QColor(0x00, 0x00, 0xFF));
        m_fmtRegister = MakeFmt(bDark ? QColor(0x4E, 0xC9, 0xB0)
                                      : QColor(0x00, 0x80, 0x80));
    }

protected:
    void highlightBlock(const QString& strText) override {
        const QRegularExpression reComment(";[^\n]*");
        const QRegularExpression reString("'(?:[^']|'')*'");
        const QRegularExpression reWord("[A-Za-z_][A-Za-z_0-9]*");

        // comment spans the rest of the line
        auto mtComment = reComment.match(strText);
        if (mtComment.hasMatch()) {
            setFormat(mtComment.capturedStart(), mtComment.capturedLength(),
                      m_fmtComment);
        }

        auto mt = reWord.globalMatch(strText);
        while (mt.hasNext()) {
            auto m2 = mt.next();
            const QString strWord = m2.captured().toUpper();
            int nLen = m2.capturedLength();
            if (strWord.size() == 3 && strWord.startsWith("GR") &&
                strWord[2] >= '0' && strWord[2] <= '7') {
                setFormat(m2.capturedStart(), nLen, m_fmtRegister);
            } else {
                QByteArray arrWord = strWord.toUtf8();
                if (IsCaslKeywordShared(arrWord))
                    setFormat(m2.capturedStart(), nLen, m_fmtKeyword);
            }
        }

        // numbers: decimal, +/-decimal, #hex, +#hex, -#hex; word boundaries
        // prevent coloring digits inside labels
        static const QRegularExpression reNumber(
            "(?<![A-Za-z_0-9])[+-]?(?:#[0-9A-Fa-f]+|[0-9]+)"
            "(?![A-Za-z_0-9#])");
        mt = reNumber.globalMatch(strText);
        while (mt.hasNext()) {
            auto m2 = mt.next();
            setFormat(m2.capturedStart(), m2.capturedLength(), m_fmtNumber);
        }

        mt = reString.globalMatch(strText);
        while (mt.hasNext()) {
            auto m2 = mt.next();
            setFormat(m2.capturedStart(), m2.capturedLength(), m_fmtString);
        }
    }

private:
    QTextCharFormat m_fmtKeyword, m_fmtComment, m_fmtString, m_fmtNumber,
        m_fmtRegister;
};

class CCodeEditor::CLineNumberArea : public QWidget {
public:
    explicit CLineNumberArea(CCodeEditor* pEditor)
        : QWidget(pEditor), m_pEditor(pEditor) {}

    QSize sizeHint() const override { return QSize(m_pEditor->MarginWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* pEvent) override {
        m_pEditor->PaintMargin(this, pEvent);
    }
    void mousePressEvent(QMouseEvent* pEvent) override {
        // click in margin toggles a breakpoint on that line
        QTextBlock block =
            m_pEditor->cursorForPosition(QPoint(0, pEvent->pos().y())).block();
        if (block.isValid()) m_pEditor->ToggleBreakpoint(block.blockNumber() + 1);
        QWidget::mousePressEvent(pEvent);
    }

private:
    CCodeEditor* m_pEditor;
};

CCodeEditor::CCodeEditor(QWidget* pParent) : CEditorBase(pParent) {
    QFont fontEditor = MakeEditorFont();
    setFont(fontEditor);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(4 * QFontMetricsF(fontEditor).horizontalAdvance(' '));

    // ---- theme-aware color scheme (explicit, never palette-derived) ----
    const CEditorTheme& th = EditorTheme();
    setStyleSheet(QString(
        "QPlainTextEdit { color: %1; background-color: %2; selection-color: %3;"
        " selection-background-color: %4; }")
        .arg(th.m_clrText.name(), th.m_clrPaper.name(),
             th.m_clrSelectionText.name(), th.m_clrSelectionBack.name()));

    m_pMarginArea = new CLineNumberArea(this);
    new CCaslHighlighter(document()); // parented to the document
    connect(this, &QPlainTextEdit::blockCountChanged, this,
            &CCodeEditor::OnUpdateMarginWidth);
    connect(this, &QPlainTextEdit::updateRequest, this,
            [this](const QRect& rcRect, int) {
                m_pMarginArea->update(0, rcRect.y(), m_pMarginArea->width(),
                                      rcRect.height());
            });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this,
            &CCodeEditor::OnHighlightCurrentLine);
    OnUpdateMarginWidth(0);
    OnHighlightCurrentLine();
}

void CCodeEditor::OnUpdateMarginWidth(int) {
    setViewportMargins(MarginWidth(), 0, 0, 0);
}

int CCodeEditor::MarginWidth() const {
    int nDigits = 1;
    int nMax = qMax(1, blockCount());
    while (nMax >= 10) {
        nMax /= 10;
        ++nDigits;
    }
    // 18px breakpoint gutter + number + padding
    return 18 + nDigits * fontMetrics().horizontalAdvance('9') + 12;
}

void CCodeEditor::resizeEvent(QResizeEvent* pEvent) {
    QPlainTextEdit::resizeEvent(pEvent);
    QRect rcContents = contentsRect();
    m_pMarginArea->setGeometry(rcContents.left(), rcContents.top(),
                               MarginWidth(), rcContents.height());
    PositionFindBar();
}

void CCodeEditor::OnHighlightCurrentLine() {
    const CEditorTheme& th = EditorTheme();
    QList<QTextEdit::ExtraSelection> arrSelections;
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(th.m_clrCaretLine);
        sel.format.setForeground(th.m_clrText);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        arrSelections.append(sel);
    }
    // find-bar matches join the extra selections (kept separately)
    arrSelections.append(m_arrFindSelections);
    if (m_nExecLine > 0) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(th.m_clrExecLine);
        sel.format.setForeground(th.m_clrExecText); // black text on yellow
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        QTextCursor cur = textCursor();
        int nBlock = m_nExecLine - 1;
        if (nBlock < blockCount()) {
            cur.setPosition(document()->findBlockByNumber(nBlock).position());
            sel.cursor = cur;
            arrSelections.append(sel);
        }
    }
    setExtraSelections(arrSelections);
}

void CCodeEditor::PaintMargin(CLineNumberArea* pArea, QPaintEvent* pEvent) {
    const CEditorTheme& th = EditorTheme();
    QPainter painter(pArea);
    painter.fillRect(pEvent->rect(), th.m_clrMarginBack);

    QTextBlock block = firstVisibleBlock();
    int nBlockNumber = block.blockNumber();
    int nTop = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int nBottom = nTop + qRound(blockBoundingRect(block).height());
    const int nMarkerCenter = 10;

    while (block.isValid() && nTop <= pEvent->rect().bottom()) {
        if (block.isVisible() && nBottom >= pEvent->rect().top()) {
            int nLine = nBlockNumber + 1;
            // breakpoint marker
            if (m_setBreakLines.contains(nLine)) {
                painter.setBrush(QBrush(QColor(200, 30, 30)));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(3, nTop + fontMetrics().height() / 2 - 5,
                                    nMarkerCenter, nMarkerCenter);
            }
            // execution arrow
            painter.setPen(nLine == m_nExecLine ? QColor(255, 227, 0)
                                               : th.m_clrMarginText);
            painter.drawText(20, nTop, pArea->width() - 20,
                             fontMetrics().height(), Qt::AlignLeft,
                             QString::number(nLine));
        }
        block = block.next();
        nTop = nBottom;
        nBottom = nTop + qRound(blockBoundingRect(block).height());
        ++nBlockNumber;
    }
}

void CCodeEditor::OnMarginClicked(const QPoint&) {}

void CCodeEditor::mouseMoveEvent(QMouseEvent* pEvent) {
    QPlainTextEdit::mouseMoveEvent(pEvent);
    if (m_fnTooltip) {
        QTextCursor cur = cursorForPosition(pEvent->pos());
        cur.select(QTextCursor::WordUnderCursor);
        ShowHoverTooltip(cur.selectedText(), pEvent->globalPos());
    }
}

#endif // !CASL_HAVE_QSCINTILLA

// ---------------------------------------------------------------------------
// QScintilla implementation
// ---------------------------------------------------------------------------
#ifdef CASL_HAVE_QSCINTILLA

CCodeEditor::CCodeEditor(QWidget* pParent) : CEditorBase(pParent) {
    QFont fontEditor = MakeEditorFont();
    setFont(fontEditor);
    setTabWidth(4);
    setIndentationsUseTabs(false);
    setAutoIndent(true);
    setBraceMatching(QsciScintilla::SloppyBraceMatch);
    setUnmatchedBraceForegroundColor(QColor(160, 0, 0));

    // ---- theme-aware color scheme (explicit, never palette-derived) ----
    const CEditorTheme& th = EditorTheme();
    const bool bDarkTheme = th.m_clrPaper.lightness() < 128;
    setPaper(th.m_clrPaper);           // default background
    setColor(th.m_clrText);            // default foreground
    setSelectionForegroundColor(th.m_clrSelectionText);
    setSelectionBackgroundColor(th.m_clrSelectionBack);
    setCaretForegroundColor(th.m_clrText);
    setCaretLineVisible(true);
    setCaretLineBackgroundColor(th.m_clrCaretLine);
    setMarginsBackgroundColor(th.m_clrMarginBack);
    setMarginsForegroundColor(th.m_clrMarginText);
    setEdgeMode(QsciScintilla::EdgeNone);

    // margin 1: line numbers; margin 2: breakpoint / execution markers.
    // Both margins are click-sensitive: clicking the line number or the
    // marker gutter toggles a breakpoint on that line (VS style).
    setMarginLineNumbers(1, true);
    setMarginType(2, QsciScintilla::SymbolMargin);
    setMarginWidth(2, 20);
    setMarginSensitivity(1, true);
    setMarginSensitivity(2, true);
    setMarginsFont(fontEditor);
    markerDefine(QsciScintilla::Circle, 0);    // breakpoint
    setMarkerBackgroundColor(QColor(200, 30, 30), 0);
    setMarkerForegroundColor(QColor(255, 255, 255), 0);
    markerDefine(QsciScintilla::RightArrow, 1); // execution line
    setMarkerBackgroundColor(QColor(255, 227, 0), 1);
    setMarkerForegroundColor(QColor(0, 0, 0), 1);
    // CASL syntax coloring (keywords / comments / strings / numbers / GRx)
    CCaslLexer* pLexer = new CCaslLexer(this);
    pLexer->setFont(fontEditor);
    setLexer(pLexer);
    connect(this, &QsciScintilla::marginClicked, this,
            [this](int, int nLine, Qt::KeyboardModifiers) {
                ToggleBreakpoint(nLine + 1);
            });
    // register / symbol value tooltips after a short hover
    SendScintilla(QsciScintillaBase::SCI_SETMOUSEDWELLTIME, 350);
    connect(this, &QsciScintillaBase::SCN_DWELLSTART, this,
            [this](int, int x, int y) {
                ShowHoverTooltip(wordAtPoint(QPoint(x, y)),
                                 mapToGlobal(QPoint(x, y)));
            });
    connect(this, &QsciScintillaBase::SCN_DWELLEND, this, [this](int, int, int) {
        m_strLastHoverWord.clear();
        QToolTip::hideText();
    });

    // find-bar indicators: 0 = all matches, 1 = current match (filled box)
    indicatorDefine(QsciScintilla::FullBoxIndicator, 0);
    setIndicatorForegroundColor(
        bDarkTheme ? QColor(0x6E, 0x9E, 0xDE) : QColor(0xE8, 0xB4, 0x5A), 0);
    SendScintilla(QsciScintillaBase::SCI_INDICSETALPHA, 0, 70);
    setIndicatorDrawUnder(true, 0);
    indicatorDefine(QsciScintilla::FullBoxIndicator, 1);
    setIndicatorForegroundColor(
        bDarkTheme ? QColor(0x3A, 0x82, 0xD8) : QColor(0xF5, 0xA6, 0x23), 1);
    SendScintilla(QsciScintillaBase::SCI_INDICSETALPHA, 1, 170);
    setIndicatorDrawUnder(true, 1);
}

void CCodeEditor::resizeEvent(QResizeEvent* pEvent) {
    QsciScintilla::resizeEvent(pEvent);
    PositionFindBar();
}

#endif

// ---------------------------------------------------------------------------
// common API
// ---------------------------------------------------------------------------

void CCodeEditor::ShowHoverTooltip(const QString& strWord,
                                   const QPoint& ptGlobal) {
    if (!m_fnTooltip) return;
    // suppress flicker: same word -> tooltip stays as-is (Qt keeps it shown)
    if (strWord == m_strLastHoverWord) return;
    m_strLastHoverWord = strWord;
    const QString strText = strWord.isEmpty() ? QString() : m_fnTooltip(strWord);
    if (strText.isEmpty()) {
        QToolTip::hideText();
        return;
    }
    QToolTip::showText(ptGlobal, strText, this);
}

// ---------------------------------------------------------------------------
// find bar (VS Code style)
// ---------------------------------------------------------------------------

namespace {

// whole-word boundary test for a haystack character
template <typename TChar>
bool IsWordCharT(const TChar& ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

} // namespace

void CCodeEditor::ShowFindBar() {
    if (!m_pFindBar) {
        m_pFindBar = new CFindBar(this);
        connect(m_pFindBar, &CFindBar::FindTextChanged, this,
                [this](const QString&) { RerunFind(true); });
        connect(m_pFindBar, &CFindBar::OptionsChanged, this,
                [this] { RerunFind(true); });
        connect(m_pFindBar, &CFindBar::NextRequested, this, [this] {
            if (m_arrFindMatches.empty()) return;
            GotoFindMatch((m_nFindCur + 1) % (int)m_arrFindMatches.size());
        });
        connect(m_pFindBar, &CFindBar::PrevRequested, this, [this] {
            if (m_arrFindMatches.empty()) return;
            GotoFindMatch(m_nFindCur <= 0
                              ? (int)m_arrFindMatches.size() - 1
                              : m_nFindCur - 1);
        });
        connect(m_pFindBar, &CFindBar::CloseRequested, this, [this] {
            ClearFindHighlights();
            m_pFindBar->hide();
            setFocus();
        });
    }
    PositionFindBar();
    m_pFindBar->ShowAndFocus();
    PositionFindBar(); // geometry needs the widget visible/laid out
    if (!m_pFindBar->findChild<QLineEdit*>()->text().isEmpty())
        RerunFind(true);
}

void CCodeEditor::PositionFindBar() {
    if (!m_pFindBar) return;
    m_pFindBar->adjustSize();
    const int nW = qMin(m_pFindBar->width() + 8, width() - 12);
    m_pFindBar->setGeometry(width() - nW - 6, 6, nW,
                            m_pFindBar->sizeHint().height());
}

void CCodeEditor::RerunFind(bool bFromCursor) {
    if (!m_pFindBar) return;
    m_arrFindMatches.clear();
    m_nFindCur = -1;

    const QString strNeedle = m_pFindBar->findChild<QLineEdit*>()->text();
    if (strNeedle.isEmpty()) {
        ClearFindHighlights();
        m_pFindBar->UpdateCount(-1, 0);
        return;
    }
    const bool bCs = m_pFindBar->IsCaseSensitive();
    const bool bWord = m_pFindBar->IsWholeWord();

#ifdef CASL_HAVE_QSCINTILLA
    // Scintilla positions are UTF-8 byte offsets
    const QByteArray arrHay = text().toUtf8();
    const QByteArray arrNeedle = strNeedle.toUtf8();
    const QByteArray arrHayL = bCs ? arrHay : arrHay.toLower();
    const QByteArray arrNeedleL = bCs ? arrNeedle : arrNeedle.toLower();
    auto IsWordCharAt = [&arrHayL](int nIdx) { return IsWordCharT(arrHayL.at(nIdx)); };
    auto IndexOf = [&arrHayL, &arrNeedleL](int nFrom) {
        return arrHayL.indexOf(arrNeedleL, nFrom);
    };
    const int nHayLen = (int)arrHayL.size();
    const int nNeedleLen = (int)arrNeedleL.size();
#else
    // QTextDocument positions are QChar offsets
    const QString arrHayL = toPlainText();
    const QString arrNeedleL = strNeedle;
    auto IsWordCharAt = [&arrHayL](int nIdx) { return IsWordCharT(arrHayL.at(nIdx)); };
    auto IndexOf = [&arrHayL, &arrNeedleL, bCs](int nFrom) {
        return arrHayL.indexOf(arrNeedleL, nFrom,
                               bCs ? Qt::CaseSensitive : Qt::CaseInsensitive);
    };
    const int nHayLen = (int)arrHayL.size();
    const int nNeedleLen = (int)arrNeedleL.size();
#endif

    for (int nFrom = 0;;) {
        const int nPos = IndexOf(nFrom);
        if (nPos < 0) break;
        const bool bBoundary =
            !bWord || ((nPos == 0 || !IsWordCharAt(nPos - 1)) &&
                       (nPos + nNeedleLen >= nHayLen ||
                        !IsWordCharAt(nPos + nNeedleLen)));
        if (bBoundary) m_arrFindMatches.push_back({nPos, nNeedleLen});
        nFrom = nPos + 1;
    }

    // start at the first match at/after the caret
    int nStart = 0;
    if (bFromCursor) {
        const int nCaret =
#ifdef CASL_HAVE_QSCINTILLA
            (int)SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
#else
            textCursor().position();
#endif
        for (size_t i = 0; i < m_arrFindMatches.size(); ++i) {
            if (m_arrFindMatches[i].first >= nCaret) {
                nStart = (int)i;
                break;
            }
        }
    }
    if (!m_arrFindMatches.empty()) GotoFindMatch(nStart);
    else ClearFindHighlights();
    m_pFindBar->UpdateCount(m_nFindCur, (int)m_arrFindMatches.size());
}

void CCodeEditor::GotoFindMatch(int nIndex) {
    if (nIndex < 0 || (size_t)nIndex >= m_arrFindMatches.size()) return;
    m_nFindCur = nIndex;
    const auto& mt = m_arrFindMatches[(size_t)nIndex];
#ifdef CASL_HAVE_QSCINTILLA
    SendScintilla(QsciScintillaBase::SCI_SETSEL, mt.first,
                  mt.first + mt.second);
    SendScintilla(QsciScintillaBase::SCI_SCROLLCARET);
#else
    QTextCursor cur = textCursor();
    cur.setPosition(mt.first);
    cur.setPosition(mt.first + mt.second, QTextCursor::KeepAnchor);
    setTextCursor(cur);
#endif
    ApplyFindHighlights();
    m_pFindBar->UpdateCount(m_nFindCur, (int)m_arrFindMatches.size());
}

void CCodeEditor::ApplyFindHighlights() {
#ifdef CASL_HAVE_QSCINTILLA
    ClearFindHighlights();
    for (size_t i = 0; i < m_arrFindMatches.size(); ++i) {
        const auto& mt = m_arrFindMatches[i];
        // indicator 0: all matches; indicator 1: current (stronger fill)
        SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT,
                      (int)(i == (size_t)m_nFindCur ? 1 : 0));
        SendScintilla(QsciScintillaBase::SCI_INDICATORFILLRANGE, mt.first,
                      mt.second);
    }
#else
    const CEditorTheme& th = EditorTheme();
    QColor clrAll = QColor(255, 213, 0, 90);    // translucent yellow
    QColor clrCur = QColor(255, 170, 0, 170);   // stronger orange
    if (th.m_clrPaper.lightness() < 128) {
        clrAll = QColor(120, 170, 255, 70);
        clrCur = QColor(0, 120, 215, 160);
    }
    m_arrFindSelections.clear();
    for (size_t i = 0; i < m_arrFindMatches.size(); ++i) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(i == (size_t)m_nFindCur ? clrCur : clrAll);
        QTextCursor cur(document());
        cur.setPosition(m_arrFindMatches[i].first);
        cur.setPosition(m_arrFindMatches[i].first + m_arrFindMatches[i].second,
                        QTextCursor::KeepAnchor);
        sel.cursor = cur;
        m_arrFindSelections.append(sel);
    }
    OnHighlightCurrentLine();
#endif
}

void CCodeEditor::ClearFindHighlights() {
#ifdef CASL_HAVE_QSCINTILLA
    // SCI_INDICATORCLEARRANGE only clears the CURRENT indicator: clear both
    const int nLen = (int)SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    for (int nInd = 0; nInd < 2; ++nInd) {
        SendScintilla(QsciScintillaBase::SCI_SETINDICATORCURRENT, nInd);
        SendScintilla(QsciScintillaBase::SCI_INDICATORCLEARRANGE, 0, nLen);
    }
#else
    m_arrFindSelections.clear();
    OnHighlightCurrentLine();
#endif
}

void CCodeEditor::SetSourceText(const QString& strText) {
#ifdef CASL_HAVE_QSCINTILLA
    setText(strText);
#else
    setPlainText(strText);
#endif
    m_setBreakLines.clear();
    m_nExecLine = -1;
    emit BreakpointsChanged();
}

QString CCodeEditor::GetSourceText() const {
#ifdef CASL_HAVE_QSCINTILLA
    return text();
#else
    return toPlainText();
#endif
}

int CCodeEditor::CurrentLine() const {
#ifdef CASL_HAVE_QSCINTILLA
    int nLine = 0, nCol = 0;
    getCursorPosition(&nLine, &nCol);
    return nLine + 1;
#else
    return textCursor().blockNumber() + 1;
#endif
}

QString CCodeEditor::CurrentWord() const {
#ifdef CASL_HAVE_QSCINTILLA
    int nLine = 0, nCol = 0;
    getCursorPosition(&nLine, &nCol);
    QString strLine = text(nLine);
    if (nCol >= strLine.size()) return QString();
    int nEnd = nCol;
    while (nEnd < strLine.size() &&
           (strLine[nEnd].isLetterOrNumber() || strLine[nEnd] == '_'))
        ++nEnd;
    int nBeg = nCol;
    while (nBeg > 0 &&
           (strLine[nBeg - 1].isLetterOrNumber() || strLine[nBeg - 1] == '_'))
        --nBeg;
    return strLine.mid(nBeg, nEnd - nBeg);
#else
    QTextCursor cur = textCursor();
    cur.select(QTextCursor::WordUnderCursor);
    return cur.selectedText();
#endif
}

void CCodeEditor::GoToLine(int nLine) {
#ifdef CASL_HAVE_QSCINTILLA
    setCursorPosition(nLine - 1, 0);
#else
    QTextCursor cur = textCursor();
    cur.setPosition(document()->findBlockByNumber(nLine - 1).position());
    setTextCursor(cur);
    centerCursor();
#endif
}

void CCodeEditor::HighlightLine(int nLine) {
    m_nExecLine = nLine;
#ifdef CASL_HAVE_QSCINTILLA
    markerDeleteAll(1);
    if (nLine > 0) markerAdd(nLine - 1, 1);
#else
    OnHighlightCurrentLine();
    if (m_pMarginArea) m_pMarginArea->update();
#endif
}

bool CCodeEditor::IsBreakpointSet(int nLine) const {
    return m_setBreakLines.contains(nLine);
}

void CCodeEditor::ToggleBreakpoint(int nLine) {
    if (nLine < 1) return;
    if (m_setBreakLines.contains(nLine))
        m_setBreakLines.remove(nLine);
    else
        m_setBreakLines.insert(nLine);
#ifdef CASL_HAVE_QSCINTILLA
    markerDeleteAll(0);
    for (int nBp : m_setBreakLines) markerAdd(nBp - 1, 0);
#else
    if (m_pMarginArea) m_pMarginArea->update();
#endif
    emit BreakpointsChanged();
}
