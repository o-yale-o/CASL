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
{"内存", "概念", "65536 个字，每字 16 位，地址 0x0000..0xFFFF",
 "把内存想成一排【带编号的储物柜】：一共 65536 个柜子，编号（地址）从 "
 "0x0000 到 0xFFFF，每个柜子里放一张【16 位的卡片】（一个“字”）。\n"
 "【重要】编号是“第几个柜子”，不是第几个字节！柜子 0x0100 往后数 5 个是 "
 "0x0105（不是 0x010A）。一条指令、一个数、一个字符，各占一个柜子（一个字）。\n"
 "【放什么】一个字可以存 0..65535 的无符号数，或 -32768..32767 的补码"
 "（负数），或一个字符（IN/OUT 时用，占低 8 位）。\n"
 "【谁住哪】0x0000 开始放【指令】（程序区），指令后面放【字面量池】"
 "（#常数用的柜子），再后面放【数据】（DC/DS），最顶上 0xFFFE 附近是"
 "【堆栈】（从上往下摞）。\n"
 "【例】BUF 在 0x0100 号柜子。LD GR0,BUF,GR1 且 GR1 里写的数是 5："
 "就是去开 0x0105 号柜子，把里面的卡片抄进 GR0。",
 "        ; 内存就是 65536 个带编号的柜子\n"
 "        ; 每个柜子放一个 16 位的“字”\n"
 "        LD    GR0,BUF,GR1   ; GR0 ← 柜子(BUF+GR1的值)的内容"},

{"寄存器", "概念", "GR0..GR7（通用）| SP（栈顶）| PR（下一条）| FR（标志）",
 "寄存器是 CPU 手里的【8 个小本本】：GR0 到 GR7，每个本本只能写一个 16 位的数。"
 "算数、搬数都靠它们。\n"
 "【GR0】算术最常用（好比第一号草稿本），但【不能当变址用】！"
 "指令里 GRx 位置写 GR0 表示“没有变址”，所以 LD GR0,BUF,GR0 的第二个 GR0 "
 "不是变址——这是 CASL 最著名的坑，下标请用 GR1..GR7。\n"
 "【SP】栈指针本本：记着堆栈最顶上用到了哪个柜子。\n"
 "【PR】指令指针本本：记着下一条要执行的指令在哪个柜子。\n"
 "【FR】标志本本：只记三个灯（见“标志 FR”条目）。\n"
 "【例】LD GR1,#0005 → GR1 本本上写 5；ST GR2,BUF,GR1 → "
 "把 GR2 本本上的数抄到 BUF+5 号柜子。",
 "        LD    GR1,#5       ; GR1 本本写上 5\n"
 "        ST    GR2,BUF,GR1  ; 抄到柜子 BUF+5\n"
 "        ; 注意：变址位置别写 GR0（等于没写）"},

{"标志 FR", "概念", "OF（溢出）| SF（负号）| ZF（零）",
 "FR 是三个【小灯】，每次算数或比较后自动亮灭，跳转指令看灯办事。\n"
 "【ZF 零号灯】结果 = 0 亮。JZE（结果为零跳）、JNZ（非零跳）看它。\n"
 "【SF 负号灯】结果是负数亮。JMI（负跳）、JPL（正且非零跳）看它。\n"
 "【OF 溢出灯】加减超出了 -32768..32767 的范围亮。JOV（溢出跳）看它。\n"
 "【例】GR1 = 3，CPA GR1,LIM（LIM 处存 5）：3 - 5 = -2 → "
 "SF 亮（GR1 小）、ZF 灭。接着 JMI 就跳（小于走这条路），JZE 不跳。\n"
 "【口诀】先 CPA/CPL 比一比，再看灯决定 JUMP 去哪。",
 "        CPA   GR0,#5      ; 和 5 比一比，点亮小灯\n"
 "        JMI   SMALL       ; 负号灯亮：GR0 < 5 跳走\n"
 "        JZE   EQUAL       ; 零号灯亮：GR0 = 5 跳走\n"
 "        ; 两个都没跳：GR0 > 5，继续往下"},

