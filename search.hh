#pragma once
#include <string_view>
#include <vector>

void LogHash(const std::string_view s);
void SubstringSearch(const std::string_view s);
void MoveToFound();

extern std::vector<long> searchIndices;
