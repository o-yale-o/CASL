# CASL Studio — 桌面版 CASL / COMET II 汇编开发环境

面向**老软考（高级程序员）CASL 题目练习、教学与怀旧**的 Windows 桌面应用：
编辑、汇编、运行、单步调试、断点、寄存器 / 内存观察、输入输出模拟，一应俱全。

- 语言标准：**C++17**
- UI 框架：**Qt 6 Widgets**（本机使用 Qt 6.8.3 `msvc2022_64`）
- 代码编辑器：**QScintilla 2.14.1**（缺失时自动降级为内置 QPlainTextEdit 编辑器）
- 命名规范：**MFC / VC++ 风格匈牙利命名法**（类 `C` 前缀、枚举 `E` 前缀、成员 `m_` + 类型前缀、函数 PascalCase）

## 架构：核心逻辑与 UI 分离

```
┌───────────────────────────────────────────────┐
│ app/                  Qt Widgets 前端          │
│   CodeEditor   编辑器（QScintilla / 降级双实现） │
│   MachineRunner 调试执行引擎（后台线程、快照）    │
│   MainWindow   主窗口（工具栏、面板、装配动作）    │
│   Panels       寄存器/内存/符号表/控制台/错误面板  │
└───────────────┬───────────────────────────────┘
                │ 仅依赖纯 C++ 头文件
┌───────────────┴───────────────────────────────┐
│ core/                 casl-core 静态库（无 Qt）  │
│   Assembler    CASL II 两遍汇编器              │
│   Machine      COMET II 虚拟机（GR0-7/SP/PR/FR)│
│   OpCode       指令集定义                       │
│   tests/       24 项单元测试（纯标准库）          │
└───────────────────────────────────────────────┘
```

- **core 库零 Qt 依赖**，可单独编译、单独测试（`ctest`），未来可换成任何前端
  （CLI、Web、Qt Quick……）。
- UI 与机器之间通过 `CMachineRunner` 的命令队列 + 互斥锁保护的**快照**通信，
  机器跑在专用 `std::thread` 上，UI 永不卡死。

## 功能

| 功能 | 说明 |
| --- | --- |
| 编辑 | QScintilla 语法友好的等宽编辑器、行号、当前行高亮 |
| 汇编 | CASL II 全指令集：`LD/ST/ADDA/SUBA/ADDL/SUBL/AND/OR/XOR/CPA/CPL`、移位、`JUMP/JPL/JMI/JNZ/JZE/JOV`、`PUSH/POP/CALL/RET/SVC/NOP`、伪指令 `START/END/DS/DC`、宏 `IN/OUT/EXIT`、字面量 `#hex`、变址寄存器 |
| 运行 | **F5** 开始调试/继续，**Ctrl+F5** 开始执行(不调试、忽略断点)，**Shift+F5** 停止，**Ctrl+Shift+F5** 重新调试 —— 与 Visual Studio 习惯一致 |
| 调试 | **F10** 逐过程、**F11** 逐语句、**Shift+F11** 跳出（运行至当前子程序返回，即 SP 回升）、**F9** 切换断点（编辑器左边栏点击亦可）、当前执行行高亮；工具栏按钮为 VS 风格自绘图标 |
| 观察 | GR0–GR7 / SP / PR / OF·SF·ZF / 步数实时刷新；内存 8×8 十六进制窗口（高亮 PR，可调基址）；符号表 |
| 输入输出 | `OUT` 输出到控制台面板；`IN` 阻塞等待输入行（面板红色提示） |

## 构建与运行

### 依赖

- CMake ≥ 3.21 + Visual Studio 2022（MSVC v143）
- Qt 6（本仓库默认按 `C:\Qt\6.8.3\msvc2022_64` 配置，可在下面脚本中修改）
- QScintilla（可选）：仓库已内置针对 Qt 6.8.3 预编译的 `third_party/qscintilla`；
  也可用 `find_package(QScintilla)` 探测系统安装；都没有则自动用降级编辑器

### 一键构建（本仓库提供脚本）

```bat
build-app.cmd        :: 配置 + 编译（Release），自动清掉会导致 MSBuild 崩溃的重复代理环境变量
```

产物：`build\app\Release\CASLStudio.exe`（QScintilla DLL 自动拷贝到旁边）。

### 手动构建

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

### 仅构建/测试核心库（无 Qt）

```bat
cmake -S . -B build-core -DCASL_BUILD_UI=OFF
cmake --build build-core --config Debug
build-core\core\tests\Debug\test_assembler.exe
build-core\core\tests\Debug\test_machine.exe
```

### 重新编译 QScintilla（可选，仅当更换 Qt 版本时）

```bat
build-qscintilla.cmd    :: 下载源码后在 third_party 下针对本机 Qt 重编
```

## 示例程序（samples/）

| 文件 | 内容 |
| --- | --- |
| `hello.casl` | OUT 输出字符串 |
| `arith.casl` | 字面量算术（注意 `#10` 是十六进制 16） |
| `sum.casl` | 变址寄存器循环求和 |
| `sum10.casl` | 1..10 求和并十进制输出（软考经典：除 10 取余 + 倒序） |
| `bubble.casl` | 冒泡排序（软考经典），排序后输出 |
| `echo.casl` | IN/OUT 回显，空行退出 |

## CASL 语法提示

- `#` 前缀为**十六进制**：`LD GR0,#10` 装入的是 16。
- CASL 没有寄存器间 ADDA/SUBA（只有 `LD GRx,GRy`），需经内存字中转。
- `IN BUF,LNG` / `OUT BUF,LNG` 的第二个操作数是存放长度的标号。
- 栈向下生长，初始 `SP = 0xFFFE`；`CALL` 压入返回地址，`RET` 弹出。
- 程序以 `EXIT` 结束（汇编为 SVC 0）。

## 已知限制

- 位移指令的移位数取自有效地址所指内存（COMET II 语义），上限 16。
- 输入行最长 256 字符（超出截断），与 COMET II 常见实现一致。
