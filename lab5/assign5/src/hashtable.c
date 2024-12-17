/*---------------------------------------------------------------------------*/
/* hashtable.c                                                               */
/* Author: Junghan Yoon, KyoungSoo Park                                      */
/* Modified by: (Your Name)                                                  */
/*---------------------------------------------------------------------------*/
#include "hashtable.h"
/*---------------------------------------------------------------------------*/
int hash(const char *key, size_t hash_size)
{
    TRACE_PRINT();
    unsigned int hash = 0;
    while (*key)
    {
        hash = (hash << 5) + *key++;
    }

    return hash % hash_size;
}
/*---------------------------------------------------------------------------*/
hashtable_t *hash_init(size_t hash_size, int delay)
{
    TRACE_PRINT();
    int i, j, ret;
    hashtable_t *table = malloc(sizeof(hashtable_t));

    if (table == NULL)
    {
        DEBUG_PRINT("Failed to allocate memory for hash table");
        return NULL;
    }

    table->hash_size = hash_size;
    table->total_entries = 0;

    table->buckets = malloc(hash_size * sizeof(node_t *));
    if (table->buckets == NULL)
    {
        DEBUG_PRINT("Failed to allocate memory for hash table buckets");
        free(table);
        return NULL;
    }

    table->locks = malloc(hash_size * sizeof(rwlock_t));
    if (table->locks == NULL)
    {
        DEBUG_PRINT("Failed to allocate memory for hash table locks");
        free(table->buckets);
        free(table);
        return NULL;
    }

    table->bucket_sizes = malloc(hash_size * sizeof(*table->bucket_sizes));
    if (table->bucket_sizes == NULL)
    {
        DEBUG_PRINT("Failed to allocate memory for hash table bucket sizes");
        free(table->buckets);
        free(table->locks);
        free(table);
        return NULL;
    }

    for (i = 0; i < hash_size; i++)
    {
        table->buckets[i] = NULL;
        table->bucket_sizes[i] = 0;
        ret = rwlock_init(&table->locks[i], delay);
        if (ret != 0)
        {
            DEBUG_PRINT("Failed to initialize read-write lock");
            for (j = 0; j < i; j++)
            {
                rwlock_destroy(&table->locks[j]);
            }
            free(table->buckets);
            free(table->locks);
            free(table->bucket_sizes);
            free(table);
            return NULL;
        }
    }

    return table;
}
/*---------------------------------------------------------------------------*/
int hash_destroy(hashtable_t *table)
{
    TRACE_PRINT();
    node_t *node, *tmp;
    int i;

    for (i = 0; i < table->hash_size; i++)
    {
        node = table->buckets[i];
        while (node)
        {
            tmp = node;
            node = node->next;
            free(tmp->key);
            free(tmp->value);
            free(tmp);
        }
        if (rwlock_destroy(&table->locks[i]) != 0)
        {
            DEBUG_PRINT("Failed to destroy read-write lock");
            return -1;
        }
    }

    free(table->buckets);
    free(table->locks);
    free(table->bucket_sizes);
    free(table);
    
    return 0;
}
/*---------------------------------------------------------------------------*/
int hash_insert(hashtable_t *table, const char *key, const char *value)
{
    TRACE_PRINT();
    node_t *node;
    unsigned int index = hash(key, table->hash_size);
    rwlock_t *lock = &table->locks[index];

/*---------------------------------------------------------------------------*/
    /* edit here */
    
    /* 쓰기 락 획득 */
    rwlock_write_lock(lock);

    /* 버킷에 같은 키가 있는지 검사 */
    for (node = table->buckets[index]; node; node = node->next)
    {
        if (strcmp(node->key, key) == 0)
        {
            rwlock_write_unlock(lock);
            return 0; // Collision (키가 이미 존재)
        }
    }

    /* 새로운 노드 할당 및 데이터 삽입 */
    node = malloc(sizeof(node_t));
    if (!node)
    {
        rwlock_write_unlock(lock);
        return -1; // 메모리 할당 실패
    }
    node->key = strdup(key);
    node->value = strdup(value);
    node->next = table->buckets[index];
    table->buckets[index] = node;

    table->bucket_sizes[index]++;
    table->total_entries++;

    /* 쓰기 락 해제 */
    rwlock_write_unlock(lock);
    
/*---------------------------------------------------------------------------*/

    /* inserted */
    return 1;
}
/*---------------------------------------------------------------------------*/
int hash_search(hashtable_t *table, const char *key, const char **value)
{
    TRACE_PRINT();
    node_t *node;
    unsigned int index = hash(key, table->hash_size);
    rwlock_t *lock = &table->locks[index];

/*---------------------------------------------------------------------------*/
    /* edit here */
    
    /* 읽기 락 획득 */
    rwlock_read_lock(lock);

    /* 버킷에서 키 검색 */
    for (node = table->buckets[index]; node; node = node->next)
    {
        if (strcmp(node->key, key) == 0)
        {
            *value = node->value;
            rwlock_read_unlock(lock);
            return 1; // 키를 찾음
        }
    }

    /* 읽기 락 해제 */
    rwlock_read_unlock(lock);
    
/*---------------------------------------------------------------------------*/

    /* key not found */
    return 0;
}
/*---------------------------------------------------------------------------*/
int hash_update(hashtable_t *table, const char *key, const char *value)
{
    TRACE_PRINT();
    node_t *node;
    unsigned int index = hash(key, table->hash_size);
    rwlock_t *lock = &table->locks[index];

/*---------------------------------------------------------------------------*/
    /* edit here */
    
    /* 쓰기 락 획득 */
    rwlock_write_lock(lock);

    /* 버킷에서 키 검색 후 값 갱신 */
    for (node = table->buckets[index]; node; node = node->next)
    {
        if (strcmp(node->key, key) == 0)
        {
            free(node->value);           // 기존 값 메모리 해제
            node->value = strdup(value); // 새 값 복사
            rwlock_write_unlock(lock);
            return 1; // 값 갱신 성공
        }
    }

    /* 쓰기 락 해제 */
    rwlock_write_unlock(lock);
    
/*---------------------------------------------------------------------------*/

    /* key not found */
    return 0;
}
/*---------------------------------------------------------------------------*/
int hash_delete(hashtable_t *table, const char *key)
{
    TRACE_PRINT();
    node_t *node, *prev = NULL;
    unsigned int index = hash(key, table->hash_size);
    rwlock_t *lock = &table->locks[index];

/*---------------------------------------------------------------------------*/
    /* edit here */
    
    /* 쓰기 락 획득 */
    rwlock_write_lock(lock);

    /* 버킷에서 노드 검색 및 삭제 */
    for (node = table->buckets[index]; node; node = node->next)
    {
        if (strcmp(node->key, key) == 0)
        {
            if (prev)
            {
                prev->next = node->next; // 이전 노드가 있을 경우 연결
            }
            else
            {
                table->buckets[index] = node->next; // 버킷의 첫 노드 제거
            }

            free(node->key);
            free(node->value);
            free(node);

            table->bucket_sizes[index]--;
            table->total_entries--;

            rwlock_write_unlock(lock);
            return 1; // 삭제 성공
        }
        prev = node;
    }

    /* 쓰기 락 해제 */
    rwlock_write_unlock(lock);
    
/*---------------------------------------------------------------------------*/

    /* key not found */
    return 0;
}
/*---------------------------------------------------------------------------*/
/* function to dump the contents of the hash table, including locks status */
void hash_dump(hashtable_t *table)
{
    TRACE_PRINT();
    node_t *node;
    int i;

    printf("[Hash Table Dump]");
    printf("Total Entries: %ld\n", table->total_entries);

    for (i = 0; i < table->hash_size; i++)
    {
        if (!table->bucket_sizes[i])
        {
            continue;
        }
        printf("Bucket %d: %ld entries\n", i, table->bucket_sizes[i]);
        printf("  Lock State -> Read Count: %d, Write Count: %d\n",
               table->locks[i].read_count, table->locks[i].write_count);
        node = table->buckets[i];
        while (node)
        {
            printf("    Key:   %s\n"
                   "    Value: %s\n", node->key, node->value);
            node = node->next;
        }
    }
    printf("End of Dump\n");
}