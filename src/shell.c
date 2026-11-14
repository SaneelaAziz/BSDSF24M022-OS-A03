#include "shell.h"

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;   // skip whitespace
        if (*cp == '\0') break;

        // Operators <, > and | are tokens on their own, spaces or not
        if (*cp == '<' || *cp == '>' || *cp == '|') {
            arglist[argnum][0] = *cp;
            arglist[argnum][1] = '\0';
            argnum++;
            cp++;
            continue;
        }

        // Ordinary word: stops at whitespace or an operator.
        // Text inside double quotes is kept together, quotes removed.
        int len = 0;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t' &&
               *cp != '<' && *cp != '>' && *cp != '|') {
            if (*cp == '"') {
                cp++;
                while (*cp != '\0' && *cp != '"') {
                    if (len < ARGLEN - 1) arglist[argnum][len++] = *cp;
                    cp++;
                }
                if (*cp == '"') cp++;
            } else {
                if (len < ARGLEN - 1) arglist[argnum][len++] = *cp;
                cp++;
            }
        }
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {   // nothing parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    // Free the unused slots and terminate the list with NULL
    for (int i = argnum; i <= MAXARGS; i++) {
        free(arglist[i]);
        arglist[i] = NULL;
    }
    return arglist;
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    // exit: terminate the shell process itself
    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    // cd: must run in the shell process, because chdir() changes
    // the working directory of the process that calls it
    if (strcmp(arglist[0], "cd") == 0) {
        const char* target = arglist[1];
        if (target == NULL) {
            target = getenv("HOME");   // plain "cd" goes home
        }
        if (target == NULL || chdir(target) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <directory>  Change the current directory\n");
        printf("  exit            Exit the shell\n");
        printf("  help            Show this help message\n");
        printf("  history         Show command history\n");
        printf("  jobs            List background jobs (not yet implemented)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;   // not a built-in
}


// ---- Command history storage ----
static char* hist[HISTORY_SIZE];
static int hist_count = 0;

void add_history_entry(const char* cmd) {
    if (cmd == NULL || cmd[0] == '\0') {
        return;
    }
    // History full: drop the oldest entry and shift the rest left
    if (hist_count == HISTORY_SIZE) {
        free(hist[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            hist[i - 1] = hist[i];
        }
        hist_count--;
    }
    hist[hist_count++] = strdup(cmd);
}

void print_history(void) {
    for (int i = 0; i < hist_count; i++) {
        printf("%d  %s\n", i + 1, hist[i]);
    }
}

// n is 1-based, matching the numbers shown by "history"
const char* get_history_entry(int n) {
    if (n < 1 || n > hist_count) {
        return NULL;
    }
    return hist[n - 1];
}
