// Debug dump utility: assemble a .casl file, print words, symbols, and a
// short execution trace. Not shipped with the GUI; build-time helper.
#include "casl/Assembler.hpp"
#include "casl/Machine.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

using namespace casl;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: casldump <file.casl> [--run]\n");
        return 1;
    }
    std::ifstream ifs(argv[1]);
    std::stringstream ss;
    ss << ifs.rdbuf();
    CAssembleResult r = Assemble(ss.str());
    std::printf("ok=%d errors=%zu words=%zu start=%d\n", (int)r.m_bOk,
                r.m_arrErrors.size(), r.m_arrWords.size(), r.m_nStartAddress);
    for (const auto& err : r.m_arrErrors)
        std::printf("  ERR line %d: %s\n", err.m_nLine, err.m_strMessage.c_str());
    for (size_t i = 0; i < r.m_arrWords.size(); ++i)
        std::printf("%04X: %04X\n", (unsigned)i, (unsigned)r.m_arrWords[i]);
    std::printf("symbols:\n");
    for (const auto& kv : r.m_mapSymbols)
        std::printf("  %s = %04X\n", kv.first.c_str(), (unsigned)kv.second);

    if (argc > 2 && std::string(argv[2]) == "--run") {
        CMachine m;
        m.SetOutputHook([](const std::string& strText) {
            std::printf("[out] '%s'\n", strText.c_str());
        });
        m.SetInputHook([] { return std::string("ABC"); });
        m.Load(r.m_arrWords, r.m_nStartAddress);
        int n = 0;
        while (m.CanStep() && n++ < 100000) {
            std::printf("PR=%04X GR0=%04X GR1=%04X\n", (unsigned)m.Pr(),
                        (unsigned)(uint16_t)m.Gr(0), (unsigned)(uint16_t)m.Gr(1));
            m.Step();
        }
        std::printf("final state=%d steps=%d err=%s\n", (int)m.GetState(),
                    m.Steps(), m.GetLastError().c_str());
    }
    return 0;
}
