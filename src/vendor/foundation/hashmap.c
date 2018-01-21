/* hashmap.h  -  Foundation library  -  Public Domain  -  2013 Mattias Jansson /
 * Rampant Pixels
 *
 * This library provides a cross-platform foundation library in C11 providing
 * basic support data types and functions to write applications and games in a
 * platform-independent fashion. The latest source code is always available at
 *
 * https://github.com/rampantpixels/foundation_lib
 *
 * This library is put in the public domain; you can redistribute it and/or
 * modify it without any restrictions.
 */

#include <foundation/foundation.h>

#define HASHMAP_MINBUCKETS 13
#define HASHMAP_MINBUCKETSIZE 8

#define GET_BUCKET(map, key) (key % map->num_buckets)

hashmap_t *hashmap_allocate(size_t buckets, size_t bucketsize) {
    hashmap_t *map;

    if (buckets < HASHMAP_MINBUCKETS)
        buckets = HASHMAP_MINBUCKETS;
    if (bucketsize < HASHMAP_MINBUCKETSIZE)
        bucketsize = HASHMAP_MINBUCKETSIZE;

    map = memory_allocate(
              0, sizeof(hashmap_t) + sizeof(hashmap_node_t *) * buckets, 0,
              MEMORY_PERSISTENT);

    hashmap_initialize(map, buckets, bucketsize);

    return map;
}

void hashmap_initialize(hashmap_t *map, size_t buckets, size_t bucketsize) {
    size_t ibucket;

    if (bucketsize < HASHMAP_MINBUCKETSIZE)
        bucketsize = HASHMAP_MINBUCKETSIZE;

    map->num_buckets = buckets;
    map->num_nodes = 0;

    for (ibucket = 0; ibucket < buckets; ++ibucket) {
        map->bucket[ibucket] = 0;
        array_reserve(map->bucket[ibucket], bucketsize);
    }
}

void hashmap_deallocate(hashmap_t *map) {
    hashmap_finalize(map);
    memory_deallocate(map);
}

void hashmap_finalize(hashmap_t *map) {
    size_t ibucket;
    for (ibucket = 0; ibucket < map->num_buckets; ++ibucket)
        array_deallocate(map->bucket[ibucket]);
}

void *hashmap_insert(hashmap_t *map, hash_t key, void *value) {
    /*lint --e{613} */
    size_t ibucket = GET_BUCKET(map, key);
    hashmap_node_t *bucket = map->bucket[ibucket];
    size_t inode, nsize;
    for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
        if (bucket[inode].key == key) {
            void *prev = bucket[inode].value;
            bucket[inode].value = value;
            return prev;
        }
    }
    {
        hashmap_node_t node = {key, value};
        array_push(map->bucket[ibucket], node);
        ++map->num_nodes;
    }
    return 0;
}

void *hashmap_erase(hashmap_t *map, hash_t key) {
    /*lint --e{613} */
    size_t ibucket = GET_BUCKET(map, key);
    hashmap_node_t *bucket = map->bucket[ibucket];
    size_t inode, nsize;
    for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
        if (bucket[inode].key == key) {
            void *prev = bucket[inode].value;
            array_erase(map->bucket[ibucket], inode);
            --map->num_nodes;
            return prev;
        }
    }
    return 0;
}

void *hashmap_lookup(hashmap_t *map, hash_t key) {
    /*lint --e{613} */
    size_t ibucket = GET_BUCKET(map, key);
    hashmap_node_t *bucket = map->bucket[ibucket];
    size_t inode, nsize;
    for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
        if (bucket[inode].key == key)
            return bucket[inode].value;
    }
    return 0;
}

bool hashmap_has_key(hashmap_t *map, hash_t key) {
    /*lint --e{613} */
    size_t ibucket = GET_BUCKET(map, key);
    hashmap_node_t *bucket = map->bucket[ibucket];
    size_t inode, nsize;
    for (inode = 0, nsize = array_size(bucket); inode < nsize; ++inode) {
        if (bucket[inode].key == key)
            return true;
    }
    return false;
}

size_t hashmap_size(hashmap_t *map) {
    return map->num_nodes;
}

void hashmap_clear(hashmap_t *map) {
    size_t ibucket;
    for (ibucket = 0; ibucket < map->num_buckets; ++ibucket)
        array_clear(map->bucket[ibucket]);
    map->num_nodes = 0;
}
