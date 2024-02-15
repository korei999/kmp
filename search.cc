#include "search.hh"
#include "main.hh"

std::vector<long> searchIndices {};
long searchTarget {-1};

// size_t h {std::hash<std::string_view>{}(s)};

void
SubstringSearch(const std::string_view s, bool forward)
{
    bool found {false};
    searchIndices.clear();
    searchTarget = -1;

    if (forward) {
        for (size_t i = 0; i < State.songList.size(); i++) {
            auto e = State.songList[i];

            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};
            

            if (delpath.find(s) != std::string::npos) {
                found = true;
                searchIndices.push_back(i);
            }
        }
    } else {
        for (long i = State.songList.size() - 1; i >= 0; i--) {
            auto e = State.songList[i];

            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};

            if (delpath.find(s) != std::string::npos) {
                found = true;
                searchIndices.push_back(i);
            }
        }
    }

    if (found) {
        searchTarget = 0;
    }
}

void
MoveToFound(SeachNP val)
{
    if (searchTarget != - 1 || !searchIndices.empty()) {
        searchTarget += (int)val;

        if (searchTarget >= (long)searchIndices.size()) {
            searchTarget = 0;
        } else if (searchTarget < 0) {
            searchTarget = searchIndices.size() - 1;
        }

        State.inQSelected = searchIndices[searchTarget];

        if (State.inQSelected < State.firstToDraw || State.inQSelected >= (State.firstToDraw + songListSubWin->_maxy)) {
            State.firstToDraw = State.inQSelected - State.scrolloff;
        }
    }
}
