#include "cache.h"

static cache_t *cache_head;
static size_t cache_size;
static sem_t mutex, w;
static int readcnt;

/*
 * Initialize cache
 */
void cache_init()
{
	cache_head = NULL;
	cache_size = 0;
	readcnt = 0;
	Sem_init(&mutex, 0, 1);
	Sem_init(&w, 0, 1);
	cache_mark_clear();
}

/*
 * Store cache information in a pointer
 * Add the cache pointer to cache list
 */
void cache_store(size_t filesize, char *uri, unsigned char *response)
{
	P(&w);
	cache_t *ptr = (cache_t *)Calloc(1, sizeof(*ptr));
	ptr->size = filesize;
	strcpy(ptr->uri, uri);
	ptr->content = (unsigned char*)Calloc(1, filesize);
	size_t i;
	for (i = 0; i < filesize; i++) {
		ptr->content[i] = response[i];
	}
	cache_add(ptr);
	V(&w);
}

/*
 * Add cache into a linked list
 */
void cache_add(cache_t *ptr)
{
	/* Add item into cache if it can fit */
	if (cache_size + ptr->size <= MAX_CACHE_SIZE) {
		if (cache_head != NULL) {
			ptr->next = cache_head;
		}
		cache_head = ptr;
		cache_size += ptr->size;
	}
	else {
		/* Delete the last cache item */
		cache_delete();
		/* Continue to add this item */
		cache_add(ptr);
	}
}

/*
 * Delete a cache item in the list
 */
void cache_delete()
{
	cache_t *ptr = cache_head;
	cache_t *prev = NULL;
	while (ptr != NULL && ptr->next != NULL) {
		prev = ptr;
		ptr = ptr->next;
	}
	if (prev != NULL) {
		prev->next = NULL;
	}
	else {
		cache_head = NULL;
	}
	cache_size -= ptr->size;
	Free(ptr);
}

/*
 * Find if the item is in the cache
 */
cache_t *cache_find(char *uri)
{
	P(&mutex);
	readcnt++;
	if (readcnt == 1)
		P(&w);
	V(&mutex);
	cache_t *ptr = cache_head;
	cache_t *result = NULL;
	/* Linearly look through the list */
	while (ptr != NULL) {
		/* When item is found */
		if (!strcmp(uri, ptr->uri)) {
			result = ptr;
			ptr->visited = 1;
			break;
		}
		ptr = ptr->next;
	}
	P(&mutex);
	readcnt--;
	if (readcnt == 0) {
		/* Only update cache sequence before last write lock */
		cache_update();
		V(&w);
	}
	V(&mutex);
	return result;
}

/* 
 * Update cache sequence based on recently used policy
 */
void cache_update()
{
	cache_t *curr = cache_head;
	cache_t *prev = NULL;
	while (curr != NULL) {
		if (curr->visited == 1 && prev != NULL) {
			prev->next = curr->next;
			curr->next = cache_head;
			cache_head = curr;
			curr = prev->next;

		}
		else {
			prev = curr;
			curr = curr->next;
		}
	}
	cache_mark_clear();
}

/*
 * Initialize cache visit status
 */
void cache_mark_clear()
{
	cache_t *ptr = cache_head;
	while (ptr != NULL) {
		ptr->visited = 0;
		ptr = ptr->next;
	}
}