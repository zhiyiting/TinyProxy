#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cache_t cache_t;
struct cache_t {
	cache_t *next;
	char uri[MAXLINE];
	char visited;
	size_t size;
	unsigned char *content;
};

void cache_init();
void cache_store(size_t filesize, char *uri, unsigned char *response);
void cache_add(cache_t *ptr);
void cache_delete();
cache_t *cache_find(char *uri);
void cache_update();
void cache_mark_clear();

#endif