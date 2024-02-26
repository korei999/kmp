#pragma once
#include <string_view>
#include <vector>

enum class seach_np : int
{
    backwards = -1,
    nochange = 0,
    forward = 1
};

void log_hash(const std::string_view s);
void substring_search(const std::string_view s, bool forward);
void move_to_found(seach_np val);

extern std::vector<long> search_indices;