{"堆栈", "概念", "SP 初值 0xFFFE，向低地址生长（一摞盘子）",
 "堆栈是【一摞盘子】：柜子从高端 0xFFFE 开始，只能最上面放/取"
 "（先进后出）。\n"
 "【PUSH addr】放一个盘子：SP 先减 1（往上摞一格），再把 addr 处的数"
 "抄到 [SP] 号柜子。\n"
 "【POP GR】取一个盘子：GR ← [SP] 号柜子的数，SP 加 1。\n"
 "【CALL / RET】CALL 把“回来住哪个柜子”（返回地址）压盘，子程序 RET 时"
 "取出来接着执行。\n"
 "【例】SP = 0xFFFE 时 PUSH X（X 处存 1234）：SP 变 0xFFFD，"
 "1234 写进 0xFFFD 号柜子。POP GR0 → GR0 = 1234，SP 回 0xFFFE。\n"
 "【用途】子程序里先把要用的寄存器 PUSH 存起来，用完 POP 恢复"
 "（保护现场）。",
 "        PUSH  GR0SAVE     ; 先存起来\n"
 "        CALL  SUB1        ; 压入返回地址后进入子程序\n"
 "        POP   GR0SAVE     ; 取回原值\n"
 "        ; SP 初值 0xFFFE，盘子往低地址摞"},

{"数据段", "概念", "标号 DC 常数[,常数...] | 标号 DS n",
 "数据段是【提前填好的表格】：DC 是往柜子里预写数字，DS 是空出一段柜子"
 "留给程序用。\n"
 "【DC 定义常数】按出现顺序占连续柜子，每个常数一个字。"
 "'AB' 占 2 个柜子（0041、0042）；3,-1 占 2 个柜子（0003、FFFF）。\n"
 "【DS 留空位】空出 n 个柜子（n 是十进制数），程序运行时往里写。\n"
 "【标号】给第一个柜子起的名字 = 它的地址。BUF+1 就是下一个柜子。\n"
 "【例】设数据段从 0x0100 开始：\n"
 "MSG DC 'AB' → 0x0100=0041、0x0101=0042\n"
 "NUM DC 3,-1 → 0x0102=0003、0x0103=FFFF\n"
 "BUF DS 8 → 0x0104..0x010B 共 8 个柜子空着\n"
 "【访问】LD GR0,MSG 读；ST GR0,BUF,GR1 写进 BUF+第 GR1 个柜子。",
 "MSG     DC    'AB'        ; 两个柜子：0041 0042\n"
 "NUM     DC    3,-1        ; 两个柜子：0003 FFFF\n"
 "BUF     DS    8           ; 空出 8 个柜子\n"
 "        LD    GR0,MSG     ; 读第一个柜子"},

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
 "【两个操作数】buf = 缓冲区基地址（标号，建议 DS 256 预留）；"
 "len = 存放实际长度的字的标号。\n"
 "【干什么】程序执行到 IN 时暂停，在左下角【程序输入 (IN)】框等待："
 "只输入字符本身（不用带长度、不用逗号！），回车后机器把字符逐个"
 "写进内存 buf 开始的连续字（每字符占一个字），并【自动数出个数】"
 "写进 len 处。\n"
 "【例】IN BUF,LNG（设 BUF=0x0100、LNG=0x0200）：在输入框打 Hello 回车 → "
 "内存[0x0100]=0048('H')、[0x0101]=0065('e')、[0x0102..0x0104]='l','l','o'，"
 "LNG 处（0x0200）自动变为 0005。\n"
 "【注意】缓冲区要够大（DS 256）：输入超过缓冲区的部分会越界覆盖"
 "后面的内存（COMET II 不检查边界）。",
 "        IN    BUF,LNG     ; 停下来等输入\n"
 "        ...\n"
 "BUF     DS    256         ; 缓冲区：最多 256 个字符\n"
 "LNG     DS    1           ; 机器自动写入实际个数"},

