#include "input.hh"
#include "util.hh"
#include "main.hh"
#include "search.hh"

std::string
GetString(size_t maxlen)
{
    std::string input;
    echo();

    int ch = wgetch(bottomRow);
    size_t count {};
    while (ch != '\n' && count++ < maxlen) {
        input.push_back(ch);
        ch = wgetch(bottomRow);
    }

    noecho();
    return input;
}

void
ReadInput()
{
    int c;
    bool volume_changed = false;
    bool lockChanged = false;

    size_t maxlen {};

    while ( (c = getch()) ) {
        switch (c) {
            case 'q':
                State.exit = true;

                if (State.paused)
                    playCnd.notify_one();

                return;

            case 'o':
                State.next = true;
                break;

            case 'i':
                State.prev = true;
                break;

            case '0':
                State.volume += 0.003;
                volume_changed = true;
                break;
            case ')':
                State.volume += 0.001;
                volume_changed = true;
                break;

            case '9':
                State.volume -= 0.003;
                volume_changed = true;
                break;
            case '(':
                State.volume -= 0.001;
                volume_changed = true;
                break;

            case 'l':
            case 'L':
                State.right = true;
                break;

            case 'h':
            case 'H':
                State.left = true;
                break;

            case 'c':
            case ' ':
                State.paused = !State.paused;
                lockChanged = true;
                break;

            case 'j':
                State.goDown = true;
                State.inQSelected++;

                if (State.inQSelected > State.Size() - 1)
                    State.inQSelected = State.Size() - 1;

                PrintSongList();
                break;
            case 'k':
                State.goUp = true;
                State.inQSelected--;

                if (State.inQSelected < 0)
                    State.inQSelected = 0;

                PrintSongList();
                break;

            case 'g':
                State.inQSelected = 0;
                State.firstToDraw = 0;

                PrintSongList();
                break;
            case 'G':
                State.inQSelected = State.Size() - 1;
                State.firstToDraw = (State.Size() - 1) - songListSubWin->_maxy;

                PrintSongList();
                break;

            case 4: /* C-d */
            case KEY_NPAGE:
                State.inQSelected += 22;
                State.firstToDraw += 22;

                if (State.firstToDraw > State.Size()) {
                    State.firstToDraw = (State.Size() - 1) - songListSubWin->_maxy;
                }
                if (State.inQSelected >= State.Size()) {
                    State.inQSelected = State.Size() - 1;
                }

                PrintSongList();
                break;
            case 21: /* C-u */
            case KEY_PPAGE:
                State.inQSelected -= 22;
                State.firstToDraw -= 22;

                if (State.firstToDraw < 0)
                    State.firstToDraw = 0;
                if (State.inQSelected < 0)
                    State.inQSelected = 0;

                PrintSongList();
                break;

            case '\n':
                State.pressedEnter = true;
                State.inQ = State.inQSelected;
                break;

            case 12: /* C-l */
                PrintSongName();
                PrintVolume();
                RefreshWindows();
                PrintSongList();
                break;

            case 47: /* / */
                maxlen = std::max(size_t(length(sbuff) - 1), size_t(bottomRow->_maxx - 1));

                SubstringSearch(GetString(maxlen), true);

                MoveToFound(SeachNP::nochange);
                PrintSongList();

                wmove(bottomRow, 0, 0);
                wclrtobot(bottomRow);
                break;

            case 63: /* ? */
                maxlen = std::max(size_t(length(sbuff) - 1), size_t(bottomRow->_maxx - 1));

                SubstringSearch(GetString(maxlen), false);

                MoveToFound(SeachNP::nochange);
                PrintSongList();

                wmove(bottomRow, 0, 0);
                wclrtobot(bottomRow);
                break;

            case 'n':
                MoveToFound(SeachNP::forward);
                PrintSongList();
                break;
            case 'N':
                MoveToFound(SeachNP::backwards);
                PrintSongList();
                break;

            case KEY_RESIZE:
                PrintSongName();
                PrintVolume();
                RefreshWindows();
                PrintSongList();
                break;

            default:
#ifdef DEBUG
                PrintCharDebug(c);
#endif
                break;
        }

        State.volume = Clamp(State.volume, State.minVolume, State.maxVolume);

        if (volume_changed) {
            volume_changed = false;
            PrintVolume();
        }

        if (lockChanged && !State.paused) {
            lockChanged = false;
            playCnd.notify_one();
        }
    }
}
