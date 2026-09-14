#include "CommandHelp.hpp"

#include "CodeEditor.hpp"

#include "casl/OpCode.hpp"

#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <cstring>

// ---------------------------------------------------------------------------
// CASL II command reference data
// ---------------------------------------------------------------------------
namespace {

class CHelpEntry {
public:
    const char* m_pszName;      // mnemonic
    const char* m_pszCategory;  // group
    const char* m_pszSyntax;    // operand form
    const char* m_pszDesc;      // Chinese description
    const char* m_pszExample;   // code example
};

const CHelpEntry s_arrHelp[] = {
{"START", "伪指令", "label START [entry]",
 "程序开始。label 为程序名，可省略；操作数指定执行起始标号（省略则从本块头部开始）。"
 "一个源文件必须且只能有一个 START。",
 "MAIN    START ENTRY\n"
 "        ...\n"
 "ENTRY   LD    GR0,#0\n"
 "        END"},

{"END", "伪指令", "END",
 "程序结束。与 START 配对使用，其后可以放置字面量池等由汇编器生成的数据。",
 "MAIN    START\n"
 "        EXIT\n"
 "        END"},

{"DS", "伪指令", "label DS n",
 "定义存储区（Define Storage）：保留 n 个字（n 为非负整数），不初始化。"
 "label 为该区域的起始标号。",
 "BUF     DS    256     ; 保留 256 字的缓冲区\n"
 "LNG     DS    1"},

{"DC", "伪指令", "label DC 常数[,常数...]",
 "定义常数（Define Constant）。常数可以是十进制(-32768..65535)、十六进制（#FF）、"
 "字符常数（'ABC'，每个字符占一字，'' 表示单引号本身）或地址常数（标号）。",
 "MSG     DC    'HELLO'\n"
 "NUM     DC    -1\n"
 "HEX     DC    #FF\n"
 "ADDR    DC    MSG      ; 地址常数"},

{"IN", "宏指令", "IN buf,len",
 "输入一行字符到 buf 指示的缓冲区，并把实际长度写入 len 指示的字。"
 "缓冲区最大 256 字符。程序执行到 IN 时等待用户在输入框中输入一行。",
 "        IN    BUF,LNG\n"
 "        ...\n"
 "BUF     DS    256\n"
 "LNG     DS    1"},

{"OUT", "宏指令", "OUT buf,len",
 "输出 buf 缓冲区中 len 指示长度的字符串到输出窗口。",
 "MSG     DC    'HELLO'\n"
 "LNG     DC    5\n"
 "        OUT   MSG,LNG"},

{"EXIT", "宏指令", "EXIT",
 "程序正常结束（汇编为 SVC 0）。",
 "        OUT   MSG,LNG\n"
 "        EXIT"},

{"SVC", "系统调用", "SVC code",
 "超级调用指令。code=0 结束程序，1 输入，2 输出（通常由 IN/OUT/EXIT 宏展开，不必手写）。",
 "        SVC   0        ; 等价于 EXIT"},

{"NOP", "控制", "NOP",
 "空操作，只使 PR 前进一个字。常用于占位。",
 "        NOP"},

{"LD", "传送", "LD GR,addr[,GRx] | LD GR,GR2",
 "装入：把有效地址的内容装入 GR（双字长指令）。寄存器形式 LD GR1,GR2 为单字长。"
 "执行后按装入值设置 SF/ZF（OF=0）。",
 "        LD    GR0,BUF,GR1   ; GR0 = BUF[GR1]\n"
 "        LD    GR2,GR0       ; 寄存器间传送"},

{"ST", "传送", "ST GR,addr[,GRx]",
 "存储：把 GR 的内容存入有效地址。不影响标志。",
 "        ST    GR0,ANS       ; ANS = GR0"},

{"ADDA", "算术", "ADDA GR,addr[,GRx]",
 "带符号加法：GR = GR + 内存值（补码运算）。按结果设置 OF/SF/ZF。",
 "        LD    GR0,#10\n"
 "        ADDA  GR0,#5      ; GR0 = 21（#10 是十六进制 16）"},

{"ADDL", "算术", "ADDL GR,addr[,GRx]",
 "逻辑（无符号）加法：GR = GR + 内存值（0..65535 视为无符号）。进位时 OF=1。",
 "        ADDL  GR0,#1      ; 0x7FFF+1 -> 0x8000, OF=1"},

{"SUBA", "算术", "SUBA GR,addr[,GRx]",
 "带符号减法：GR = GR - 内存值。按结果设置 OF/SF/ZF。",
 "        SUBA  GR0,#1      ; GR0 = GR0 - 1"},

{"SUBL", "算术", "SUBL GR,addr[,GRx]",
 "逻辑（无符号）减法：GR = GR - 内存值（按 65536 取模）。借位时 OF=1。",
 "        SUBL  GR0,#1"},

{"AND", "逻辑", "AND GR,addr[,GRx]",
 "按位与。按结果设置 SF/ZF（OF=0）。",
 "        LD    GR0,#0F0F\n"
 "        AND   GR0,#00FF   ; GR0 = 0x000F"},

{"OR", "逻辑", "OR GR,addr[,GRx]",
 "按位或。按结果设置 SF/ZF（OF=0）。",
 "        OR    GR0,#00F0"},

{"XOR", "逻辑", "XOR GR,addr[,GRx]",
 "按位异或。结果为 0 时 ZF=1（常用于清零或比较相等）。",
 "        XOR   GR0,GR0SAVE"},

{"CPA", "比较", "CPA GR,addr[,GRx]",
 "带符号比较：GR 与内存值按补码比较，只设置标志不写回。"
 "SF=1 表示 GR 小；等值 ZF=1；溢出 OF=1。常与 JPL/JMI/JZE 配合。",
 "        CPA   GR0,#0\n"
 "        JMI   NEG       ; GR0 < 0 转移"},

{"CPL", "比较", "CPL GR,addr[,GRx]",
 "逻辑（无符号）比较：0..65535 范围比较，设置 SF/ZF（OF=0）。",
 "        CPL   GR0,#8000"},

{"SLA", "移位", "SLA GR,addr[,GRx]",
 "算术左移：符号位不变，其余 15 位左移，低位补 0。移位数 = 有效地址所指内存的值。"
 "按结果设置 SF/ZF。",
 "        LD    GR0,#0001\n"
 "        SLA   GR0,#3     ; GR0 = 8"},

{"SRA", "移位", "SRA GR,addr[,GRx]",
 "算术右移：保持符号，高位用符号位填充。负数右移仍为负。",
 "        LD    GR0,#8000\n"
 "        SRA   GR0,#4     ; GR0 = 0xF800"},

{"SLL", "移位", "SLL GR,addr[,GRx]",
 "逻辑左移：16 位整体左移，低位补 0，高位丢弃。",
 "        SLL   GR0,#4     ; 0x0FFF -> 0xFFF0"},

{"SRL", "移位", "SRL GR,addr[,GRx]",
 "逻辑右移：16 位整体右移，高位补 0。",
 "        SRL   GR0,#4     ; 0xFFF0 -> 0x0FFF"},

{"JUMP", "转移", "JUMP addr[,GRx]",
 "无条件转移到有效地址。",
 "        JUMP  LOOP"},

{"JPL", "条件转移", "JPL addr[,GRx]",
 "SF=0 且 ZF=0（结果为正）时转移。",
 "        SUBA  GR1,#1\n"
 "        JPL   LOOP     ; GR1 > 0 继续"},

{"JMI", "条件转移", "JMI addr[,GRx]",
 "SF=1（结果为负）时转移。",
 "        CPA   GR0,#0\n"
 "        JMI   NEGPROC"},

{"JNZ", "条件转移", "JNZ addr[,GRx]",
 "ZF=0（结果非 0）时转移。",
 "        JNZ   LOOP"},

{"JZE", "条件转移", "JZE addr[,GRx]",
 "ZF=1（结果为 0）时转移。",
 "        CPA   GR0,#0\n"
 "        JZE   DONE"},

{"JOV", "条件转移", "JOV addr[,GRx]",
 "OF=1（溢出）时转移。",
 "        ADDL  GR0,#1\n"
 "        JOV   OVERFLOW"},

{"PUSH", "栈", "PUSH addr[,GRx]",
 "把有效地址所指内存的字压入栈（SP 先减 1）。用于保护现场。",
 "        PUSH  SAVEGR0"},

{"POP", "栈", "POP GR",
 "从栈弹出顶部的字装入 GR（SP 加 1）。与 PUSH 配对恢复现场。",
 "        POP   GR0"},

{"CALL", "子程序", "CALL addr[,GRx]",
 "调用子程序：把返回地址（下一条指令地址）压栈，转移到有效地址。",
 "        CALL  SUB1"},

{"RET", "子程序", "RET",
 "子程序返回：从栈弹出返回地址送 PR。与 CALL 配对。",
 "SUB1    ST    GR0,WORK\n"
 "        ...\n"
 "        RET"},
};

const int s_nHelpCount = (int)(sizeof(s_arrHelp) / sizeof(s_arrHelp[0]));

// ---------------------------------------------------------------------------
// mnemonic -> full English name (abbreviation expansion shown in the detail)
// ---------------------------------------------------------------------------
const char* s_arrFullNames[][2] = {
    {"START", "Program Start"},
    {"END", "Program End"},
    {"DS", "Define Storage"},
    {"DC", "Define Constant"},
    {"IN", "Input"},
    {"OUT", "Output"},
    {"EXIT", "Exit"},
    {"SVC", "Supervisor Call"},
    {"NOP", "No Operation"},
    {"LD", "Load"},
    {"ST", "Store"},
    {"ADDA", "Add Arithmetic (signed)"},
    {"ADDL", "Add Logical (unsigned)"},
    {"SUBA", "Subtract Arithmetic (signed)"},
    {"SUBL", "Subtract Logical (unsigned)"},
    {"AND", "Logical AND"},
    {"OR", "Logical OR"},
    {"XOR", "Exclusive OR"},
    {"CPA", "Compare Arithmetic (signed)"},
    {"CPL", "Compare Logical (unsigned)"},
    {"SLA", "Shift Left Arithmetic"},
    {"SRA", "Shift Right Arithmetic"},
    {"SLL", "Shift Left Logical"},
    {"SRL", "Shift Right Logical"},
    {"JUMP", "Jump (unconditional)"},
    {"JPL", "Jump if Plus"},
    {"JMI", "Jump if Minus"},
    {"JNZ", "Jump if Not Zero"},
    {"JZE", "Jump if Zero"},
    {"JOV", "Jump if Overflow"},
    {"PUSH", "Push onto Stack"},
    {"POP", "Pop from Stack"},
    {"CALL", "Call Subroutine"},
    {"RET", "Return from Subroutine"},
};

QString FullNameOf(const char* pszName) {
    for (const auto& arrPair : s_arrFullNames)
        if (std::strcmp(arrPair[0], pszName) == 0)
            return QString::fromUtf8(arrPair[1]);
    return QString();
}

// ---------------------------------------------------------------------------
// 历年真题（软考经典 CASL 题型，全部经核心库验证可运行）
// ---------------------------------------------------------------------------
class CExamEntry {
public:
    const char* m_pszTitle;  // 题目名
    const char* m_pszTopic;  // 考点
    const char* m_pszDesc;   // 题目说明
    const char* m_pszCode;   // 带注释源码
};

const CExamEntry s_arrExams[] = {
{"累加 1..10 并十进制输出", "循环 / 除法取余",
 "软考经典题型：先循环累加求和，再用“除 10 取余”把结果逐位转成 ASCII 字符，"
 "最后倒序输出。运行后输出 55。",
 "SUM10   START\n"
 "        LD    GR0,#0        ; 累加器\n"
 "        LD    GR1,#A        ; 计数器 = 10\n"
 "LOOP    ST    GR1,WORK      ; CASL 无寄存器间 ADDA\n"
 "        ADDA  GR0,WORK\n"
 "        SUBA  GR1,#1\n"
 "        CPA   GR1,#0\n"
 "        JPL   LOOP\n"
 "        ST    GR0,NUM       ; GR0 = 55\n"
 "        LD    GR2,#0        ; 位数\n"
 "        LD    GR0,NUM\n"
 "        CPA   GR0,#0\n"
 "        JZE   PDIG0\n"
 "PDIV    LD    GR1,#0        ; 商\n"
 "PSUB    SUBA  GR0,#A        ; 反复减 10\n"
 "        JMI   PREM\n"
 "        ADDA  GR1,#1\n"
 "        JUMP  PSUB\n"
 "PREM    ADDA  GR0,#A        ; 还原余数\n"
 "        ADDA  GR0,#30       ; + '0'\n"
 "        ST    GR0,DIGBUF,GR2\n"
 "        ADDA  GR2,#1\n"
 "        ST    GR1,QWORD\n"
 "        LD    GR0,QWORD\n"
 "        CPA   GR1,#0\n"
 "        JNZ   PDIV\n"
 "        JUMP  POUT\n"
 "PDIG0   LD    GR0,#30\n"
 "        ST    GR0,DIGBUF\n"
 "        LD    GR2,#1\n"
 "POUT    ST    GR2,LNG\n"
 "        LD    GR3,#0        ; 倒序拷贝\n"
 "PLOOP   CPA   GR2,#0\n"
 "        JZE   PDONE\n"
 "        SUBA  GR2,#1\n"
 "        LD    GR0,DIGBUF,GR2\n"
 "        ST    GR0,OUTBUF,GR3\n"
 "        ADDA  GR3,#1\n"
 "        JUMP  PLOOP\n"
 "PDONE   OUT   OUTBUF,LNG\n"
 "        EXIT\n"
 "WORK    DS    1\n"
 "NUM     DS    1\n"
 "QWORD   DS    1\n"
 "DIGBUF  DS    8\n"
 "OUTBUF  DS    8\n"
 "LNG     DS    1\n"
 "        END"},

{"冒泡排序", "双重循环 / 变址 / 数据交换",
 "软考经典题型：双重循环冒泡排序，内层用变址寄存器访问 BUF[j]、BUF[j+1]，"
 "用 CPA 判断是否交换。运行后输出 DEORST。",
 "BUBBLE  START\n"
 "        LD    GR0,#0        ; 外层 i\n"
 "OUTER   CPA   GR0,LIM       ; LIM = 5 (n-1)\n"
 "        JZE   DONE\n"
 "        ST    GR0,IWORD\n"
 "        LD    GR2,LIM       ; 内层上界 = LIM-1-i\n"
 "        SUBA  GR2,#1\n"
 "        SUBA  GR2,IWORD\n"
 "        ST    GR2,LWORD\n"
 "        LD    GR1,#0        ; 内层 j\n"
 "INNER   CPA   GR1,LWORD\n"
 "        JPL   IEND\n"
 "        LD    GR3,BUF,GR1   ; a = BUF[j]\n"
 "        LD    GR4,BUF+1,GR1 ; b = BUF[j+1]\n"
 "        ST    GR3,VA\n"
 "        ST    GR4,VB\n"
 "        CPA   GR3,VB        ; a > b ?\n"
 "        JPL   SWAP\n"
 "        JUMP  NEXT\n"
 "SWAP    LD    GR3,VB\n"
 "        ST    GR3,BUF,GR1\n"
 "        LD    GR3,VA\n"
 "        ST    GR3,BUF+1,GR1\n"
 "NEXT    ADDA  GR1,#1\n"
 "        JUMP  INNER\n"
 "IEND    ADDA  GR0,#1\n"
 "        JUMP  OUTER\n"
 "DONE    OUT   BUF,LNG       ; 输出 DEORST\n"
 "        EXIT\n"
 "LIM     DC    5\n"
 "LNG     DC    6\n"
 "IWORD   DS    1\n"
 "LWORD   DS    1\n"
 "VA      DS    1\n"
 "VB      DS    1\n"
 "BUF     DC    'S'\n"
 "        DC    'O'\n"
 "        DC    'R'\n"
 "        DC    'T'\n"
 "        DC    'E'\n"
 "        DC    'D'\n"
 "        END"},

{"输入输出与结束判断", "IN/OUT 宏 / 字符串比较",
 "软考常考的输入处理：反复读入一行并回显，直到读入空行（长度为 0）结束。"
 "运行后在输入框输入任意文本观察回显，输入空行退出。",
 "ECHO    START\n"
 "LOOP    IN    BUF,LNG      ; 读一行\n"
 "        LD    GR0,LNG\n"
 "        CPA   GR0,#0\n"
 "        JZE   DONE         ; 空行退出\n"
 "        OUT   BUF,LNG      ; 回显\n"
 "        JUMP  LOOP\n"
 "DONE    EXIT\n"
 "BUF     DS    256\n"
 "LNG     DS    1\n"
 "        END"},

{"斐波那契数列", "递推 / 数组存储",
 "把 F(0)..F(9) 递推存入 FIBBUF 区。注意：CASL 没有寄存器间 ADDA，"
 "需经内存字中转；用断点或单步观察 FIBBUF 区域（0x1F 起）的变化。",
 "FIB     START\n"
 "        LD    GR0,#1       ; F(1)\n"
 "        LD    GR1,#0       ; F(0)\n"
 "        ST    GR1,FIBBUF\n"
 "        ST    GR0,FIBBUF+1\n"
 "        LD    GR2,#2       ; 下标\n"
 "LOOP    CPA   GR2,#A       ; 注意 # 是十六进制\n"
 "        JZE   DONE\n"
 "        ST    GR0,TMP0\n"
 "        ST    GR1,TMP1\n"
 "        LD    GR0,TMP0\n"
 "        ADDA  GR0,TMP1     ; F(i)=F(i-1)+F(i-2)\n"
 "        ST    GR0,FIBBUF,GR2\n"
 "        LD    GR1,TMP0     ; 递推前移\n"
 "        ADDA  GR2,#1\n"
 "        JUMP  LOOP\n"
 "DONE    EXIT\n"
 "FIBBUF  DS    10\n"
 "TMP0    DS    1\n"
 "TMP1    DS    1\n"
 "        END"},

{"统计正数 / 负数 / 零", "分支结构 / CPA+JMI/JZE",
 "统计 8 个带符号数中正、负、零的个数，结果存入 PCNT/NCNT/ZCNT。"
 "典型的分支转移题目：CPA 后用 JZE/JMI/JPL 三路分支。",
 "SIGN    START\n"
 "        LD    GR1,#0       ; 正数计数\n"
 "        LD    GR2,#0       ; 负数计数\n"
 "        LD    GR3,#0       ; 零计数\n"
 "        LD    GR4,#0       ; 下标\n"
 "LOOP    CPA   GR4,N\n"
 "        JZE   DONE\n"
 "        LD    GR0,BUF,GR4\n"
 "        CPA   GR0,#0\n"
 "        JZE   ISZERO\n"
 "        JMI   ISNEG\n"
 "        ADDA  GR1,#1       ; 正数\n"
 "        JUMP  NEXT\n"
 "ISZERO  ADDA  GR3,#1\n"
 "        JUMP  NEXT\n"
 "ISNEG   ADDA  GR2,#1\n"
 "NEXT    ADDA  GR4,#1\n"
 "        JUMP  LOOP\n"
 "DONE    ST    GR1,PCNT     ; 3\n"
 "        ST    GR2,NCNT     ; 3\n"
 "        ST    GR3,ZCNT     ; 2\n"
 "        EXIT\n"
 "N       DC    8\n"
 "BUF     DC    5\n"
 "        DC    -3\n"
 "        DC    0\n"
 "        DC    9\n"
 "        DC    -7\n"
 "        DC    0\n"
 "        DC    2\n"
 "        DC    -1\n"
 "PCNT    DS    1\n"
 "NCNT    DS    1\n"
 "ZCNT    DS    1\n"
 "        END"},

{"求最大值", "循环 / 比较更新",
 "遍历 8 个带符号数求最大值（带符号比较用 CPA），结果存 MAXW。",
 "MAX     START\n"
 "        LD    GR0,BUF      ; 最大值 = BUF[0]\n"
 "        LD    GR1,#1       ; 下标从 1 开始\n"
 "LOOP    CPA   GR1,N\n"
 "        JZE   DONE\n"
 "        LD    GR2,BUF,GR1\n"
 "        ST    GR2,WORK\n"
 "        CPA   GR0,WORK\n"
 "        JPL   NEXT         ; 当前最大值更大\n"
 "        LD    GR0,WORK     ; 更新最大值\n"
 "NEXT    ADDA  GR1,#1\n"
 "        JUMP  LOOP\n"
 "DONE    ST    GR0,MAXW     ; 9\n"
 "        EXIT\n"
 "N       DC    8\n"
 "BUF     DC    5\n"
 "        DC    -3\n"
 "        DC    9\n"
 "        DC    1\n"
 "        DC    7\n"
 "        DC    -8\n"
 "        DC    2\n"
 "        DC    6\n"
 "WORK    DS    1\n"
 "MAXW    DS    1\n"
 "        END"},

{"字符串逆置", "双下标交换 / GR0 陷阱",
 "经典双下标题：i 从头、j 从尾，向中间推进并交换，i>=j 结束。"
 "特别注意：COMET II 的 GR0 不能作变址寄存器（xr 字段为 0 表示无变址），"
 "下标要用 GR1..GR7。运行后输出 FEDCBA。",
 "REV     START\n"
 "        LD    GR5,#0       ; i = 0（GR5 可作变址）\n"
 "        LD    GR1,LNG\n"
 "        SUBA  GR1,#1       ; j = len-1\n"
 "LOOP    ST    GR5,IW\n"
 "        ST    GR1,JW\n"
 "        LD    GR0,IW\n"
 "        CPA   GR0,JW       ; i >= j ?\n"
 "        JPL   DONE\n"
 "        LD    GR2,BUF,GR5  ; a = BUF[i]\n"
 "        LD    GR3,BUF,GR1  ; b = BUF[j]\n"
 "        ST    GR2,BUF,GR1  ; 交换\n"
 "        ST    GR3,BUF,GR5\n"
 "        ADDA  GR5,#1       ; i++, j--\n"
 "        SUBA  GR1,#1\n"
 "        JUMP  LOOP\n"
 "DONE    OUT   BUF,LNG      ; 输出 \"FEDCBA\"\n"
 "        EXIT\n"
 "BUF     DC    'ABCDEF'\n"
 "LNG     DC    6\n"
 "IW      DS    1\n"
 "JW      DS    1\n"
 "        END"},

{"二分查找", "SRL 除 2 / 三路分支",
 "在升序数组中查找 KEY：mid=(lo+hi)/2 用 SRL #1 实现除 2，"
 "CPA 后 JZE/JMI 三路分支。找到存下标 IDX，未找到存 -1（#FFFF）。",
 "BINS    START\n"
 "        LD    GR0,#0       ; lo = 0\n"
 "        ST    GR0,LO\n"
 "        LD    GR0,N\n"
 "        SUBA  GR0,#1       ; hi = n-1\n"
 "        ST    GR0,HI\n"
 "LOOP    LD    GR0,LO\n"
 "        LD    GR1,HI\n"
 "        ST    GR1,W2\n"
 "        CPA   GR0,W2       ; lo > hi ?\n"
 "        JPL   FAIL\n"
 "        LD    GR0,LO\n"
 "        ADDA  GR0,W2       ; lo+hi\n"
 "        SRL   GR0,#1       ; mid = (lo+hi)/2\n"
 "        ST    GR0,MID\n"
 "        LD    GR1,KEYW\n"
 "        ST    GR1,W1\n"
 "        LD    GR2,MID\n"
 "        LD    GR3,BUF,GR2\n"
 "        ST    GR3,MVAL\n"
 "        CPA   GR1,MVAL     ; KEY 与 BUF[mid]\n"
 "        JZE   FOUND\n"
 "        JMI   LESS\n"
 "        LD    GR0,MID      ; KEY 大：lo = mid+1\n"
 "        ADDA  GR0,#1\n"
 "        ST    GR0,LO\n"
 "        JUMP  LOOP\n"
 "LESS    LD    GR0,MID      ; KEY 小：hi = mid-1\n"
 "        SUBA  GR0,#1\n"
 "        ST    GR0,HI\n"
 "        JUMP  LOOP\n"
 "FOUND   LD    GR0,MID\n"
 "        ST    GR0,IDX      ; 4\n"
 "        EXIT\n"
 "FAIL    LD    GR0,#FFFF    ; -1\n"
 "        ST    GR0,IDX\n"
 "        EXIT\n"
 "N       DC    8\n"
 "KEYW    DC    22\n"
 "BUF     DC    3\n"
 "        DC    7\n"
 "        DC    11\n"
 "        DC    18\n"
 "        DC    22\n"
 "        DC    29\n"
 "        DC    35\n"
 "        DC    40\n"
 "LO      DS    1\n"
 "HI      DS    1\n"
 "MID     DS    1\n"
 "MVAL    DS    1\n"
 "IDX     DS    1\n"
 "W1      DS    1\n"
 "W2      DS    1\n"
 "        END"},

{"十六进制输出", "移位 / AND 取位 / 字符转换",
 "把一个字按 4 个十六进制字符输出：每次 SRL + AND #F 取出 4 位，"
 "0-9 加 '0'（#30），A-F 加 'A'-10（#37）。软考移位指令的典型应用。",
 "HEXO    START\n"
 "        LD    GR1,#0       ; 存放下标 0..3（高位在前）\n"
 "        LD    GR2,#4       ; 循环计数\n"
 "        LD    GR4,#C       ; 移位量 12,8,4,0\n"
 "        ST    GR4,SHIFT\n"
 "LOOP    CPA   GR2,#0\n"
 "        JZE   DONE\n"
 "        LD    GR3,NUM\n"
 "        SRL   GR3,SHIFT    ; 右移到最低 4 位\n"
 "        AND   GR3,#F       ; 取低 4 位\n"
 "        CPA   GR3,#A\n"
 "        JMI   DIG\n"
 "        ADDA  GR3,#37      ; 'A'-10\n"
 "        JUMP  STO\n"
 "DIG     ADDA  GR3,#30      ; '0'\n"
 "STO     ST    GR3,OUTBUF,GR1\n"
 "        LD    GR4,SHIFT\n"
 "        SUBA  GR4,#4\n"
 "        ST    GR4,SHIFT\n"
 "        ADDA  GR1,#1\n"
 "        SUBA  GR2,#1\n"
 "        JUMP  LOOP\n"
 "DONE    OUT   OUTBUF,LNG   ; 输出 \"ABCD\"\n"
 "        EXIT\n"
 "NUM     DC    #ABCD\n"
 "LNG     DC    4\n"
 "OUTBUF  DS    4\n"
 "SHIFT   DS    1\n"
 "        END"},

{"统计字符出现次数", "顺序扫描 / 逐字比较",
 "统计字符串 'HELLO WORLD' 中字符 'L' 出现的次数，结果存 COUNT（=3）。"
 "注意 DC 字符串可以含空格，长度要人工数对。",
 "CNT     START\n"
 "        LD    GR1,#0       ; 计数\n"
 "        LD    GR2,#0       ; 下标\n"
 "LOOP    CPA   GR2,LNG\n"
 "        JZE   DONE\n"
 "        LD    GR0,BUF,GR2\n"
 "        LD    GR3,CH\n"
 "        ST    GR3,W\n"
 "        CPA   GR0,W\n"
 "        JNZ   NEXT\n"
 "        ADDA  GR1,#1       ; 找到一次\n"
 "NEXT    ADDA  GR2,#1\n"
 "        JUMP  LOOP\n"
 "DONE    ST    GR1,COUNT    ; 3\n"
 "        EXIT\n"
 "BUF     DC    'HELLO WORLD'\n"
 "LNG     DC    11\n"
 "CH      DC    'L'\n"
 "COUNT   DS    1\n"
 "W       DS    1\n"
 "        END"},

{"数组循环左移", "变址传送 / 边界处理",
 "把字符串循环左移一位：'ABCDE' -> 'BCDEA'。先保存首元素，"
 "再把 BUF[i]=BUF[i+1] 逐个前移，最后首元素放到末尾。",
 "ROT     START\n"
 "        LD    GR0,BUF      ; 保存首元素\n"
 "        ST    GR0,TMP\n"
 "        LD    GR1,#0       ; 下标\n"
 "        LD    GR2,NN\n"
 "        SUBA  GR2,#1\n"
 "        ST    GR2,WM       ; n-1\n"
 "LOOP    CPA   GR1,WM\n"
 "        JZE   LAST\n"
 "        JPL   LAST\n"
 "        LD    GR2,BUF+1,GR1\n"
 "        ST    GR2,BUF,GR1  ; BUF[i] = BUF[i+1]\n"
 "        ADDA  GR1,#1\n"
 "        JUMP  LOOP\n"
 "LAST    LD    GR2,WM       ; 首元素放到末尾\n"
 "        LD    GR0,TMP\n"
 "        ST    GR0,BUF,GR2\n"
 "        OUT   BUF,LNG      ; 输出 \"BCDEA\"\n"
 "        EXIT\n"
 "BUF     DC    'ABCDE'\n"
 "LNG     DC    5\n"
 "NN      DC    5\n"
 "TMP     DS    1\n"
 "WM      DS    1\n"
 "        END"},

{"小写转大写", "字符区间判断 / ASCII 运算",
 "遍历字符串，字符在 'a'..'z'（#61..#7A）区间内则减 #20 转成大写。"
 "运行后输出 'HELLO CASL 2024!'。",
 "UPPER   START\n"
 "        LD    GR1,#0\n"
 "LOOP    CPA   GR1,LNG\n"
 "        JZE   DONE\n"
 "        LD    GR0,BUF,GR1\n"
 "        CPA   GR0,#61      ; < 'a' 跳过\n"
 "        JMI   NEXT\n"
 "        CPA   GR0,#7A      ; > 'z' 跳过\n"
 "        JPL   NEXT\n"
 "        SUBA  GR0,#20      ; 小写转大写\n"
 "        ST    GR0,BUF,GR1\n"
 "NEXT    ADDA  GR1,#1\n"
 "        JUMP  LOOP\n"
 "DONE    OUT   BUF,LNG\n"
 "        EXIT\n"
 "BUF     DC    'Hello CASL 2024!'\n"
 "LNG     DC    16\n"
 "        END"},

{"阶乘（乘法子程序）", "CALL/RET / 重复加法实现乘法",
 "COMET II 没有乘法指令，用“重复加法”子程序 MUL 实现 GR2=GR0*GR1，"
 "主程序循环求 5!=120 存入 F。子程序与 CALL/RET 的经典练习。",
 "FACT    START\n"
 "        LD    GR0,#1       ; F = 1\n"
 "        LD    GR1,#1       ; i = 1\n"
 "LOOP    CPA   GR1,N\n"
 "        JPL   DONE         ; i > N 结束\n"
 "        CALL  MUL          ; GR2 = GR0 * GR1\n"
 "        ST    GR2,W\n"
 "        LD    GR0,W        ; F = F * i\n"
 "        ADDA  GR1,#1\n"
 "        JUMP  LOOP\n"
 "DONE    ST    GR0,F        ; 120\n"
 "        EXIT\n"
 "; ---- 乘法子程序：GR2 = GR0 * GR1 ----\n"
 "MUL     ST    GR0,FA\n"
 "        ST    GR1,FB\n"
 "        LD    GR2,#0\n"
 "        LD    GR3,FB\n"
 "MLOOP   CPA   GR3,#0\n"
 "        JZE   MRET\n"
 "        ADDA  GR2,FA       ; 累加 FB 次\n"
 "        SUBA  GR3,#1\n"
 "        JUMP  MLOOP\n"
 "MRET    RET\n"
 "N       DC    5\n"
 "F       DS    1\n"
 "W       DS    1\n"
 "FA      DS    1\n"
 "FB      DS    1\n"
 "        END"},
};

const int s_nExamCount = (int)(sizeof(s_arrExams) / sizeof(s_arrExams[0]));

QString FormatEntryHtml(const CHelpEntry& entry) {
    QString strHtml;
    strHtml += "<html><body>";
    strHtml += QString("<h2 style='color:%1;margin:0'>%2</h2>")
                   .arg("#2A82DA", entry.m_pszName);
    const QString strFull = FullNameOf(entry.m_pszName);
    if (!strFull.isEmpty())
        strHtml += QString(
                       "<p style='color:#888;margin:0 0 2px 0'>%1 &mdash; %2</p>")
                       .arg(entry.m_pszName, strFull);
    strHtml += QString("<p style='color:gray;margin:2px 0 10px 0'>%1</p>")
                   .arg(entry.m_pszCategory);
    strHtml += QString("<p style='color:gray;margin:2px 0 10px 0'>%1</p>")
                   .arg(entry.m_pszCategory);
    strHtml += "<p><b>语法</b></p>";
    strHtml += QString(
                   "<pre style='background:#2D2D30;color:#DCDCDC;padding:8px;"
                   "border-radius:4px'>%1</pre>")
                   .arg(entry.m_pszSyntax);
    strHtml += "<p><b>说明</b></p>";
    strHtml += QString("<p>%1</p>").arg(entry.m_pszDesc);
    // the code example is shown in a colored snippet editor below this text
    strHtml += "</body></html>";
    return strHtml;
}

} // namespace

