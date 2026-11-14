#include "shell.h"
#include <fcntl.h>

#define MAX_CMDS (MAXARGS + 1)

// One command in a pipeline: its arguments and optional redirections
typedef struct {
    char* argv[MAXARGS + 1];
    char* infile;    // file after '<', or NULL
    char* outfile;   // file after '>', or NULL
} command_t;

static int is_operator(const char* t) {
    return strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, "|") == 0;
}

// Split the token list into commands at '|' and pull out < and > files.
// Returns the number of commands, or -1 on a syntax error.
static int parse_commands(char** arglist, command_t* cmds) {
    int n = 0, argc = 0;
    memset(&cmds[0], 0, sizeof(command_t));

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (argc == 0) {
                fprintf(stderr, "syntax error: missing command before '|'\n");
                return -1;
            }
            cmds[n].argv[argc] = NULL;
            n++;
            memset(&cmds[n], 0, sizeof(command_t));
            argc = 0;
        } else if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
            int is_in = (strcmp(arglist[i], "<") == 0);
            if (arglist[i + 1] == NULL || is_operator(arglist[i + 1])) {
                fprintf(stderr, "syntax error: missing filename after '%s'\n", arglist[i]);
                return -1;
            }
            if (is_in) cmds[n].infile = arglist[i + 1];
            else       cmds[n].outfile = arglist[i + 1];
            i++;   // skip the filename
        } else {
            cmds[n].argv[argc++] = arglist[i];
        }
    }

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    cmds[n].argv[argc] = NULL;
    return n + 1;
}

int execute(char** arglist, int background) {
    command_t cmds[MAX_CMDS];
    pid_t pids[MAX_CMDS];
    int ncmds = parse_commands(arglist, cmds);
    if (ncmds < 0) {
        return -1;
    }

    int prev_read = -1;   // read end of the previous pipe (stdin of this command)
    int started = 0;

    for (int i = 0; i < ncmds; i++) {
        int fd[2] = {-1, -1};

        // A pipe is needed between this command and the next one
        if (i < ncmds - 1 && pipe(fd) < 0) {
            perror("pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            if (fd[0] != -1) { close(fd[0]); close(fd[1]); }
            break;
        }

        if (pid == 0) {   // ---- child ----
            if (prev_read != -1) {          // read from the previous command
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (fd[1] != -1) {              // write into the next command
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
                close(fd[0]);
            }
            if (cmds[i].infile != NULL) {   // '<' overrides the pipe input
                int f = open(cmds[i].infile, O_RDONLY);
                if (f < 0) { perror(cmds[i].infile); exit(1); }
                dup2(f, STDIN_FILENO);
                close(f);
            }
            if (cmds[i].outfile != NULL) {  // '>' overrides the pipe output
                int f = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (f < 0) { perror(cmds[i].outfile); exit(1); }
                dup2(f, STDOUT_FILENO);
                close(f);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found");    // runs only if execvp fails
            exit(1);
        }

        // ---- parent ----
        pids[started++] = pid;
        if (prev_read != -1) close(prev_read);   // parent must close unused ends,
        if (fd[1] != -1) close(fd[1]);           // or the reader never sees EOF
        prev_read = fd[0];                       // -1 after the last command
    }

    if (prev_read != -1) close(prev_read);

    if (background && started > 0) {
        // Do not wait: remember the last process of the pipeline as the job
        char text[128] = "";
        for (int i = 0; arglist[i] != NULL; i++) {
            if (i > 0) strncat(text, " ", sizeof(text) - strlen(text) - 1);
            strncat(text, arglist[i], sizeof(text) - strlen(text) - 1);
        }
        add_job(pids[started - 1], text);
        return 0;
    }

    int status;
    for (int i = 0; i < started; i++) {
        waitpid(pids[i], &status, 0);
    }
    return 0;
}
