// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
// Turning OCR text into numbers. Digits are language-independent, so this is the preferred way
// to read values (AP, ticket counts, prices). Conservative: anything ambiguous is rejected.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "maba/ocr.h"

namespace maba {

// "1,234" / "1 234" / "1.234" (thousands separators) -> 1234. Rejects empty text, letters,
// signs, more than 12 digits and separators in odd places ("12,34" is rejected).
std::optional<long long> ParseInteger(const std::string& text);

struct Fraction {
    long long value = 0;
    long long max = 0;
};

// "120/240" (spaces allowed around '/') -> {120, 240}. Each side follows ParseInteger.
std::optional<Fraction> ParseFraction(const std::string& text);

// Joins OCR pieces left to right (by box x) without separators, e.g. {"12", "0/240"} -> "120/240".
std::string JoinLeftToRight(std::vector<OcrResult> pieces);

}  // namespace maba