// ---------------------------------------------------------------------------
// dialog
// ---------------------------------------------------------------------------

CCommandHelpDialog::CCommandHelpDialog(QWidget* pParent) : QDialog(pParent) {
    setWindowTitle("CASL 命令帮助");
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(860, 560);

    auto* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(8, 8, 8, 8);

    auto* pTop = new QWidget(this);
    auto* pTopLayout = new QVBoxLayout(pTop);
    pTopLayout->setContentsMargins(0, 0, 0, 0);
    auto* plblHint = new QLabel("输入命令关键字自动定位（如 LD、JUMP、DC）：", pTop);
    m_peditSearch = new QLineEdit(pTop);
    m_peditSearch->setPlaceholderText("搜索命令...");
    m_peditSearch->setClearButtonEnabled(true);
    pTopLayout->addWidget(plblHint);
    pTopLayout->addWidget(m_peditSearch);
    pLayout->addWidget(pTop);

    auto* pSplitter = new QSplitter(Qt::Horizontal, this);

    // left side: command list on top, past-exam list below
    auto* pLeft = new QWidget(pSplitter);
    auto* pLeftLayout = new QVBoxLayout(pLeft);
    pLeftLayout->setContentsMargins(0, 0, 0, 0);
    pLeftLayout->setSpacing(4);
    pLeftLayout->addWidget(new QLabel("<b>命令</b>", pLeft));
    m_plistCommands = new QListWidget(pLeft);
    QFont fontList = m_plistCommands->font();
    fontList.setFamily("Consolas");
    fontList.setBold(true);
    m_plistCommands->setFont(fontList);
    pLeftLayout->addWidget(m_plistCommands, 3);
    pLeftLayout->addWidget(new QLabel("<b>历年真题</b>", pLeft));
    m_plistExams = new QListWidget(pLeft);
    pLeftLayout->addWidget(m_plistExams, 2);
    for (int i = 0; i < s_nExamCount; ++i)
        m_plistExams->addItem(QString::fromUtf8(s_arrExams[i].m_pszTitle));

    // right side: usage text on top, colored example code below; a vertical
    // splitter between them so the code area can be enlarged by dragging
    auto* pRightSplitter = new QSplitter(Qt::Vertical, pSplitter);
    pRightSplitter->setChildrenCollapsible(false);
    m_ptxtDetail = new QTextBrowser(pRightSplitter);
    m_ptxtDetail->setOpenExternalLinks(false);
    m_ptxtDetail->setFont(CaslCodeFont()); // 说明文字也用代码字体
    auto* pExampleBox = new QWidget(pRightSplitter);
    auto* pExampleLayout = new QVBoxLayout(pExampleBox);
    pExampleLayout->setContentsMargins(0, 4, 0, 0);
    pExampleLayout->setSpacing(4);
    auto* plblExample = new QLabel("<b>示例（CASL 着色）</b>", pExampleBox);
    m_pSnippet = CCaslSnippet::Create(pExampleBox);
    pExampleLayout->addWidget(plblExample);
    pExampleLayout->addWidget(m_pSnippet->Widget());
    pRightSplitter->addWidget(m_ptxtDetail);
    pRightSplitter->addWidget(pExampleBox);
    pRightSplitter->setStretchFactor(0, 3);
    pRightSplitter->setStretchFactor(1, 2);
    pRightSplitter->setSizes({280, 220});

    pSplitter->addWidget(pLeft);
    pSplitter->addWidget(pRightSplitter);
    pSplitter->setStretchFactor(0, 0);
    pSplitter->setStretchFactor(1, 1);
    pSplitter->setSizes({240, 620});
    pLayout->addWidget(pSplitter, 1);

    connect(m_peditSearch, &QLineEdit::textChanged, this,
            &CCommandHelpDialog::OnSearchChanged);
    connect(m_plistCommands, &QListWidget::currentRowChanged, this,
            &CCommandHelpDialog::OnCommandSelected);
    connect(m_plistExams, &QListWidget::currentRowChanged, this,
            &CCommandHelpDialog::OnExamSelected);

    BuildCommandList();
    if (m_plistCommands->count() > 0) m_plistCommands->setCurrentRow(0);
}

