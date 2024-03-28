#include "main.hh"
#include "search.hh"

std::vector<long> search_indices {};
long search_target = -1;

void
substring_search(const std::string_view s, bool forward)
{
    if (s.empty())
        return;

    bool found = false;
    search_indices.clear();
    search_target = -1;

    if (forward)
    {
        for (long i = 0; i < state.size(); i++)
        {
            auto e = state.song_list[i];
            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};

            if (delpath.find(s) != std::string::npos)
            {
                found = true;
                search_indices.push_back(i);
            }
        }
    }
    else
    {
        for (long i = state.size() - 1; i >= 0; i--)
        {
            auto e = state.song_list[i];
            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};

            if (delpath.find(s) != std::string::npos)
            {
                found = true;
                search_indices.push_back(i);
            }
        }
    }

    if (found)
        search_target = 0;
}

void
move_to_found(enum search_np val)
{
    if (search_target != -1 || !search_indices.empty())
    {
        search_target += (int)val;

        if (search_target >= (long)search_indices.size())
            search_target = 0;
        else if (search_target < 0)
            search_target = search_indices.size() - 1;

        state.in_q_selected = search_indices[search_target];

        if (state.in_q_selected < state.first_to_draw || state.in_q_selected >= (state.first_to_draw + song_list_sub_win->_maxy))
            state.first_to_draw = state.in_q_selected - state.scrolloff;
    }
}
