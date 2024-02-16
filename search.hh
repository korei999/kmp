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
void MoveToFound(SeachNP val);

extern std::vector<long> searchIndices;