{"OUT", "宏指令", "OUT buf,len",
 "【两个操作数】buf = 缓冲区基地址（标号）；"
 "len = 存放输出长度的字的标号。\n"
 "【干什么】从内存 buf 开始，连续把 N 个字的内容按字符显示到输出窗口"
 "（N = len 处那个字里存的数，不是指令里写死的！）。\n"
 "【例】MSG DC 'HELLO'、LNG DC 5：OUT MSG,LNG → 显示 HELLO（5 个字）。"
 "运行时把 LNG 改成 3（变量面板）再运行 → 只显示 HEL。\n"
 "【配合】常与 IN 配对：IN 之后 LNG 已是实际长度，直接 OUT BUF,LNG 即可"
 "回显刚输入的内容。",
 "MSG     DC    'HELLO'\n"
 "LNG     DC    5\n"
 "        OUT   MSG,LNG     ; 显示 HELLO"},

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
 "【算地址】EA = addr + GRx 里存的数。addr 是基地址（通常是标号）；"
 "GRx 里存的数是偏移量（单位=字，不是字节）；省略 ,GRx 时 EA = addr。\n"
 "【干什么】把内存 EA 处的一个字（16 位）复制进 GR，GR 原值被覆盖。\n"
 "【例】设 BUF 的地址是 0x0100（BUF DS 8 预留了 8 个字：0x0100..0x0107）。"
 "LD GR0,BUF,GR1 且 GR1 里存的数是 5：\n"
 "EA = 0x0100 + 5 = 0x0105（从 BUF 起第 6 个字，编号从 0 数起）。"
 "GR0 ← 内存[0x0105] 的 16 位内容：若该处原值是 0041，则 GR0 = 0x0041。"
 "相当于 C 语言的 GR0 = BUF[GR1]。\n"
 "提示：COMET 内存全由字组成（65536 个字，每字 16 位），地址就是字的编号，"
 "加减按字数算——0x0100+5 是 0x0105，不是 0x010A。\n"
 "寄存器形式 LD GR1,GR2 为单字长，仅寄存器间传送。"
 "执行后按装入值设置 SF/ZF（OF=0）。",
 "        LD    GR0,BUF,GR1   ; 若 GR1=5: GR0 ← BUF+5 处的字\n"
 "        LD    GR2,GR0       ; 寄存器间传送（单字长）"},

{"ST", "传送", "ST GR,addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数。addr 是基地址（通常是标号）；"
 "GRx 里存的数是偏移量（单位=字）；省略 ,GRx 时 EA = addr。\n"
 "【干什么】把 GR 的 16 位值写进内存 EA 处的一个字。"
 "只写一个字（16 位），不会顺序写多个字。\n"
 "【例】设 BUF 的地址是 0x0100（8 个字：0x0100..0x0107）。"
 "ST GR2,BUF,GR1 且 GR1 里存的数是 5：\n"
 "EA = 0x0100 + 5 = 0x0105。\n"
 "内存[0x0105] ← GR2 的 16 位值：若 GR2 = 0x1234，则 0x0105 处变成 1234。"
 "0x0106、0x0107 不受影响；偏移按字数算（+5 是第 6 个字，"
 "不是按字节算的 0x010A）。相当于 C 语言的 BUF[GR1] = GR2。\n"
 "不影响标志。",
 "        ST    GR2,BUF,GR1   ; 若 GR1=5: BUF+5 ← GR2（即 BUF[5]=GR2）\n"
 "        ST    GR0,ANS       ; ANS ← GR0（无变址：EA=ANS）"},

{"ADDA", "算术", "ADDA GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR + 内存[EA]，带符号补码加法；"
 "加数是 EA 处一个字（16 位）的内容，不是 EA 本身。按结果设置 OF/SF/ZF。\n"
 "【例】设 CNT 的地址是 0x0200，GR1 里存的数是 5。ADDA GR0,CNT,GR1：\n"
 "读内存[0x0205] 当加数：若该处存的是 0003、GR0 = 0007，则 GR0 = 0x000A。",
 "        LD    GR0,#10\n"
 "        ADDA  GR0,#5      ; GR0 = 21（#10 是十六进制 16）"},

