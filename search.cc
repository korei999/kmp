#include "search.hh"
#include "main.hh"
#include <algorithm>

std::vector<long> searchIndices {};
long searchTarget;

// size_t h {std::hash<std::string_view>{}(s)};

void
SubstringSearch(const std::string_view ss, bool forward)
{
    bool found {false};
    searchIndices.clear();
    searchTarget = -1;

    std::string s {ss};
    std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c){ return std::toupper(c); });

    if (forward) {
        for (size_t i = 0; i < State.songList.size(); i++) {
            auto e = State.songList[i];

            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};
            std::transform(delpath.begin(), delpath.end(), delpath.begin(),
                    [](unsigned char c){ return std::toupper(c); });
            

            if (delpath.find(s) != std::string::npos) {
                found = true;
                searchIndices.push_back(i);
            }
        }
    } else {
        for (long i = State.songList.size() - 1; i >= 0; i--) {
            auto e = State.songList[i];

            std::string delpath {e.substr(e.find_last_of("/") + 1, e.size())};
            std::transform(delpath.begin(), delpath.end(), delpath.begin(),
                    [](unsigned char c){ return std::toupper(c); });

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
    if (searchTarget != - 1) {
        searchTarget += (int)val;

        if (searchTarget >= (long)searchIndices.size()) {
            searchTarget = 0;
        } else if (searchTarget < 0) {
            searchTarget = searchIndices.size() - 1;
        }

        if (!searchIndices.empty())
            State.inQSelected = searchIndices[searchTarget];

        if (State.inQSelected < State.firstToDraw || State.inQSelected >= (State.firstToDraw + songListSubWin->_maxy)) {
            State.firstToDraw = State.inQSelected - State.scrolloff;
        }
    }
}