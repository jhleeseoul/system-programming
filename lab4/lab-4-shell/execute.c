/*---------------------------------------------------------------------------*/
/* execute.c                                                                 */
/* Author: Jongki Park, Kyoungsoo Park                                       */
/*---------------------------------------------------------------------------*/

#include "dynarray.h"
#include "token.h"
#include "util.h"
#include "lexsyn.h"
#include "snush.h"
#include "execute.h"

extern volatile int bg_array_idx;
extern int *bg_array;
extern int bg_cnt;

/*---------------------------------------------------------------------------*/
void redout_handler(char *fname) {
    int fd;

    fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        error_print(NULL, PERROR);
        exit(EXIT_FAILURE);
    } else {
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

/*---------------------------------------------------------------------------*/
void redin_handler(char *fname) {
	int fd;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else {
		dup2(fd, STDIN_FILENO);
		close(fd);
	}
}
/*---------------------------------------------------------------------------*/
int build_command_partial(DynArray_T oTokens, int start, 
						int end, char *args[]) {
	int i, ret = 0, redin = FALSE, redout = FALSE, cnt = 0;
	struct Token *t;

	/* Build command */
	for (i = start; i < end; i++) {

		t = dynarray_get(oTokens, i);

		if (t->token_type == TOKEN_WORD) {
			if (redin == TRUE) {
				redin_handler(t->token_value);
				redin = FALSE;
			}
			else if (redout == TRUE) {
				redout_handler(t->token_value);
				redout = FALSE;
			}
			else
				args[cnt++] = t->token_value;
		}
		else if (t->token_type == TOKEN_REDIN)
			redin = TRUE;
		else if (t->token_type == TOKEN_REDOUT)
			redout = TRUE;
	}
	args[cnt] = NULL;

#ifdef DEBUG
	for (i = 0; i < cnt; i++)
	{
		if (args[i] == NULL)
			printf("CMD: NULL\n");
		else
			printf("CMD: %s\n", args[i]);
	}
	printf("END\n");
#endif
	return ret;
}
/*---------------------------------------------------------------------------*/
int build_command(DynArray_T oTokens, char *args[]) {
	return build_command_partial(oTokens, 0, 
								dynarray_get_length(oTokens), args);
}
/*---------------------------------------------------------------------------*/
void execute_builtin(DynArray_T oTokens, enum BuiltinType btype) {
	int ret;
	char *dir = NULL;
	struct Token *t1;

	switch (btype) {
	case B_EXIT:
		if (dynarray_get_length(oTokens) == 1) {
			// printf("\n");
			dynarray_map(oTokens, free_token, NULL);
			dynarray_free(oTokens);

			exit(EXIT_SUCCESS);
		}
		else
			error_print("exit does not take any parameters", FPRINTF);

		break;

	case B_CD:
		if (dynarray_get_length(oTokens) == 1) {
			dir = getenv("HOME");
			if (dir == NULL) {
				error_print("cd: HOME variable not set", FPRINTF);
				break;
			}
		}
		else if (dynarray_get_length(oTokens) == 2) {
			t1 = dynarray_get(oTokens, 1);
			if (t1->token_type == TOKEN_WORD)
				dir = t1->token_value;
		}

		if (dir == NULL) {
			error_print("cd takes one parameter", FPRINTF);
			break;
		}
		else {
			ret = chdir(dir);
			if (ret < 0)
				error_print(NULL, PERROR);
		}
		break;

	default:
		error_print("Bug found in execute_builtin", FPRINTF);
		exit(EXIT_FAILURE);
	}
}
/*---------------------------------------------------------------------------*/
/* Important Notice!! 
	Add "signal(SIGINT, SIG_DFL);" after fork
*/
int fork_exec(DynArray_T oTokens, int is_background) {
    pid_t pid = fork();

    if (pid == 0) { // Child process
        signal(SIGINT, SIG_DFL); // Ctrl-C 처리
        char *args[MAX_ARGS_CNT];
        build_command(oTokens, args);

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) { // Parent process
        if (!is_background) {
            waitpid(pid, NULL, 0);
        } else {
            bg_array[bg_array_idx++] = pid;
            bg_cnt++;
        }
    } else {
        perror("fork");
        return -1;
    }
    return pid;
}

/*---------------------------------------------------------------------------*/
/* Important Notice!! 
	Add "signal(SIGINT, SIG_DFL);" after fork
*/
int iter_pipe_fork_exec(int pcount, DynArray_T oTokens, int is_background) {
    int pipe_fd[2], prev_fd = -1; // 현재 및 이전 파이프 파일 디스크립터
    int i, cmd_start = 0, cmd_end = 0;
    pid_t pid;
    char *args[MAX_ARGS_CNT];
    int ret_status = 0; // 최종 반환 상태

    for (i = 0; i <= pcount; i++) {
        // 파이프 생성
        if (i < pcount) {
            if (pipe(pipe_fd) < 0) {
                perror("pipe");
                return -1;
            }
        }

        // 명령어 끝 구간 찾기
        cmd_end = cmd_start;
        while (cmd_end < dynarray_get_length(oTokens)) {
            struct Token *t = dynarray_get(oTokens, cmd_end);
            if (t->token_type == TOKEN_PIPE) break;
            cmd_end++;
        }

        // 프로세스 생성
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) { // 자식 프로세스
			signal(SIGINT, SIG_DFL); // 기본 동작으로 복원
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO); // 이전 출력 -> 현재 입력
                close(prev_fd);
            }
            if (i < pcount) {
                dup2(pipe_fd[1], STDOUT_FILENO); // 현재 출력 -> 다음 입력
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }

            // 명령어 빌드 및 실행
            build_command_partial(oTokens, cmd_start, cmd_end, args);
#ifdef DEBUG
            fprintf(stderr, "Executing command in child: %s\n", args[0]);
#endif
            if (execvp(args[0], args) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        } else { // 부모 프로세스
            if (prev_fd != -1) close(prev_fd); // 이전 읽기 파이프 닫기
            if (i < pcount) {
                close(pipe_fd[1]); // 현재 쓰기 파이프 닫기
                prev_fd = pipe_fd[0]; // 다음 읽기 파이프 설정
            }

            if (!is_background) {
                int status;
                waitpid(pid, &status, 0); // 포그라운드 대기
                if (WIFEXITED(status)) {
                    ret_status = WEXITSTATUS(status);
                } else {
					// printf("Child process %d terminated abnormally\n", pid);
                    ret_status = -1; // 비정상 종료
                }
            } else {
                bg_array[bg_array_idx++] = pid; // 백그라운드 등록
                bg_cnt++;
            }
        }

        cmd_start = cmd_end + 1; // 다음 명령어 시작 인덱스
    }

    if (prev_fd != -1) close(prev_fd); // 마지막 읽기 파이프 닫기
	return ret_status == 0 ? 1 : -1; // 성공 시 1 반환

}



/*---------------------------------------------------------------------------*/