{"ADDL", "算术", "ADDL GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR + 内存[EA]，无符号加法（0..65535）。"
 "加数是 EA 处一个字的内容。进位时 OF=1。\n"
 "【例】GR0 = 0x7FFF，ADDL GR0,#1：#1 进字面量池（某地址 W 处放 0001），"
 "加数 = 内存[W] = 1 → GR0 = 0x8000，OF=1。",
 "        ADDL  GR0,#1      ; 0x7FFF+1 -> 0x8000, OF=1"},

{"SUBA", "算术", "SUBA GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR - 内存[EA]，带符号减法。"
 "减数是 EA 处一个字的内容。按结果设置 OF/SF/ZF。\n"
 "【例】设 WORK 的地址是 0x0300 且该处存 0001。SUBA GR0,WORK：\n"
 "GR0 = GR0 - 内存[0x0300] = GR0 - 1。",
 "        SUBA  GR0,#1      ; GR0 = GR0 - 1"},

{"SUBL", "算术", "SUBL GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR - 内存[EA]，无符号减法（按 65536 取模）。"
 "减数是 EA 处一个字的内容。借位时 OF=1。\n"
 "【例】GR0 = 0x0000，SUBL GR0,#1：减数=1 → GR0 = 0xFFFF"
 "（即 -1 的补码），OF=1。",
 "        SUBL  GR0,#1"},

{"AND", "逻辑", "AND GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR 按位与 内存[EA]。操作数是 EA 处一个字的内容。"
 "按结果设置 SF/ZF（OF=0）。\n"
 "【例】GR0 = 0x0F0F，AND GR0,#00FF：加数 = 0x00FF（字面量池）"
 "→ GR0 = 0x000F。",
 "        LD    GR0,#0F0F\n"
 "        AND   GR0,#00FF   ; GR0 = 0x000F"},

{"OR", "逻辑", "OR GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR 按位或 内存[EA]。操作数是 EA 处一个字的内容。"
 "按结果设置 SF/ZF（OF=0）。\n"
 "【例】GR0 = 0x0F00，OR GR0,#00F0 → GR0 = 0x0FF0。",
 "        OR    GR0,#00F0"},

{"XOR", "逻辑", "XOR GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR ← GR 按位异或 内存[EA]。操作数是 EA 处一个字的内容。"
 "结果为 0 时 ZF=1（常用于清零或判断相等）。\n"
 "【例】GR0 = 0x1234，XOR GR0,#1234（字面量池）→ GR0 = 0x0000，ZF=1。",
 "        XOR   GR0,GR0SAVE"},

{"CPA", "比较", "CPA GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】拿 GR 与内存[EA] 处一个字的内容做带符号（补码）比较，"
 "只设标志、不写回。\n"
 "【结果】SF=1 表示 GR 小；相等 ZF=1；溢出 OF=1。\n"
 "【例】GR0 = 0x0003，CPA GR0,LIM（设 LIM 处存 5）：3 - 5 = -2 → SF=1"
 "（GR0 小），配 JMI/JPL 判断大小。",
 "        CPA   GR0,#0\n"
 "        JMI   NEG       ; GR0 < 0 转移"},

{"CPL", "比较", "CPL GR,addr[,GRx]",
 "【算地址】同 LD：EA = addr + GRx 里存的数。\n"
 "【干什么】GR 与内存[EA] 处的内容按无符号（0..65535）比较，只设标志。"
 "SF=1 表示 GR 小；相等 ZF=1（OF=0）。\n"
 "【例】GR0 = 0x8000，CPL GR0,#7FFF：无符号 32768 > 32767 → SF=0"
 "（GR0 大）。若用 CPA 会当 -32768 < 32767 得出相反结论。",
 "        CPL   GR0,#8000"},

