#include "chunk.h"
#include <unistd.h>
#include <assert.h>

static Chunk_T free_head = NULL;  // 자유 리스트의 헤드 포인터
static void *heap_start = NULL;
static void *heap_end = NULL;

// 힙 초기화
static void init_heap() {
    if (heap_start == NULL) {
        heap_start = sbrk(0);
        heap_end = heap_start;
        free_head = NULL;
    }
}

// 크기를 단위로 변환
static size_t size_to_units(size_t size) {
    return (size + sizeof(Chunk_T) - 1) / sizeof(Chunk_T) + 1;
}

// 메모리 할당 요청 시 추가 메모리 할당 함수
static Chunk_T allocate_more_memory(size_t units) {
    Chunk_T c = (Chunk_T)sbrk(units * sizeof(struct Chunk));
    if (c == (Chunk_T)-1) return NULL;

    chunk_set_units(c, units);
    chunk_set_status(c, CHUNK_FREE);
    heap_end = sbrk(0);
    return c;
}

// 자유 리스트에서 청크 제거
static void remove_from_list(Chunk_T chunk) {
    if (chunk == free_head) {
        free_head = chunk_get_next_free_chunk(chunk);
        if (free_head) {
            chunk_set_prev_free_chunk(free_head, NULL);
        }
    } else {
        Chunk_T prev = chunk_get_prev_free_chunk(chunk);
        Chunk_T next = chunk_get_next_free_chunk(chunk);

        if (prev) {
            chunk_set_next_free_chunk(prev, next);
        }
        if (next) {
            chunk_set_prev_free_chunk(next, prev);
        }
    }
    chunk_set_next_free_chunk(chunk, NULL);
    chunk_set_prev_free_chunk(chunk, NULL);
}

// 조건부 인접 청크 병합 (Lazy Coalescing)
static Chunk_T coalesce_if_needed(Chunk_T chunk) {
    Chunk_T next_chunk = chunk_get_next_adjacent(chunk, heap_start, heap_end);
    Chunk_T prev_chunk = chunk_get_prev_adjacent(chunk, heap_start, heap_end);

    // 조건에 따라 병합: 청크의 크기가 일정 크기 이상일 때만 병합 수행
    if (next_chunk && chunk_get_status(next_chunk) == CHUNK_FREE && chunk_get_units(chunk) > 4) {
        remove_from_list(next_chunk);
        chunk_set_units(chunk, chunk_get_units(chunk) + chunk_get_units(next_chunk));
    }

    if (prev_chunk && chunk_get_status(prev_chunk) == CHUNK_FREE && chunk_get_units(prev_chunk) > 4) {
        remove_from_list(prev_chunk);
        chunk_set_units(prev_chunk, chunk_get_units(prev_chunk) + chunk_get_units(chunk));
        chunk = prev_chunk;  // 병합된 청크 반환
    }

    return chunk;
}

// 메모리 할당 함수
void *heapmgr_malloc(size_t size) {
    if (size <= 0) return NULL;

    if (heap_start == NULL) {
        init_heap();
    }

    size_t units = size_to_units(size);

    // 자유 리스트에서 적합한 청크 찾기 (First Fit)
    Chunk_T chunk = free_head;
    while (chunk != NULL) {
        if (chunk_get_units(chunk) >= units) {
            remove_from_list(chunk);

            // 청크가 요청 크기보다 큰 경우 분할
            if (chunk_get_units(chunk) > units) {
                size_t remaining_units = chunk_get_units(chunk) - units;
                chunk_set_units(chunk, units);

                // 분할된 나머지 청크를 자유 리스트에 추가
                Chunk_T new_chunk = (Chunk_T)((char *)chunk + units * sizeof(struct Chunk));
                chunk_set_units(new_chunk, remaining_units);
                chunk_set_status(new_chunk, CHUNK_FREE);
                chunk_set_next_free_chunk(new_chunk, free_head);
                chunk_set_prev_free_chunk(new_chunk, NULL);
                if (free_head != NULL) {
                    chunk_set_prev_free_chunk(free_head, new_chunk);
                }
                free_head = new_chunk;
            }
            chunk_set_status(chunk, CHUNK_IN_USE);
            return (void *)((char *)chunk + sizeof(struct Chunk));
        }
        chunk = chunk_get_next_free_chunk(chunk);
    }

    // 적합한 청크가 없으면 새로 할당
    chunk = allocate_more_memory(units);
    if (chunk == NULL) return NULL;

    chunk_set_status(chunk, CHUNK_IN_USE);
    return (void *)((char *)chunk + sizeof(struct Chunk));
}

// 메모리 해제 함수
void heapmgr_free(void *ptr) {
    if (ptr == NULL) return;

    Chunk_T chunk = (Chunk_T)((char *)ptr - sizeof(struct Chunk));
    chunk_set_status(chunk, CHUNK_FREE);

    // 조건부 병합 수행 (Lazy Coalescing)
    chunk = coalesce_if_needed(chunk);

    // 자유 리스트의 맨 앞에 청크 추가
    chunk_set_next_free_chunk(chunk, free_head);
    chunk_set_prev_free_chunk(chunk, NULL);
    if (free_head != NULL) {
        chunk_set_prev_free_chunk(free_head, chunk);
    }
    free_head = chunk;
}