void CCommandHelpDialog::BuildCommandList(const QString& strFilter) {
    m_plistCommands->blockSignals(true);
    m_plistCommands->clear();
    m_arrNames.clear();
    const QString strFilterUpper = strFilter.trimmed().toUpper();
    for (int i = 0; i < s_nHelpCount; ++i) {
        const QString strName = s_arrHelp[i].m_pszName;
        if (!strFilterUpper.isEmpty() && !strName.contains(strFilterUpper))
            continue;
        m_plistCommands->addItem(strName);
        m_arrNames.push_back(strName);
        m_plistCommands->item(m_plistCommands->count() - 1)
            ->setData(Qt::UserRole, i); // index into the entry table
    }
    m_plistCommands->blockSignals(false);
}

void CCommandHelpDialog::OnSearchChanged(const QString& strText) {
    const QString strTrimmed = strText.trimmed();
    BuildCommandList(strTrimmed);
    if (m_plistCommands->count() > 0) m_plistCommands->setCurrentRow(0);
    else m_ptxtDetail->setHtml(
        "<p style='color:gray'>没有匹配的命令。</p>");
}

void CCommandHelpDialog::OnCommandSelected(int nRow) {
    if (nRow >= 0) m_plistExams->setCurrentRow(-1); // exclusive selection
    ShowEntry(nRow);
}