{"SLA", "移位", "SLA GR,addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数（同 LD）。\n"
 "【特别】移位位数不是写在指令里的数，而是内存 EA 处那个字的值！"
 "写 SLA GR0,#3 时，#3 被汇编器放进字面量池（某地址 W 处放 0003），"
 "运行时 N = 内存[W] = 3。\n"
 "【干什么】GR 左移 N 位：符号位不变，其余 15 位左移，低位补 0。"
 "按结果设置 SF/ZF。\n"
 "【例】GR0 = 0x0001，SLA GR0,#3：N = 3 → GR0 = 0x0008"
 "（1 左移 3 位）。",
 "        LD    GR0,#0001\n"
 "        SLA   GR0,#3     ; GR0 = 8"},

{"SRA", "移位", "SRA GR,addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数（同 LD）。\n"
 "【特别】移位位数 = 内存 EA 处那个字的值（同 SLA）。\n"
 "【干什么】GR 右移 N 位：保持符号，高位用符号位填充，负数右移仍为负。\n"
 "【例】GR0 = 0x8000，SRA GR0,#4：N = 4，符号位 1 保持 → GR0 = 0xF800"
 "（仍是负数）。",
 "        LD    GR0,#8000\n"
 "        SRA   GR0,#4     ; GR0 = 0xF800"},

{"SLL", "移位", "SLL GR,addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数（同 LD）。\n"
 "【特别】移位位数 = 内存 EA 处那个字的值（同 SLA）。\n"
 "【干什么】GR 左移 N 位：16 位整体左移，低位补 0，高位丢弃（不保符号）。\n"
 "【例】GR0 = 0x0FFF，SLL GR0,#4：N = 4 → GR0 = 0xFFF0。",
 "        SLL   GR0,#4     ; 0x0FFF -> 0xFFF0"},

{"SRL", "移位", "SRL GR,addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数（同 LD）。\n"
 "【特别】移位位数 = 内存 EA 处那个字的值（同 SLA）。\n"
 "【干什么】GR 右移 N 位：16 位整体右移，高位补 0（不保符号）。\n"
 "【例】GR0 = 0xFFF0，SRL GR0,#4：N = 4 → GR0 = 0x0FFF。",
 "        SRL   GR0,#4     ; 0xFFF0 -> 0x0FFF"},

{"JUMP", "转移", "JUMP addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（addr 基地址，GRx 里存的数=偏移；"
 "省略则 EA = addr）。\n"
 "【注意】跳转用 EA 这个地址本身，不取 EA 处的内容（与 LD/ST 取值相反！）。\n"
 "【干什么】PR ← EA：下一条指令从 EA 处开始执行。无条件。\n"
 "【例】设 LOOP 的地址是 0x0010。JUMP LOOP → PR = 0x0010。"
 "带变址：JUMP TAB,GR1，TAB = 0x0040 且 GR1 里存的数是 3 → "
 "PR = 0x0043（从 TAB 起第 4 个字的地址）。",
 "        JUMP  LOOP"},

{"JPL", "条件转移", "JPL addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】条件满足时 PR ← EA，否则顺序执行。\n"
 "【条件】SF=0 且 ZF=0（上次运算结果为正）。\n"
 "【例】GR1 = 0x0005：SUBA GR1,#1 → GR1 = 4，结果为正 → JPL LOOP 跳转。"
 "GR1 = 0x0001 时减 1 得 0（ZF=1）→ 不跳。",
 "        SUBA  GR1,#1\n"
 "        JPL   LOOP     ; GR1 > 0 继续"},

{"JMI", "条件转移", "JMI addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】SF=1（结果为负）时 PR ← EA，否则顺序执行。\n"
 "【例】CPA GR0,#0 后 GR0 < 0（SF=1）→ JMI NEGPROC 跳走。",
 "        CPA   GR0,#0\n"
 "        JMI   NEGPROC"},

