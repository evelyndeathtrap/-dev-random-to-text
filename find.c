#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <ncurses.h>

int min(int a, int b, int c) {
    if (a <= b && a <= c) return a;
    if (b <= a && b <= c) return b;
    return c;
}

int levenshtein(const char *s1, const char *s2, int *v0, int *v1) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    if (len2 > 512) len2 = 512; 
    for (int i = 0; i <= len2; i++) v0[i] = i;
    for (int i = 0; i < len1; i++) {
        v1[0] = i + 1;
        for (int j = 0; j < len2; j++) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            v1[j + 1] = min(v1[j] + 1, v0[j + 1] + 1, v0[j] + cost);
        }
        for (int j = 0; j <= len2; j++) v0[j] = v1[j];
    }
    return v0[len2];
}

int main() {
    const char *filename = "cwords.txt";
    int target_len = 8;
    int ideal_dist = target_len / 2;
    char target[target_len + 1];
    unsigned char buf[target_len];

    char *line = NULL;
    size_t line_cap = 0;
    int *v0 = malloc(513 * sizeof(int));
    int *v1 = malloc(513 * sizeof(int));

    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd < 0) { perror("urandom"); free(v0); free(v1); return 1; }

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    WINDOW *input_win = newwin(1, cols, 0, 0);
    WINDOW *output_win = newwin(rows - 2, cols, 1, 0);
    scrollok(output_win, TRUE);

    char user_input[256] = {0};
    char final_output[256] = {0}; // Buffer to save the last input for the very end
    int input_idx = 0;
    time_t last_gen_time = 0;
    unsigned char writee = 0;

    while (1) {
        int ch = getch();
        if (ch != ERR) {
            if (ch == 27) break; // ESC to exit
            
            if (ch == '\n' || ch == KEY_ENTER) {
                writee = 1; 
                strncpy(final_output, user_input, 255); // Save it before clearing
                
                memset(user_input, 0, sizeof(user_input));
                input_idx = 0;
                wclear(input_win);
                wrefresh(input_win);
            } else if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && input_idx > 0) {
                user_input[--input_idx] = '\0';
            } else if (input_idx < 250 && ch >= 32 && ch <= 126) {
                user_input[input_idx++] = (char)ch;
            }

            wmove(input_win, 0, 0);
            wclrtoeol(input_win);
            mvwprintw(input_win, 0, 0, "%s", user_input);
            wrefresh(input_win);
        }

        // --- Generate Word Cycle (Re-open file) ---
        time_t now = time(NULL);
        if (now - last_gen_time >= 3) {
            FILE *fp = fopen(filename, "r");
            if (fp && read(urandom_fd, buf, target_len) == target_len) {
                for (int i = 0; i < target_len; i++) target[i] = 'a' + (buf[i] % 26);
                target[target_len] = '\0';

                char best_word[256] = "";
                int best_diff = INT_MAX;

                while (getline(&line, &line_cap, fp) != -1) {
                    size_t r_len = strlen(line);
                    if (r_len > 0 && line[r_len - 1] == '\n') line[--r_len] = '\0';
                    if (r_len == 0 || r_len > 512) continue;

                    int dist = levenshtein(target, line, v0, v1);
                    int diff = abs(dist - ideal_dist);
                    if (diff < best_diff) {
                        best_diff = diff;
                        strncpy(best_word, line, 255);
                        if (best_diff == 0) break;
                    }
                }
                wprintw(output_win, "%s ", best_word);
                wrefresh(output_win);
                fclose(fp);
            }
            last_gen_time = now;
        }

        if (writee) {
            writee = 0; 
            FILE* fp_rand = fopen("/dev/random", "wb");
            if (fp_rand) {
                fwrite(final_output, 1, sizeof(final_output), fp_rand);
                fclose(fp_rand);
            }
            final_output[sizeof(final_output)] = ' ';
            final_output[sizeof(final_output+1)] = '\0';
            fwrite(final_output, 1, sizeof(final_output+1), stdout);
            char s  = ' ';
            fflush(stdout);
        }

        usleep(300000);
    }

    // --- CLEAN EXIT ---
    endwin(); // Close ncurses
    
    // Print user_input (or the last submitted input) at the end
    if (strlen(final_output) > 0) {
        printf("\nLast submitted input: %s\n", final_output);
    } else {
        printf("\nNo input was submitted.\n");
    }

    free(v0);
    free(v1);
    free(line);
    close(urandom_fd);
    return 0;
}
