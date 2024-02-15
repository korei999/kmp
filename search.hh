#pragma once
#include <string_view>
#include <vector>

enum class SeachNP : int {
    backwards = -1,
    nochange = 0,
    forward = 1
};

void LogHash(const std::string_view s);

void SubstringSearch(const std::string_view s, bool forward);

/* forward(-1: decrement target, 0: do not change target, 1: increment target) */
void MoveToFound(SeachNP val);

extern std::vector<long> searchIndices;
