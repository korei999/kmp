#include "input.hh"
#include "util.hh"
#include "main.hh"
#include "search.hh"

#include <unistd.h>

std::string
get_string(size_t maxlen, bool forward)
{
    state.searching = true;
    std::lock_guard lock(print_mtx);

    forward ? waddch(bottom_row, '/') : waddch(bottom_row, '?');
    wrefresh(bottom_row);

    char buffer[255] {};
    echo();
    wgetnstr(bottom_row, buffer, std::min(length(buffer) - 1, maxlen));
    noecho();

    state.searching = false;
    return std::string(buffer);
}

void
read_input()
{
    int c;
    bool volume_changed = false;
    bool lock_changed = false;

    size_t maxlen = 0;

    while ((c = getch()))
    {
        switch (c)
        {
            case 'q':
                state.exit = true;

                if (state.paused)
                    play_cnd.notify_one();

                return;

            case 'o':
                state.next = true;
                break;

            case 'i':
                state.prev = true;
                break;

            case '0':
                state.volume += 0.003;
                volume_changed = true;
                break;
            case ')':
                state.volume += 0.001;
                volume_changed = true;
                break;

            case '9':
                state.volume -= 0.003;
                volume_changed = true;
                break;
            case '(':
                state.volume -= 0.001;
                volume_changed = true;
                break;

            case 'l':
            case 'L':
                state.right = true;
                break;

            case 'h':
            case 'H':
                state.left = true;
                break;

            case 'c':
            case ' ':
                state.paused = !state.paused;
                lock_changed = true;
                break;

            case 'j':
                state.go_down = true;
                state.in_q_selected++;

                if (state.in_q_selected > state.size() - 1)
                    state.in_q_selected = state.size() - 1;

                print_song_list();
                break;
            case 'k':
                state.go_up = true;
                state.in_q_selected--;

                if (state.in_q_selected < 0)
                    state.in_q_selected = 0;

                print_song_list();
                break;

            case 'g':
                state.in_q_selected = 0;
                state.first_to_draw = 0;

                print_song_list();
                break;
            case 'G':
                state.in_q_selected = state.size() - 1;
                state.first_to_draw = (state.size() - 1) - song_list_sub_win->_maxy;

                print_song_list();
                break;

            case 4: /* C-d */
            case KEY_NPAGE:
                state.in_q_selected += 22;
                state.first_to_draw += 22;

                if (state.first_to_draw > state.size())
                    state.first_to_draw = (state.size() - 1) - song_list_sub_win->_maxy;
                if (state.in_q_selected >= state.size())
                    state.in_q_selected = state.size() - 1;

                print_song_list();
                break;
            case 21: /* C-u */
            case KEY_PPAGE:
                state.in_q_selected -= 22;
                state.first_to_draw -= 22;

                if (state.first_to_draw < 0)
                    state.first_to_draw = 0;
                if (state.in_q_selected < 0)
                    state.in_q_selected = 0;

                print_song_list();
                break;

            case '\n':
                state.pressed_enter = true;
                state.in_q = state.in_q_selected;
                break;

            case KEY_RESIZE:
            case 12: /* C-l */
                endwin();
                print_song_name();
                print_volume();
                refresh_windows();
                print_song_list();
                break;

            case 47: /* / */
                maxlen = std::max(size_t(length(sbuff) - 1), size_t(bottom_row->_maxx - 1));

                substring_search(get_string(maxlen, true), true);

                move_to_found(search_np::nochange);
                print_song_list();

                wmove(bottom_row, 0, 0);
                wclrtobot(bottom_row);
                break;

            case 63: /* ? */
                maxlen = std::max(size_t(length(sbuff) - 1), size_t(bottom_row->_maxx - 1));

                substring_search(get_string(maxlen, false), false);

                move_to_found(search_np::nochange);
                print_song_list();

                wmove(bottom_row, 0, 0);
                wclrtobot(bottom_row);
                break;

            case 'n':
                move_to_found(search_np::forward);
                print_song_list();
                break;
            case 'N':
                move_to_found(search_np::backwards);
                print_song_list();
                break;

            case 'r':
                state.repeat_on_end = !state.repeat_on_end;
                print_song_name();
                refresh_windows();
                break;

            default:
#ifdef DEBUG
                print_char_debug(c);
#endif
                break;
        }

        state.volume = Clamp(state.volume, state.min_volume, state.max_volume);

        if (volume_changed)
        {
            volume_changed = false;
            print_volume();
        }

        if (lock_changed && !state.paused)
        {
            lock_changed = false;
            play_cnd.notify_one();
        }
    }
}

#ifdef DEBUG
void
print_char_debug(char c)
{
    std::lock_guard pl(print_mtx);

    std::string_view fmt = "pressed: %c(%d)";
    move(stdscr->_maxy, (stdscr->_maxx - fmt.size()));
    clrtoeol();
    printw(fmt.data(), c, c);
    std::string_view fmt2 = "size: %lu, maxx: %d, maxy: %d";
    mvprintw(stdscr->_maxy, 0, fmt2.data(), state.song_list.size(), stdscr->_maxx, stdscr->_maxy);
}
#endif
