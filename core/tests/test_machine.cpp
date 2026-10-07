#include "TestHarness.hpp"

#include "casl/Assembler.hpp"
#include "casl/Machine.hpp"

#include <string>

using namespace casl;
using namespace casltest;

namespace {

// Assemble + load a program; aborts the test with CHECKs on failure.
CMachine MakeMachine(const CAssembleResult& r) {
    CHECK(r.m_bOk);
    CMachine m;
    m.Load(r.m_arrWords, r.m_nStartAddress);
    return m;
}

// Run until halt, with a safety cap.
void RunToHalt(CMachine& m, int nMaxSteps = 100000) {
    int n = 0;
    while (m.CanStep() && n++ < nMaxSteps) m.Step();
    CHECK(m.GetState() == ERunState::stHalted);
}

} // namespace

CASL_TEST(arithmetic_adda_result) {
    // NOTE: '#' literals are hexadecimal: #10 = 16, so 16 + 5 - 3 = 18.
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#10\n"
        "        ADDA  GR0,#5\n"
        "        SUBA  GR0,#3\n"
        "        ST    GR0,ANS\n"
        "        EXIT\n"
        "ANS     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]), 18);
}

CASL_TEST(flags_and_compare_jump) {
    // Count down GR0 from 5 to 0 using CPA and JZE.
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#5\n"
        "LOOP    CPA   GR0,#0\n"
        "        JZE   DONE\n"
        "        SUBA  GR0,#1\n"
        "        JUMP  LOOP\n"
        "DONE    ST    GR0,ANS\n"
        "        EXIT\n"
        "ANS     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]), 0);
}

CASL_TEST(call_ret_push_pop) {
    // '#' literals are hex: PUSH #21 pushes 33. The subroutine doubles GR0
    // via ST + ADDA (CASL has no register-register ADDA).
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#7\n"
        "        PUSH  #21\n"
        "        CALL  DOUBLE\n"
        "        POP   GR1\n"
        "        ST    GR0,ANS\n"
        "        ST    GR1,POPANS\n"
        "        EXIT\n"
        "DOUBLE  ST    GR0,WORK\n"
        "        ADDA  GR0,WORK\n"
        "        RET\n"
        "WORK    DS    1\n"
        "ANS     DS    1\n"
        "POPANS  DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]), 14);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["POPANS"]), 33);
}

CASL_TEST(svc_out_writes_string) {
    CAssembleResult r = Assemble(
        "HELLO   START\n"
        "        OUT   MSG,LNG\n"
        "        EXIT\n"
        "MSG     DC    'HELLO CASL'\n"
        "LNG     DC    10\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    std::string strGot;
    m.SetOutputHook([&](const std::string& strText) { strGot += strText; });
    RunToHalt(m);
    CHECK_EQ(strGot, std::string("HELLO CASL"));
}

CASL_TEST(svc_in_reads_line) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        IN    BUF,LNG\n"
        "        OUT   BUF,LNG\n"
        "        EXIT\n"
        "BUF     DS    256\n"
        "LNG     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    std::string strGot;
    m.SetInputHook([] { return std::string("ABC"); });
    m.SetOutputHook([&](const std::string& strText) { strGot += strText; });
    RunToHalt(m);
    CHECK_EQ(strGot, std::string("ABC"));
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["LNG"]), 3);
}

CASL_TEST(shifts) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#0001\n"
        "        SLL   GR0,#3\n"      // 1 << 3 = 8
        "        ST    GR0,A\n"
        "        LD    GR1,#0FFF\n"
        "        SLL   GR1,#4\n"      // 0xFFF0
        "        SRA   GR1,#4\n"      // arithmetic: 0xFFF0 is -16, -16>>4 = -1
        "        ST    GR1,B\n"
        "        LD    GR2,#8000\n"
        "        SRA   GR2,#4\n"      // sign-extended: 0xF800
        "        ST    GR2,C\n"
        "        LD    GR3,#FFFE\n"
        "        SLA   GR3,#1\n"      // sign kept: 0xFFFC
        "        ST    GR3,D\n"
        "        EXIT\n"
        "A       DS    1\n"
        "B       DS    1\n"
        "C       DS    1\n"
        "D       DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["A"]), 8);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["B"]) & 0xFFFF, 0xFFFF);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["C"]) & 0xFFFF, 0xF800);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["D"]) & 0xFFFF, 0xFFFC);
}

CASL_TEST(logical_ops) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#0F0F\n"
        "        AND   GR0,#00FF\n"   // 0x000F
        "        ST    GR0,A\n"
        "        LD    GR1,#0F0F\n"
        "        OR    GR1,#00F0\n"   // 0x0FFF
        "        ST    GR1,B\n"
        "        LD    GR2,#0FF0\n"
        "        XOR   GR2,#0FF0\n"   // 0
        "        ST    GR2,C\n"
        "        EXIT\n"
        "A       DS    1\n"
        "B       DS    1\n"
        "C       DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["A"]) & 0xFFFF, 0x000F);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["B"]) & 0xFFFF, 0x0FFF);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["C"]), 0);
}

CASL_TEST(addl_unsigned_carry) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#7FFF\n"
        "        ADDL  GR0,#1\n"     // 0x8000 unsigned, carry set
        "        ST    GR0,A\n"
        "        EXIT\n"
        "A       DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["A"]) & 0xFFFF, 0x8000);
}

