// Minimal C++ test harness (no external dependency).
// Naming convention: MFC-style Hungarian notation.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace casltest {

class CTestCase {
public:
    const char* m_pszName;
    void (*m_pfnTest)();
};

inline std::vector<CTestCase>& Registry() {
    static std::vector<CTestCase> s_arr;
    return s_arr;
}
inline int& FailureCount() {
    static int s_n = 0;
    return s_n;
}
inline std::string& CurrentTest() {
    static std::string s_str;
    return s_str;
}

class CTestRegistrar {
public:
    CTestRegistrar(const char* pszName, void (*pfnTest)()) {
        Registry().push_back({pszName, pfnTest});
    }
};

#define CASL_TEST(name)                                                        \
    static void casl_test_##name();                                            \
    static casltest::CTestRegistrar casl_reg_##name(#name, &casl_test_##name); \
    static void casl_test_##name()

inline void Check(bool bCond, const char* pszExpr, int nLine) {
    if (!bCond) {
        ++FailureCount();
        std::printf("    FAIL [%s] line %d: %s\n", CurrentTest().c_str(), nLine,
                    pszExpr);
    }
}

#define CHECK(cond) casltest::Check((cond), #cond, __LINE__)
#define CHECK_EQ(a, b) casltest::Check((a) == (b), #a " == " #b, __LINE__)

inline int RunAll(const char* pszSuite) {
    std::printf("== %s: %zu tests ==\n", pszSuite, Registry().size());
    for (const auto& tc : Registry()) {
        CurrentTest() = tc.m_pszName;
        int nBefore = FailureCount();
        std::printf("  [run] %s\n", tc.m_pszName);
        tc.m_pfnTest();
        if (FailureCount() == nBefore) std::printf("  [ ok] %s\n", tc.m_pszName);
        else std::printf("  [FAIL] %s\n", tc.m_pszName);
    }
    if (FailureCount() == 0) {
        std::printf("== %s: ALL PASSED ==\n", pszSuite);
        return 0;
    }
    std::printf("== %s: %d FAILURE(S) ==\n", pszSuite, FailureCount());
    return 1;
}

} // namespace casltest
