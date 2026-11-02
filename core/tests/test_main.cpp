// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include <cstring>

#include "test_framework.h"

// Usage: maba_core_tests [name-substring]
int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    for (const auto& t : maba_test::Registry()) {
        if (filter && std::strstr(t.name, filter) == nullptr) continue;
        const int before = maba_test::Failures();
        t.fn();
        ++run;
        if (maba_test::Failures() != before) std::printf("  in %s\n", t.name);
    }
    if (maba_test::Failures() == 0) {
        std::printf("all %d tests passed\n", run);
        return 0;
    }
    std::printf("%d check(s) failed\n", maba_test::Failures());
    return 1;
}