void CCommandHelpDialog::OnExamSelected(int nRow) {
    if (nRow >= 0) m_plistCommands->setCurrentRow(-1);
    ShowExam(nRow);
}

void CCommandHelpDialog::ShowExam(int nRow) {
    if (nRow < 0 || nRow >= s_nExamCount) return;
    const CExamEntry& exam = s_arrExams[nRow];
    QString strHtml;
    strHtml += "<html><body>";
    strHtml += QString("<h2 style='color:#C08030;margin:0'>%1</h2>")
                   .arg(QString::fromUtf8(exam.m_pszTitle));
    strHtml += QString("<p style='color:gray;margin:2px 0 10px 0'>考点：%1</p>")
                   .arg(QString::fromUtf8(exam.m_pszTopic));
    strHtml += QString("<p>%1</p>").arg(QString::fromUtf8(exam.m_pszDesc));
    strHtml += "</body></html>";
    m_ptxtDetail->setHtml(strHtml);
    m_pSnippet->SetCode(QString::fromUtf8(exam.m_pszCode));
}

void CCommandHelpDialog::ShowEntry(int nRow) {
    m_nActiveRow = nRow;
    if (nRow < 0 || nRow >= m_plistCommands->count()) return;
    int nEntry = m_plistCommands->item(nRow)->data(Qt::UserRole).toInt();
    if (nEntry >= 0 && nEntry < s_nHelpCount) {
        m_ptxtDetail->setHtml(FormatEntryHtml(s_arrHelp[nEntry]));
        m_pSnippet->SetCode(s_arrHelp[nEntry].m_pszExample);
    }
}

