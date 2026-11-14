#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
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
            add_history_entry(cmdline);

            // Only fork and exec if the command is not a built-in
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

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
