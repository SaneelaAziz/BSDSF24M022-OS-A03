#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

// Run one command: a built-in in the shell itself, otherwise fork and exec
static void run_segment(char** seg, int background) {
    if (!handle_builtin(seg)) {
        execute(seg, background);
    }
}

// Split the token list at ';' and '&' and run the commands one after another.
// A command followed by '&' runs in the background (the shell does not wait).
static void run_line(char** arglist) {
    char* seg[MAXARGS + 1];
    int n = 0;

    for (int i = 0; arglist[i] != NULL; i++) {
        int is_semi = (strcmp(arglist[i], ";") == 0);
        int is_amp  = (strcmp(arglist[i], "&") == 0);

        if (is_semi || is_amp) {
            if (n == 0) {
                fprintf(stderr, "syntax error near unexpected token '%s'\n", arglist[i]);
                return;
            }
            seg[n] = NULL;
            run_segment(seg, is_amp);
            n = 0;
        } else {
            seg[n++] = arglist[i];
        }
    }

    if (n > 0) {   // last command, no separator after it
        seg[n] = NULL;
        run_segment(seg, 0);
    }
}

int main() {
    char* cmdline;
    char** arglist;

    while (1) {
        reap_jobs();   // collect finished background jobs (no zombies)

        cmdline = readline(PROMPT);
        if (cmdline == NULL) {
            break;     // Ctrl+D
        }

        // !n: replace the line with the n-th command from history
        if (cmdline[0] == '!') {
            char* end;
            long n = strtol(cmdline + 1, &end, 10);
            const char* cmd = NULL;
            if (end != cmdline + 1 && *end == '\0') {
                cmd = get_history_entry((int) n);
            }
            if (cmd == NULL) {
                printf("%s: event not found\n", cmdline);
                free(cmdline);
                continue;
            }
            char* copy = strdup(cmd);
            free(cmdline);
            cmdline = copy;
            printf("%s\n", cmdline);   // show which command is being run
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            add_history_entry(cmdline);   // our array: history and !n
            add_history(cmdline);         // readline: Up/Down arrow keys

            run_line(arglist);

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