CASL_TEST(loop_with_index_register) {
    // Sum BUF[0..4] using GR1 as index; compare against #-1 (0xFFFF) so the
    // element at index 0 is included before exiting.
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#0\n"
        "        LD    GR1,#4\n"
        "LOOP    ADDA  GR0,BUF,GR1\n"
        "        SUBA  GR1,#1\n"
        "        CPA   GR1,#FFFF\n"
        "        JPL   LOOP\n"
        "        ST    GR0,SUM\n"
        "        EXIT\n"
        "BUF     DC    1\n"
        "        DC    2\n"
        "        DC    3\n"
        "        DC    4\n"
        "        DC    5\n"
        "SUM     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["SUM"]), 15);
}

CASL_TEST(step_hook_pauses) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#1\n"
        "        EXIT\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    int nCalls = 0;
    m.SetStepHook([&](uint16_t) { return ++nCalls <= 1; }); // stop after 1 step
    CHECK(m.Step());
    CHECK(!m.Step());
    CHECK_EQ(nCalls, 2);
}

CASL_TEST(illegal_opcode_reports_error) {
    CMachine m;
    std::vector<uint16_t> arrBad = {0xFFFF};
    m.Load(arrBad, 0);
    m.Step();
    CHECK(m.GetState() == ERunState::stError);
}

CASL_TEST(reset_restores_state) {
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR0,#7\n"
        "        EXIT\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK(m.GetState() == ERunState::stHalted);
    m.Reset();
    CHECK(m.GetState() == ERunState::stReady);
    CHECK_EQ((int)m.Gr(0), 0);
    CHECK_EQ((int)m.Pr(), r.m_nStartAddress);
    CHECK_EQ((int)m.Sp(), 0xFFFE);
}

CASL_TEST(casl2_lad_rpush_rpop) {
    // LAD loads the effective address itself (no flags); RPUSH saves
    // GR1..GR7, the subroutine clobbers them, RPOP restores everything.
    CAssembleResult r = Assemble(
        "        START\n"
        "        LAD   GR1,BUF,GR2\n"
        "        LD    GR0,0,GR1\n"
        "        RPUSH\n"
        "        LD    GR1,#AAAA\n"
        "        LD    GR2,#BBBB\n"
        "        RPOP\n"
        "        ST    GR0,ANS\n"
        "        EXIT\n"
        "BUF     DC    #1111\n"
        "        DC    #2222\n"
        "        DC    #3333\n"
        "        DC    #4444\n"
        "        DC    #5555\n"
        "ANS     DS    1\n"
        "        END\n");
    // set GR2 = 3 before... source has no such step; instead verify via the
    // simpler route: GR2 is 0 at start, so EA = BUF + 0.
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Gr(1), r.m_mapSymbols["BUF"]); // LAD loaded the address
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]), 0x1111); // BUF[0] content
    // GR1/GR2 were clobbered to #AAAA/#BBBB mid-run; RPOP restored the
    // original values (GR1 = BUF address, GR2 = 0), stack balanced
    CHECK_EQ((int)m.Gr(1), r.m_mapSymbols["BUF"]);
    CHECK_EQ((int)m.Gr(2), 0);
    CHECK_EQ((int)m.Sp(), 0xFFFE);  // stack balanced
}

CASL_TEST(casl2_lad_with_index) {
    // LAD GR1,BUF,GR2 with GR2=2 -> GR1 = BUF+2 (address arithmetic)
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR2,#2\n"
        "        LAD   GR1,BUF,GR2\n"
        "        ST    GR1,ANS\n"
        "        EXIT\n"
        "BUF     DS    4\n"
        "ANS     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]),
             r.m_mapSymbols["BUF"] + 2);
}

CASL_TEST(legacy_lea_jpz_add_sub_eor_jmp) {
    // Old CASL mnemonics must assemble and behave correctly:
    // - LEA sets flags (unlike LAD): LEA GR1,-1,GR1 + JZE is the classic
    //   "decrement and test for zero" idiom
    // - JPZ jumps when not minus (plus OR zero)
    // - ADD/SUB/EOR/JMP behave exactly like ADDA/SUBA/XOR/JUMP
    CAssembleResult r = Assemble(
        "        START\n"
        "        LD    GR1,#3\n"
        "LOOP    LEA   GR1,-1,GR1\n"
        "        JZE   DONE\n"
        "        ADD   GR2,#1\n"
        "        JMP   LOOP\n"
        "DONE    LD    GR0,#5\n"
        "        CPA   GR0,#0\n"
        "        JPZ   POS\n"
        "        LD    GR3,#F\n"
        "        SUB   GR3,#1\n"
        "        EOR   GR3,#FFFF\n"
        "        ST    GR3,ANS\n"
        "        EXIT\n"
        "POS     ST    GR2,ANS\n"
        "        EXIT\n"
        "ANS     DS    1\n"
        "        END\n");
    CMachine m = MakeMachine(r);
    RunToHalt(m);
    // loop: GR1 3->2->1->0, so GR2 was incremented twice -> ANS = 2
    CHECK_EQ((int)m.Mem(r.m_mapSymbols["ANS"]), 2);
    // spot-check the alias encoding is identical to the modern spelling
    CAssembleResult a = Assemble(
        "        START\n"
        "        ADD   GR0,#1\n"
        "        SUB   GR0,#1\n"
        "        EOR   GR0,#0\n"
        "        JMP   FIN\n"
        "FIN     EXIT\n"
        "        END\n");
    CAssembleResult b = Assemble(
        "        START\n"
        "        ADDA  GR0,#1\n"
        "        SUBA  GR0,#1\n"
        "        XOR   GR0,#0\n"
        "        JUMP  FIN\n"
        "FIN     EXIT\n"
        "        END\n");
    CHECK(a.m_bOk && b.m_bOk);
    CHECK(a.m_arrWords == b.m_arrWords); // identical machine images
}

int main() { return RunAll("machine"); }
