/*---------------------------------------------------------------------------*/
/* snush.c                                                                     */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#include "util.h"
#include "token.h"
#include "dynarray.h"
#include "execute.h"
#include "lexsyn.h"
#include "snush.h"

volatile int bg_array_idx;
int *bg_array;
int bg_cnt;

/*---------------------------------------------------------------------------*/
void cleanup() {
    free(bg_array);
}
/*---------------------------------------------------------------------------*/
/* Whenever a child process terminates, this handler handles all zombies. */
static void sigzombie_handler(int signo) {
    pid_t pid;
    int status;

    if (signo == SIGCHLD) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("Background process [%d] terminated.\n", pid);

            // 백그라운드 배열에서 프로세스 제거
            for (int i = 0; i < bg_array_idx; i++) {
                if (bg_array[i] == pid) {
                    bg_array[i] = bg_array[--bg_array_idx]; // 압축 처리
                    break;
                }
            }

            bg_cnt--; // 백그라운드 프로세스 카운터 감소
        }

        if (pid < 0 && errno != ECHILD && errno != EINTR) {
            perror("waitpid");
        }
    }
}
/*---------------------------------------------------------------------------*/
static void shell_helper(const char *in_line) {
    DynArray_T oTokens;

    enum LexResult lexcheck;
    enum SyntaxResult syncheck;
    enum BuiltinType btype;
    int pcount;
    int ret_pgid; // background pid
    int is_background;

    oTokens = dynarray_new(0);
    if (oTokens == NULL) {
        error_print("Cannot allocate memory", FPRINTF);
        exit(EXIT_FAILURE);
    }

    lexcheck = lex_line(in_line, oTokens);
    switch (lexcheck) {
    case LEX_SUCCESS:
        if (dynarray_get_length(oTokens) == 0)
            return;

        /* dump lex result when DEBUG is set */
        dump_lex(oTokens);

        syncheck = syntax_check(oTokens);
        if (syncheck == SYN_SUCCESS) {
            btype = check_builtin(dynarray_get(oTokens, 0));
            if (btype == NORMAL) {
                is_background = check_bg(oTokens);

                if (bg_array_idx == MAX_BG_PRO) {
                    error_print("Program exceeds the maximum number of the " \
                                "background processes\n", FPRINTF);
                    exit(EXIT_FAILURE);
                }

                pcount = count_pipe(oTokens);

                if (pcount > 0) {
                    ret_pgid = iter_pipe_fork_exec(pcount, oTokens, 
                                                is_background);
                }
                else {
                    ret_pgid = fork_exec(oTokens, is_background);
                }

                if (ret_pgid > 0) {
                    if (is_background == 1)
                        printf("[%d] Background process running\n", ret_pgid);
                }
                else {
                    printf("Invalid return value "\
                        "of external command execution\n");
                }
            }
            else {
                /* Execute builtin command */
                execute_builtin(oTokens, btype);
            }
        }

        /* syntax error cases */
        else if (syncheck == SYN_FAIL_NOCMD)
            error_print("Missing command name", FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDOUT)
            error_print("Multiple redirection of standard out", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTOUT)
            error_print("Standard output redirection without file name", 
                        FPRINTF);
        else if (syncheck == SYN_FAIL_MULTREDIN)
            error_print("Multiple redirection of standard input", FPRINTF);
        else if (syncheck == SYN_FAIL_NODESTIN)
            error_print("Standard input redirection without file name", 
                        FPRINTF);
        else if (syncheck == SYN_FAIL_INVALIDBG)
            error_print("Invalid use of background", FPRINTF);
        break;

    case LEX_QERROR:
        error_print("Unmatched quote", FPRINTF);
        break;

    case LEX_NOMEM:
        error_print("Cannot allocate memory", FPRINTF);
        break;

    case LEX_LONG:
        error_print("Command is too large", FPRINTF);
        break;

    default:
        error_print("lex_line needs to be fixed", FPRINTF);
        exit(EXIT_FAILURE);
    }

    /* Free memories allocated to tokens */
    dynarray_map(oTokens, free_token, NULL);
    dynarray_free(oTokens);
}
/*---------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
    sigset_t sigset;
    char c_line[MAX_LINE_SIZE + 2];

    /* Initialize Background process array and stack */
    bg_array = calloc(MAX_BG_PRO, sizeof(int));

    atexit(cleanup);

    /* Initialize variables for background/foreground processes */
    bg_array_idx = 0;
    bg_cnt = 0;


    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGCHLD);
    sigaddset(&sigset, SIGQUIT);
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
 
    /* Register signal handler */
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigzombie_handler);
    signal(SIGQUIT, SIG_IGN);

    error_print(argv[0], SETUP);

    while (1) {
        fprintf(stdout, "%% ");
        fflush(stdout);
        if (fgets(c_line, MAX_LINE_SIZE, stdin) == NULL) {
            printf("\n");
            exit(EXIT_SUCCESS);
        }
        shell_helper(c_line);
    }

    return 0;
}
/*---------------------------------------------------------------------------*/