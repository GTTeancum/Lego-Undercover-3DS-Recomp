#pragma once
#include <stdexcept>
#include <string>
// LEGO host containment: unsupported Teakra paths stop the disposable probe,
// rather than aborting the complete native game process. Partial DSP effects
// are retained only for diagnostics; no guest success is committed.
[[noreturn]] inline void Assert(const char* expression, const char* file, int line) {
    throw std::runtime_error(std::string("Teakra assertion: ")+expression+" @"+file+":"+std::to_string(line));
}
#define ASSERT(EXPRESSION) ((EXPRESSION) ? (void)0 : Assert(#EXPRESSION, __FILE__, __LINE__))
#define UNREACHABLE() Assert("UNREACHABLE", __FILE__, __LINE__)