bool CCommandHelpDialog::LocateCommand(const QString& strKeyword) {
    QString strUpper = strKeyword.trimmed().toUpper();
    if (strUpper.isEmpty()) return false;
    // exact / prefix / contains match in the FULL table
    int nExact = -1, nPrefix = -1, nContains = -1;
    for (int i = 0; i < s_nHelpCount; ++i) {
        const QString strName = s_arrHelp[i].m_pszName;
        if (strName == strUpper) nExact = i;
        else if (nPrefix < 0 && strName.startsWith(strUpper)) nPrefix = i;
        else if (nContains < 0 && strName.contains(strUpper)) nContains = i;
    }
    int nEntry = nExact >= 0 ? nExact : (nPrefix >= 0 ? nPrefix : nContains);
    if (nEntry < 0) return false;

    // rebuild the list without filter and select the row for nEntry
    m_peditSearch->blockSignals(true);
    m_peditSearch->clear();
    m_peditSearch->blockSignals(false);
    BuildCommandList();
    for (int nRow = 0; nRow < m_plistCommands->count(); ++nRow) {
        if (m_plistCommands->item(nRow)->data(Qt::UserRole).toInt() == nEntry) {
            m_plistCommands->setCurrentRow(nRow);
            m_plistCommands->scrollToItem(m_plistCommands->item(nRow),
                                          QAbstractItemView::PositionAtCenter);
            break;
        }
    }
    return true;
}

void CCommandHelpDialog::keyPressEvent(QKeyEvent* pEvent) {
    // Esc hides instead of closing, so the dialog can be reused
    if (pEvent->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QDialog::keyPressEvent(pEvent);
}
