// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#include <cstring>
#include <iostream>
#include <string>

#include "test_framework.h"

namespace maba_test {
std::string g_self_path;  // this executable, used as a line-echo peer by process tests
}

// Usage: maba_core_tests [name-substring]
//        maba_core_tests --echo-peer   (echo stdin lines to stdout until EOF)
int main(int argc, char** argv) {
    maba_test::g_self_path = argv[0];
    if (argc > 1 && std::strcmp(argv[1], "--echo-peer") == 0) {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::cout << line << std::endl;  // endl flushes: the peer must not buffer
        }
        return 0;
    }
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
