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

    //
    // TODO redout_handler() in execute.c start
    //

    fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		error_print(NULL, PERROR);
		exit(EXIT_FAILURE);
	}
	else {
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
#ifdef DEBUG // how to execute this code : $ DEBUG=1 ./snush
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
    pid_t pid;
    int status;
    char *args[128];

    // 명령어 빌드
    build_command(oTokens, args);

    // 자식 프로세스 생성
    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return -1;
    } else if (pid == 0) {
        // 자식 프로세스

        // 리다이렉션 처리
        for (int i = 0; i < dynarray_get_length(oTokens); i++) {
            char *token = dynarray_get(oTokens, i);
            if (strcmp(token, ">") == 0) {
                // 출력 리다이렉션
                redout_handler(dynarray_get(oTokens, i + 1));
                break;
            }
        }

        signal(SIGINT, SIG_DFL); // SIGINT 기본 동작 복원

        // 명령어 실행
        if (execvp(args[0], args) < 0) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // 부모 프로세스
        if (!is_background) {
            // 포그라운드 작업
            waitpid(pid, &status, 0);
        } else {
            // 백그라운드 작업
            bg_array[bg_array_idx++] = pid;
            bg_cnt++;
            printf("[Background process started: %d]\n", pid);
        }
    }

    return pid;
}



/*---------------------------------------------------------------------------*/
/* Important Notice!! 
	Add "signal(SIGINT, SIG_DFL);" after fork
*/
int iter_pipe_fork_exec(int pcount, DynArray_T oTokens, int is_background) {
    int pipe_fd[2];         // 현재 파이프의 파일 디스크립터
    int prev_fd = -1;       // 이전 파이프의 읽기 끝
    pid_t pid;              // 자식 프로세스 ID
    int i, status;          // 반복문 및 상태 변수
    char *args[128];        // 명령어와 인자를 담는 배열
    int first_pid = -1;     // 첫 번째 자식의 PID를 저장
    int start = 0, end;     // 토큰의 범위

    for (i = 0; i <= pcount; i++) {
        // 다음 파이프 기호의 위치를 찾음
        end = dynarray_get_length(oTokens);
        for (int j = start; j < dynarray_get_length(oTokens); j++) {
            if (strcmp((char *)dynarray_get(oTokens, j), "|") == 0) {
                end = j;  // 파이프 기호의 인덱스를 end로 설정
                break;
            }
        }

        // 파이프 생성 (마지막 명령어가 아닐 때만)
        if (i < pcount) {
            if (pipe(pipe_fd) < 0) {
                error_print("pipe failed", FPRINTF);
                return -1;
            }
        }

        // 자식 프로세스 생성
        pid = fork();
        if (pid < 0) {
            error_print("fork failed", FPRINTF);
            return -1;
        } else if (pid == 0) {
            // 자식 프로세스
            signal(SIGINT, SIG_DFL);  // 기본 SIGINT 핸들러 복원

            // 이전 파이프의 읽기 끝을 표준 입력으로 연결
            if (prev_fd != -1) {
                if (dup2(prev_fd, STDIN_FILENO) < 0) {
                    error_print("dup2 failed (input)", FPRINTF);
                    exit(EXIT_FAILURE);
                }
                close(prev_fd);
            }

            // 현재 파이프의 쓰기 끝을 표준 출력으로 연결
            if (i < pcount) {
                if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
                    error_print("dup2 failed (output)", FPRINTF);
                    exit(EXIT_FAILURE);
                }
                close(pipe_fd[0]);
                close(pipe_fd[1]);
            }

            // 명령어 빌드 및 실행
            if (build_command_partial(oTokens, start, end, args) < 0) {
                error_print("build_command_partial failed", FPRINTF);
                exit(EXIT_FAILURE);
            }
            if (execvp(args[0], args) < 0) {
                error_print(args[0], PERROR);
                exit(EXIT_FAILURE);
            }
        } else {
            // 부모 프로세스
            if (i == 0) first_pid = pid;
            if (prev_fd != -1) close(prev_fd);
            if (i < pcount) close(pipe_fd[1]);

            prev_fd = pipe_fd[0];
        }

        // 다음 명령어의 시작 위치 설정
        start = end + 1;
    }

    // 모든 자식 프로세스 대기
    for (i = 0; i <= pcount; i++) {
        waitpid(-1, &status, 0);
    }

    return first_pid;  // 첫 번째 자식 프로세스의 PID 반환
}





/*---------------------------------------------------------------------------*/