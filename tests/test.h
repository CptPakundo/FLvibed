// FLvibed's host-free unit tests (build.cmd test -> build\test\fltest.exe): plain checks, no framework. Each test is one
// function `void Test_<name>()` in its own file, listed in main.cpp. CHECK counts and prints every failure with file(line)
// instead of stopping at the first; the exe's exit code is 1 when anything failed. Tests that need the test install
// (Install\Freelancer HD Edition: an argument, FLVIBED_INSTALL, or the repository's) call NeedInstall() and return when
// there is none: skipped, not failed. The core sources under test are compiled plain (no namespace wrapper) with
// tests\space_stub.cpp standing in for the game (core\game\space.cpp).
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct TestState
{
    int checks = 0, failures = 0, skipped = 0;
    bool verbose = false;
    std::string install;  // the test install's folder ("" none)
    std::string baseline; // tools\decision_baseline.txt
    std::string temp;     // this run's scratch folder (deleted at the end)
};
extern TestState g_test;

#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_test.checks;                                                                                               \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++g_test.failures;                                                                                         \
            fprintf(stderr, "%s(%d): CHECK failed: %s\n", __FILE__, __LINE__, #cond);                                  \
        }                                                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_test.checks;                                                                                               \
        const double va_ = static_cast<double>(a), vb_ = static_cast<double>(b);                                       \
        if (!(std::fabs(va_ - vb_) <= static_cast<double>(eps)))                                                       \
        {                                                                                                              \
            ++g_test.failures;                                                                                         \
            fprintf(stderr, "%s(%d): CHECK_NEAR failed: %s = %g, %s = %g (eps %g)\n", __FILE__, __LINE__, #a, va_, #b, \
                    vb_, static_cast<double>(eps));                                                                    \
        }                                                                                                              \
    } while (0)

// False (and the test counts as skipped) when there is no test install; `what` names what the test needs.
bool NeedInstall(const char* what);
// A path inside this run's scratch folder (`sub`: a sub-folder to create first, "" none).
std::string TempPath(const char* name, const char* sub = "");
bool WriteBytes(const std::string& path, const void* data, size_t size);
bool WriteTextFile(const std::string& path, const std::string& text);
// A text file's lines (CR stripped, a UTF-8 BOM dropped); empty when missing.
std::vector<std::string> ReadLines(const std::string& path);
