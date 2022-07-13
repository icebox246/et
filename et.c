#include <errno.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Todo {
    char* content;

    int flags;

    struct Todo *prev, *next;
} Todo;

enum ColorPairs {
    P_HEADER = 1,
    P_NORMAL,
    P_MARK,
};

Todo *todo_head, *done_head, *cursor;
int rows, cols;

#define F_DONE (1 << 0)
#define F_HEADER (1 << 1)

Todo* new_todo(char* txt) {
    Todo* todo = malloc(sizeof(Todo));
    memset(todo, 0, sizeof(Todo));

    int n = strlen(txt);
    todo->content = malloc(n + 1);
    memcpy(todo->content, txt, n);
    todo->content[n] = '\0';

    return todo;
}
Todo* new_header(char* txt) {
    Todo* todo = new_todo(txt);
    todo->flags = F_HEADER;
    return todo;
}

Todo* append_todo(Todo* dest, Todo* new) {
    new->next = dest->next;
    if (dest->next) dest->next->prev = new;
    new->prev = dest;
    dest->next = new;
    return new;
}

Todo* last_todo(Todo* t) {
    if (t->next) return last_todo(t->next);
    return t;
}

Todo* prepend_todo(Todo* dest, Todo* new) {
    new->prev = dest->prev;
    if (dest->prev) dest->prev->next = new;
    new->next = dest;
    todo_head->prev = new;
    return new;
}

int count_todos(Todo* head) {
    if (head && (head->flags & F_HEADER) == 0)
        return 1 + count_todos(head->next);
    return 0;
}

Todo* unlink_todo(Todo* todo) {
    if (todo->prev) todo->prev->next = todo->next;
    if (todo->next) todo->next->prev = todo->prev;
    todo->prev = todo->next = NULL;

    return todo;
}

void printw_todo(Todo* todo) {
    if (todo == cursor) attron(A_REVERSE);
    if ((todo->flags & F_HEADER) == 0) {
        attron(COLOR_PAIR(P_MARK));
        printw("[%c]", (todo->flags & F_DONE) ? 'x' : ' ');
        attroff(COLOR_PAIR(P_MARK));

        attron(COLOR_PAIR(P_NORMAL));
        printw(" %s\n", todo->content);
        attroff(COLOR_PAIR(P_NORMAL));
    } else {
        attron(COLOR_PAIR(P_HEADER));
        printw("\n# %s (%d):\n\n", todo->content, count_todos(todo->next));
        attroff(COLOR_PAIR(P_HEADER));
    }
    if (todo == cursor) attroff(A_REVERSE);
}

void fprint_todo(FILE* fd, Todo* todo) {
    if ((todo->flags & F_HEADER) == 0) {
        fprintf(fd, "- [%c] %s\n", (todo->flags & F_DONE) ? 'x' : ' ',
                todo->content);
    } else {
        fprintf(fd, "\n*%s (%d):*\n\n", todo->content, count_todos(todo->next));
    }
}

void swap_next_todo(Todo* todo) {
    if (!todo->next || (todo->next->flags & F_HEADER)) return;
    Todo* next = todo->next;
    unlink_todo(todo);
    append_todo(next, todo);
}

void swap_prev_todo(Todo* todo) {
    if (!todo->prev || (todo->prev->flags & F_HEADER)) return;
    swap_next_todo(todo->prev);
}

void destroy_todo(Todo* todo) {
    unlink_todo(todo);
    free(todo->content);
    free(todo);
}

const char* begin_todo_sect = "<!-- TODOS -->\n";
const char* end_todo_sect = "<!-- ENDTODOS -->\n";

#define MAX_LINE_LEN 1024
char line[MAX_LINE_LEN];
void save_changes(char* path) {
    FILE* ifd = fopen(path, "r");

    if (!ifd) {
        printw("[ERROR] Error occured when opening file: %s", strerror(errno));
        return;
    }

    FILE* tmpfd = fopen("et.tmp", "w+");

    line[0] = '\0';
    int ll = 0;
    int ignore_write = 0;
    int found_section = 0;

    while (!feof(ifd)) {
        char c = fgetc(ifd);
        if (!ignore_write && !feof(ifd)) {
            fputc(c, tmpfd);
        }
        if (ll < MAX_LINE_LEN) {
            line[ll++] = c;
            line[ll] = '\0';
        }
        if (c == '\n') {  // line end
            if (strcmp(line, begin_todo_sect) == 0 &&
                !ignore_write) {  // beginning of todo section
                ignore_write = 1;
                for (Todo* h = todo_head; h; h = h->next) {
                    fprint_todo(tmpfd, h);
                }
            }
            if (strcmp(line, end_todo_sect) == 0 &&
                ignore_write) {  // end of todo section
                ignore_write = 0;
                fprintf(tmpfd, "%s", line);
                found_section = 1;
            }
            ll = 0;  // reset line after endl
            line[ll] = '\0';
        }
    }

    fclose(ifd);
    fclose(tmpfd);

    if (!found_section) {
        printw("[ERROR] Provided file does not contain correct todo section\n");
        printw("        It should look like this:\n");
        printw("```\n\n%s...\n%s\n```\n", begin_todo_sect, end_todo_sect);
    }

    rename("et.tmp", path);
}

void parse_line(char* line) {
    switch (line[0]) {
        case '-': {  // todo
            int n = strlen(line);
            line[n - 1] = '\0';
            Todo* todo = new_todo(line + strlen("- [ ] "));
            todo->flags = line[3] == 'x' ? F_DONE : 0;
            append_todo(last_todo(todo_head), todo);
        } break;
        case '*': {  // header
            char buff[16] = {};
            int ignore;
            sscanf(line, "*%s (%d):*", buff, &ignore);
            Todo* head = new_header(buff);
            if (strcmp(buff, "TODO") == 0) {
                todo_head = head;
            } else if (strcmp(buff, "DONE") == 0) {
                done_head = head;
                append_todo(last_todo(todo_head), head);
            }
        } break;
    }
}

