#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_BLOCK 32

static void free_arglist(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
}

// Run one command: a built-in in the shell itself, otherwise fork and exec
static void run_segment(char** seg, int background) {
    last_status = 0;                 // built-ins succeed unless they say otherwise
    if (!handle_builtin(seg)) {
        execute(seg, background);    // sets last_status for foreground commands
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
                last_status = 1;
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

// Tokenize and run one stored line of an if-block
static void run_command_line(char* line) {
    char** arglist = tokenize(line);
    if (arglist != NULL) {
        run_line(arglist);
        free_arglist(arglist);
    }
}

// Read and run an if-then-else-fi block. first_line is the original
// "if ..." line; the following lines are read with the "> " prompt.
static void handle_if(const char* first_line) {
    char* cond[MAX_BLOCK];
    char* thenb[MAX_BLOCK];
    char* elseb[MAX_BLOCK];
    int ncond = 0, nthen = 0, nelse = 0;
    enum { IN_COND, IN_THEN, IN_ELSE } state = IN_COND;
    int ok = 1, saw_fi = 0;

    // The condition starts after the word "if" on the first line
    const char* p = first_line;
    while (*p == ' ' || *p == '\t') p++;
    p += 2;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\0') {
        cond[ncond++] = strdup(p);
    }

    while (!saw_fi) {
        char* line = readline("> ");
        if (line == NULL) {
            fprintf(stderr, "syntax error: unexpected end of input, 'fi' expected\n");
            ok = 0;
            break;
        }

        // Trim leading and trailing blanks
        char* t = line;
        while (*t == ' ' || *t == '\t') t++;
        size_t len = strlen(t);
        while (len > 0 && (t[len - 1] == ' ' || t[len - 1] == '\t')) {
            t[--len] = '\0';
        }

        if (strcmp(t, "fi") == 0) {
            if (ok && state == IN_COND) {
                fprintf(stderr, "syntax error: 'then' expected before 'fi'\n");
                ok = 0;
            }
            saw_fi = 1;
        } else if (!ok || t[0] == '\0') {
            // after an error, skip lines until "fi"; blank lines are ignored
        } else if (strcmp(t, "then") == 0) {
            if (state != IN_COND || ncond == 0) {
                fprintf(stderr, "syntax error: unexpected 'then'\n");
                ok = 0;
            } else {
                state = IN_THEN;
            }
        } else if (strcmp(t, "else") == 0) {
            if (state != IN_THEN) {
                fprintf(stderr, "syntax error: unexpected 'else'\n");
                ok = 0;
            } else {
                state = IN_ELSE;
            }
        } else if (strcmp(t, "if") == 0 || strncmp(t, "if ", 3) == 0) {
            fprintf(stderr, "syntax error: nested 'if' is not supported\n");
            ok = 0;
        } else {
            // An ordinary command line: store it in the current block
            char** block = (state == IN_COND) ? cond : (state == IN_THEN) ? thenb : elseb;
            int* count = (state == IN_COND) ? &ncond : (state == IN_THEN) ? &nthen : &nelse;
            if (*count == MAX_BLOCK) {
                fprintf(stderr, "syntax error: block too long\n");
                ok = 0;
            } else {
                block[(*count)++] = strdup(t);
            }
        }
        free(line);
    }

    if (ok) {
        // Run the condition; the exit status of its last command decides
        for (int i = 0; i < ncond; i++) {
            run_command_line(cond[i]);
        }
        int success = (last_status == 0);   // 0 means success

        char** block = success ? thenb : elseb;
        int count = success ? nthen : nelse;
        for (int i = 0; i < count; i++) {
            run_command_line(block[i]);
        }
    }

    for (int i = 0; i < ncond; i++) free(cond[i]);
    for (int i = 0; i < nthen; i++) free(thenb[i]);
    for (int i = 0; i < nelse; i++) free(elseb[i]);
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

            if (strcmp(arglist[0], "if") == 0) {
                handle_if(cmdline);       // multi-line if-then-else-fi
            } else {
                run_line(arglist);
            }

            free_arglist(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