{"JNZ", "条件转移", "JNZ addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】ZF=0（结果非 0）时 PR ← EA，否则顺序执行。\n"
 "【例】上次运算结果是 0x0004（ZF=0）→ JNZ LOOP 跳转；结果为 0 则不跳。",
 "        JNZ   LOOP"},

{"JZE", "条件转移", "JZE addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】ZF=1（结果为 0）时 PR ← EA，否则顺序执行。\n"
 "【例】CPA GR0,#0 相等（ZF=1）→ JZE DONE 跳到 DONE。",
 "        CPA   GR0,#0\n"
 "        JZE   DONE"},

{"JOV", "条件转移", "JOV addr[,GRx]",
 "【算地址】目标 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】OF=1（溢出）时 PR ← EA，否则顺序执行。\n"
 "【例】GR0 = 0x7FFF，ADDL GR0,#1 → 0x8000，OF=1 → JOV OVERFLOW 跳走。",
 "        ADDL  GR0,#1\n"
 "        JOV   OVERFLOW"},

{"PUSH", "栈", "PUSH addr[,GRx]",
 "【算地址】EA = addr + GRx 里存的数（同 LD）。\n"
 "【干什么】SP 先减 1，再把内存 EA 处一个字的【内容】压入栈顶"
 "（栈向低地址生长）。注意：压的是 EA 处的值，不是 EA 地址本身。\n"
 "【例】设 SAVE 的地址是 0x0300 且该处存 0x1234。PUSH SAVE：\n"
 "SP 从 0xFFFE 变 0xFFFD，内存[0xFFFD] ← 0x1234"
 "（压内容 1234，不是地址 0300）。",
 "        PUSH  SAVEGR0"},

{"POP", "栈", "POP GR",
 "【干什么】从栈顶弹出一个字（16 位）装进 GR，SP 加 1。"
 "与 PUSH 配对恢复现场。\n"
 "【例】SP = 0xFFFD、内存[0xFFFD] = 0x1234。POP GR0 → GR0 = 0x1234，"
 "SP 回 0xFFFE。",
 "        POP   GR0"},

{"CALL", "子程序", "CALL addr[,GRx]",
 "【算地址】子程序入口 EA = addr + GRx 里存的数（同 JUMP，用地址本身）。\n"
 "【干什么】① 把返回地址（CALL 的下一条指令地址）压栈；"
 "② PR ← EA 跳入子程序。\n"
 "【例】设 SUB1 的地址是 0x0050，CALL 指令放在 0x0020（双字，"
 "占 0x0020..0x0021）：返回地址 0x0022 压栈，PR = 0x0050。"
 "RET 时弹出 0x0022 继续。",
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
    // escape HTML specials in the plain-text description, keep line breaks
    QString strDesc = QString::fromUtf8(entry.m_pszDesc);
    strDesc.replace('&', "&amp;");
    strDesc.replace('<', "&lt;");
    strDesc.replace('>', "&gt;");
    strDesc.replace('\n', "<br>");

    QString strHtml;
    strHtml += "<html><body>";
    strHtml += QString("<h2 style='color:%1;margin:0'>%2</h2>")
                   .arg("#2A82DA", QString::fromUtf8(entry.m_pszName));
    const QString strFull = FullNameOf(entry.m_pszName);
    if (!strFull.isEmpty())
        strHtml += QString(
                       "<p style='color:#888;margin:0 0 2px 0'>%1 &mdash; %2</p>")
                       .arg(QString::fromUtf8(entry.m_pszName), strFull);
    strHtml += QString("<p style='color:gray;margin:2px 0 10px 0'>%1</p>")
                   .arg(QString::fromUtf8(entry.m_pszCategory));
    strHtml += "<p><b>语法</b></p>";
    strHtml += QString(
                   "<pre style='background:#2D2D30;color:#DCDCDC;padding:8px;"
                   "border-radius:4px'>%1</pre>")
                   .arg(QString::fromUtf8(entry.m_pszSyntax));
    strHtml += "<p><b>说明</b></p>";
    strHtml += QString("<p>%1</p>").arg(strDesc);
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
