#include "input.hh"
#include "util.hh"

void
ReadInput()
{
    char c;
    bool volume_changed = false;
    bool lockChanged = false;
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

                if (State.inQSelected > (long)State.songList.size() - 1)
                    State.inQSelected = (long)State.songList.size() - 1;

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
                State.inQSelected = State.songList.size() - 1;
                State.firstToDraw = (State.songList.size() - 1) - songListSubWin->_maxy;

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

            default:
#ifdef DEBUG
                PrintCharPressed(c);
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
