#include "search.hh"
#include "util.hh"
#include "main.hh"

std::vector<long> searchIndices {};
long searchTarget;

void
LogHash(const std::string_view s)
{
    size_t h {std::hash<std::string_view>{}(s)};
    Printe("hash: {}\n", h);
}

void
SubstringSearch(const std::string_view s)
{
    bool found {false};
    searchIndices.clear();
    searchTarget = -1;

    for (size_t i = 0; i < State.songList.size(); i++) {
        auto e = State.songList[i];

        std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};

        if (delpath.find(s) != std::string::npos) {
            found = true;
            searchIndices.push_back(i);
        }
    }

    if (found) {
        searchTarget = 0;
    }
}

void
MoveToFound()
{
    if (searchTarget != - 1 && !searchIndices.empty())
        State.inQSelected = searchIndices[searchTarget++];

    if (searchTarget >= (long)searchIndices.size())
        searchTarget = 0;

    if (State.inQSelected < State.firstToDraw || State.inQSelected >= (State.firstToDraw + songListSubWin->_maxy)) {
        State.firstToDraw = State.inQSelected - State.scrolloff;
    }
}
