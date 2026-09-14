#include "TestHarness.hpp"

#include "casl/Assembler.hpp"

using namespace casl;
using namespace casltest;

CASL_TEST(assembles_hello_world) {
    const char* pszSrc =
        "HELLO   START\n"
        "        OUT   MSG,LNG\n"
        "        EXIT\n"
        "MSG     DC    'HELLO'\n"
        "LNG     DC    5\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK_EQ(r.m_arrWords.size(), (size_t)10); // 3 (OUT) + 1 (EXIT) + 5 + 1
    CHECK(r.m_mapSymbols.count("MSG") && r.m_mapSymbols.count("LNG"));
}

CASL_TEST(label_optional_and_comments) {
    const char* pszSrc =
        "        START MAIN\n"
        "MAIN    LD    GR0,#0     ; load literal zero\n"
        "        RET\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK_EQ(r.m_nStartAddress, r.m_mapSymbols["MAIN"]);
}

CASL_TEST(literal_pool) {
    const char* pszSrc =
        "        START\n"
        "        LD    GR0,#10\n"
        "        LD    GR1,#0010\n"
        "        ADDA  GR0,#10\n"
        "        EXIT\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    // 2+2+2+1 code words = 7; literals: "#10" and "#0010" are distinct keys
    // -> 2 pooled words, both holding hex 0x10 = 16
    CHECK_EQ(r.m_arrWords.size(), (size_t)9);
    CHECK_EQ((int)r.m_arrWords[7], 16);
    CHECK_EQ((int)r.m_arrWords[8], 16);
}

CASL_TEST(register_to_register_ld_single_word) {
    const char* pszSrc =
        "        START\n"
        "        LD    GR0,GR1\n"
        "        LD    GR2,BUF\n"
        "        EXIT\n"
        "BUF     DC    7\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    // LD r,r = 1 word (opcode 0x14), LD with addr = 2 words, EXIT = 1
    CHECK_EQ((int)(r.m_arrWords[0] >> 8), 0x14);
    CHECK_EQ((int)(r.m_arrWords[0] & 0xF), 1);       // xr field = source GR1
    CHECK_EQ((int)((r.m_arrWords[0] >> 4) & 0xF), 0); // gr field = GR0
}

CASL_TEST(ds_reserves_words) {
    const char* pszSrc =
        "        START\n"
        "        EXIT\n"
        "BUF     DS    5\n"
        "AFTER   DC    1\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK_EQ(r.m_mapSymbols["AFTER"], 6); // EXIT(1) + DS(5)
}

CASL_TEST(dc_string_and_numeric) {
    const char* pszSrc =
        "        START\n"
        "        EXIT\n"
        "S       DC    'AB'\n"
        "N       DC    -1\n"
        "H       DC    #FF\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK_EQ((int)r.m_arrWords[r.m_mapSymbols["S"]], 'A');
    CHECK_EQ((int)r.m_arrWords[r.m_mapSymbols["S"] + 1], 'B');
    CHECK_EQ((int)(int16_t)r.m_arrWords[r.m_mapSymbols["N"]], -1);
    CHECK_EQ((int)r.m_arrWords[r.m_mapSymbols["H"]], 0x00FF);
}

CASL_TEST(duplicate_label_reported) {
    const char* pszSrc =
        "        START\n"
        "X       EXIT\n"
        "X       EXIT\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(!r.m_bOk);
    CHECK(r.m_arrErrors.size() >= 1);
}

CASL_TEST(missing_start_reported) {
    const char* pszSrc = "        EXIT\n        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(!r.m_bOk);
}

CASL_TEST(undefined_symbol_reported) {
    const char* pszSrc =
        "        START\n"
        "        LD    GR0,NOSUCH\n"
        "        EXIT\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(!r.m_bOk);
}

CASL_TEST(index_register_encoded) {
    const char* pszSrc =
        "        START\n"
        "        LD    GR0,BUF,GR7\n"
        "        EXIT\n"
        "BUF     DC    1\n"
        "        END\n";
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK_EQ((int)(r.m_arrWords[0] & 0xF), 7);
}

CASL_TEST(in_out_need_two_operands) {
    const char* pszBad = "        START\n        OUT MSG\n        EXIT\n"
                         "MSG     DC    'X'\n        END\n";
    CHECK(!Assemble(pszBad).m_bOk);
}

CASL_TEST(linemap_built_for_instructions) {
    const char* pszSrc =
        "        START\n"        // line 1
        "        LD    GR0,#1\n" // line 2
        "        EXIT\n"         // line 3
        "        END\n";         // line 4
    CAssembleResult r = Assemble(pszSrc);
    CHECK(r.m_bOk);
    CHECK(r.m_arrLineMap.size() >= 2);
    CHECK_EQ(r.m_arrLineMap[0].first, 2);
    CHECK_EQ(r.m_arrLineMap[0].second, 0);
    CHECK_EQ(r.m_arrLineMap[1].first, 3);
    CHECK_EQ(r.m_arrLineMap[1].second, 2);
}

int main() { return RunAll("assembler"); }
