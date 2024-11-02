#include "chunk.h"

// 청크의 상태 반환
int chunk_get_status(Chunk_T chunk) {
    return chunk->status;
}

// 청크의 상태 설정
void chunk_set_status(Chunk_T chunk, int status) {
    chunk->status = status;
}

// 청크의 크기 반환
int chunk_get_units(Chunk_T chunk) {
    return chunk->units;
}

// 청크의 크기 설정 (푸터에 크기 정보 복사)
void chunk_set_units(Chunk_T chunk, int units) {
    chunk->units = units;
    chunk->footer_units = units;  // 푸터에 크기 정보 설정
}

// 자유 리스트에서 다음 자유 청크 반환
Chunk_T chunk_get_next_free_chunk(Chunk_T chunk) {
    return chunk->next;
}

// 자유 리스트에서 다음 자유 청크 설정
void chunk_set_next_free_chunk(Chunk_T chunk, Chunk_T next) {
    chunk->next = next;
}

// 자유 리스트에서 이전 자유 청크 반환
Chunk_T chunk_get_prev_free_chunk(Chunk_T chunk) {
    return chunk->prev;
}

// 자유 리스트에서 이전 자유 청크 설정
void chunk_set_prev_free_chunk(Chunk_T chunk, Chunk_T prev) {
    chunk->prev = prev;
}

// 메모리 공간에서 다음 인접 청크 반환 (units 필드를 사용하여 다음 청크 위치 계산)
Chunk_T chunk_get_next_adjacent(Chunk_T chunk, void *start, void *end) {
    Chunk_T next_chunk = (Chunk_T)((char *)chunk + chunk->units * sizeof(struct Chunk));
    if ((void *)next_chunk >= start && (void *)next_chunk < end) {
        return next_chunk;
    }
    return NULL;  // 범위를 벗어난 경우 NULL 반환
}

// 메모리 공간에서 이전 인접 청크 반환 (footer_units 필드를 사용하여 이전 청크 위치 계산)
Chunk_T chunk_get_prev_adjacent(Chunk_T chunk, void *start, void *end) {
    Chunk_T prev_chunk = (Chunk_T)((char *)chunk - chunk->footer_units * sizeof(struct Chunk));
    if ((void *)prev_chunk >= start && (void *)prev_chunk < end) {
        return prev_chunk;
    }
    return NULL;  // 범위를 벗어난 경우 NULL 반환
}