void load_data(char* path) {
    FILE* ofd = fopen(path, "r");

    if (!ofd) {
        printw("[ERROR] Error occured when opening file: %s", strerror(errno));
        return;
    }

    line[0] = '\0';
    int ll = 0;
    int parsing = 0;
    int found_section = 0;

    while (!feof(ofd)) {
        char c = fgetc(ofd);
        if (ll < MAX_LINE_LEN) {
            line[ll++] = c;
            line[ll] = '\0';
        }
        if (c == '\n') {  // line end
            if (strcmp(line, begin_todo_sect) == 0 &&
                !parsing) {  // beginning of todo section
                parsing = 1;
            }
            if (strcmp(line, end_todo_sect) == 0 &&
                parsing) {  // end of todo section
                parsing = 0;
                found_section = 1;
            }
            if (parsing) {
                parse_line(line);
            }
            ll = 0;  // reset line after endl
            line[ll] = '\0';
        }
    }

    fclose(ofd);

    if (!found_section) {
        printw("[ERROR] Provided file does not contain correct todo section\n");
        printw("        It should look like this:\n");
        printw("```\n\n%s...\n%s\n```\n", begin_todo_sect, end_todo_sect);
    }

    rename("et.tmp", path);
}

#define CTRL_KEYPRESS(k) ((k)&0x1f)

void readline(char* b) {
    int len = strlen(b);
    int cur = len;

    int startx = getcurx(stdscr);
    int starty = getcury(stdscr);
    mvprintw(starty, startx, "%s", b);
    move(starty, startx + cur);

    int c = getch();
    while (c != '\n') {
        switch (c) {
            case KEY_BACKSPACE: {
                if (!cur) break;
                for (int i = cur - 1; i < len; i++) {
                    b[i] = b[i + 1];
                }
                cur--;
                len--;
            } break;
            case KEY_LEFT: {
                if (!cur) break;
                cur--;
            } break;
            case KEY_RIGHT: {
                if (cur == len) break;
                cur++;
            } break;
            default: {
                for (int i = len; i >= cur; i--) {
                    b[i + 1] = b[i];
                }
                b[cur++] = c;
                len++;
            }
        }
        move(starty, startx);
        clrtobot();
        mvprintw(starty, startx, "%s", b);
        move(starty, startx + cur);
        c = getch();
    }
}

void prompt(char* p, char* o) {
    clrtobot();

    mvprintw(rows - 1, 0, "%s", p);
    curs_set(1);
    readline(o);
    curs_set(0);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    initscr();  // init ncurses
    noecho();
    raw();
    cbreak();
    keypad(stdscr, 1);

    use_default_colors();
    start_color();

    init_pair(P_HEADER, COLOR_BLUE, -1);
    init_pair(P_NORMAL, -1, -1);
    init_pair(P_MARK, COLOR_GREEN, -1);

    load_data(argv[1]);

    cursor = todo_head;

    int should_save = 0;

    int running = 1;
    while (running) {
        getmaxyx(stdscr, rows, cols);
        // render
        clear();
        // save progress
        if (should_save) {
            save_changes(argv[1]);
            should_save = 0;
        }
        curs_set(0);

        printw("et - Embeded TODOs\n");

        for (Todo* h = todo_head; h; h = h->next) {
            printw_todo(h);
        }

        refresh();

        // get input
        int c = getch();
        switch (c) {
            case 'q': {
                running = 0;
                should_save = 1;
            } break;
            case 'j':
            case KEY_DOWN: {
                if (cursor->next) cursor = cursor->next;
            } break;
            case 'k':
            case KEY_UP: {
                if (cursor->prev) cursor = cursor->prev;
            } break;
            case '\n':
            case ' ': {
                if (cursor->flags & F_HEADER) break;
                Todo* todo = cursor;
                if (todo->next && (todo->next->flags & F_HEADER) == 0)
                    cursor = todo->next;
                else if (todo->prev)
                    cursor = todo->prev;

                unlink_todo(todo);

                if ((todo->flags & F_DONE) == 0) {
                    append_todo(done_head, todo);
                } else {
                    append_todo(todo_head, todo);
                }

                todo->flags ^= F_DONE;
                should_save = 1;
            } break;

            case 'J': {
                if (cursor->flags & F_HEADER) break;
                swap_next_todo(cursor);
                should_save = 1;
            } break;
            case 'K': {
                if (cursor->flags & F_HEADER) break;
                swap_prev_todo(cursor);
                should_save = 1;
            } break;
            case 'X': {
                if (cursor->flags & F_HEADER) break;
                Todo* todo = cursor;
                if (todo->next && (todo->next->flags & F_HEADER) == 0)
                    cursor = todo->next;
                else if (todo->prev)
                    cursor = todo->prev;
                destroy_todo(todo);
                should_save = 1;
            } break;
            case 'i': {
                memset(line, 0, sizeof(line));
                prompt("Create: ", line);
                if (strlen(line) == 0) break;
                Todo* todo = new_todo(line);
                append_todo(todo_head, todo);
                cursor = todo;
                should_save = 1;
            } break;
            case 'c': {
                strcpy(line, cursor->content);
                prompt("Edit: ", line);
                int n = strlen(line);
                free(cursor->content);
                cursor->content = malloc(n + 1);
                strcpy(cursor->content, line);
                should_save = 1;
            } break;
        }
    }

    endwin();  // end ncurses
}
