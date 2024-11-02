#ifndef CHUNKBASE_H
#define CHUNKBASE_H

#include <stddef.h>

// 청크 상태 상수
#define CHUNK_FREE 0
#define CHUNK_IN_USE 1

// 청크 구조체 정의
typedef struct Chunk *Chunk_T;
struct Chunk {
    int units;                // 청크 크기 (헤더 부분)
    int status;               // CHUNK_FREE 또는 CHUNK_IN_USE 상태 (헤더 부분)
    struct Chunk *next;       // 자유 리스트의 다음 청크 포인터
    struct Chunk *prev;       // 자유 리스트의 이전 청크 포인터
    int footer_units;         // 청크 크기 (푸터 부분) - adjacent navigation 시 사용
};

// 함수 프로토타입
int chunk_get_status(Chunk_T chunk);
void chunk_set_status(Chunk_T chunk, int status);
int chunk_get_units(Chunk_T chunk);
void chunk_set_units(Chunk_T chunk, int units);
Chunk_T chunk_get_next_free_chunk(Chunk_T chunk);
void chunk_set_next_free_chunk(Chunk_T chunk, Chunk_T next);
Chunk_T chunk_get_prev_free_chunk(Chunk_T chunk);
void chunk_set_prev_free_chunk(Chunk_T chunk, Chunk_T prev);
Chunk_T chunk_get_next_adjacent(Chunk_T chunk, void *start, void *end);
Chunk_T chunk_get_prev_adjacent(Chunk_T chunk, void *start, void *end);

#endif // CHUNKBASE_H
