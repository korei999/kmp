#pragma once
#include <string_view>
#include <vector>

void LogHash(const std::string_view s);
void SubstringSearch(const std::string_view s);
void MoveToFound(int forward);

extern std::vector<long> searchIndices;
