/*
 * wish -- the Wisconsin Shell (OSTEP project: processes-shell)
 *
 * Reads a line at a time, from stdin (interactive) or from a batch file,
 * splits it into '&'-separated commands, launches all of them, and only
 * then waits for them to finish.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define WHITESPACE " \t\n\r\v\f"

static char error_message[30] = "An error has occurred\n";

static void report_error(void)
{
    ssize_t ignored = write(STDERR_FILENO, error_message, strlen(error_message));
    (void)ignored;
}

/*
 * The search path. Starts as a single entry, /bin; the "path" built-in
 * replaces it wholesale, and "path" with no arguments empties it.
 */
static char **search_path = NULL;
static int search_path_len = 0;

static void path_set(char **dirs, int count)
{
    for (int i = 0; i < search_path_len; i++)
        free(search_path[i]);
    free(search_path);

    search_path = malloc((count + 1) * sizeof(char *));
    for (int i = 0; i < count; i++)
        search_path[i] = strdup(dirs[i]);
    search_path_len = count;
}

/* Find cmd in the search path. Returns a malloc'd path, or NULL. */
static char *path_resolve(const char *cmd)
{
    for (int i = 0; i < search_path_len; i++) {
        size_t len = strlen(search_path[i]) + strlen(cmd) + 2;
        char *full = malloc(len);
        snprintf(full, len, "%s/%s", search_path[i], cmd);
        if (access(full, X_OK) == 0)
            return full;
        free(full);
    }
    return NULL;
}

/*
 * Split s on whitespace. Returns a malloc'd NULL-terminated array whose
 * entries point into s (which is modified in place); only the array itself
 * needs freeing. Runs of whitespace collapse, so "  a   b " yields {a, b}.
 */
static char **tokenize(char *s, int *count)
{
    int cap = 8, n = 0;
    char **tokens = malloc(cap * sizeof(char *));
    char *tok;

    while ((tok = strsep(&s, WHITESPACE)) != NULL) {
        if (*tok == '\0')
            continue;
        if (n + 2 > cap) {
            cap *= 2;
            tokens = realloc(tokens, cap * sizeof(char *));
        }
        tokens[n++] = tok;
    }
    tokens[n] = NULL;
    *count = n;
    return tokens;
}

/*
 * Run one command: "[cmd [args]] [> file]". Built-ins run here in the
 * parent; anything else is forked off. Returns the number of children
 * started (0 or 1) so the caller knows how many to wait for.
 */
static int run_command(char *segment)
{
    /* Peel off the redirection target. A second '>' is an error. */
    char *cmd_part = strsep(&segment, ">");
    char *redir_part = strsep(&segment, ">");
    if (segment != NULL) {
        report_error();
        return 0;
    }

    char *outfile = NULL;
    if (redir_part != NULL) {
        int nfiles;
        char **files = tokenize(redir_part, &nfiles);
        if (nfiles != 1) {
            free(files);
            report_error();
            return 0;
        }
        outfile = files[0];
        free(files);
    }

    int argc;
    char **argv = tokenize(cmd_part, &argc);
    if (argc == 0) {
        free(argv);
        /* "> file" needs a command; a blank segment is simply skipped. */
        if (redir_part != NULL)
            report_error();
        return 0;
    }

    /* Built-ins. Redirection does not apply to them. */
    if (strcmp(argv[0], "exit") == 0) {
        if (argc != 1)
            report_error();
        else
            exit(0);
        free(argv);
        return 0;
    }
    if (strcmp(argv[0], "cd") == 0) {
        if (argc != 2 || chdir(argv[1]) != 0)
            report_error();
        free(argv);
        return 0;
    }
    if (strcmp(argv[0], "path") == 0) {
        path_set(argv + 1, argc - 1);
        free(argv);
        return 0;
    }

    char *program = path_resolve(argv[0]);
    if (program == NULL) {
        report_error();
        free(argv);
        return 0;
    }

    fflush(NULL); /* don't hand buffered output to the child */
    pid_t pid = fork();
    if (pid < 0) {
        report_error();
        free(program);
        free(argv);
        return 0;
    }
    if (pid == 0) {
        if (outfile != NULL) {
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                report_error();
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execv(program, argv);
        report_error(); /* execv only returns on failure */
        exit(1);
    }

    free(program);
    free(argv);
    return 1;
}

/* Run every '&'-separated command on the line, then reap them all. */
static void run_line(char *line)
{
    char *rest = line;
    char *segment;
    int children = 0;

    while ((segment = strsep(&rest, "&")) != NULL)
        children += run_command(segment);

    while (children-- > 0)
        wait(NULL);
}

int main(int argc, char *argv[])
{
    FILE *input = stdin;
    int interactive = 1;

    if (argc > 2) {
        report_error();
        exit(1);
    }
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            report_error();
            exit(1);
        }
        interactive = 0;
    }

    char *bin = "/bin";
    path_set(&bin, 1);

    char *line = NULL;
    size_t cap = 0;

    while (1) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }
        if (getline(&line, &cap, input) == -1)
            break;

        run_line(line);
    }

    free(line);
    exit(0);
}
