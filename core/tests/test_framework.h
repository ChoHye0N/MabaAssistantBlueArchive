// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Minimal dependency-free test runner. Replace with GoogleTest/Catch2 if the suite grows.
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace maba_test {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& Failures() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

}  // namespace maba_test

#define MABA_TEST(name)                                                   \
    static void name();                                                   \
    static ::maba_test::Registrar name##_registrar(#name, &name);         \
    static void name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
            ++::maba_test::Failures();                                    \
        }                                                                 \
    } while (0)
