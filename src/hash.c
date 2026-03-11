#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

u64 Hash(char *key) {
	uint64_t hash = 5381;

	int32_t c;
	while((c = *key++) != '\0') 
		hash = ((hash << 5) + hash) + c;

	return hash;
}

u32 LinearProbe(uint32_t index, uint32_t attempt, uint32_t size) {
	return (index + attempt) % size;
}

void HashResize(HashMap *map) {
	u32 new_cap = map->capacity << 1;

	if(new_cap == 0)
		new_cap = 1;

	HashNode *nodes = calloc(new_cap, sizeof(HashNode));

	for(u32 i = 0; i < new_cap; i++) {
		memset(nodes[i].key, '\0', 128);
		nodes[i].val = 0;
	}

	for(u32 i = 0; i < map->capacity; i++) {
		HashNode node = map->nodes[i]; 

		if(node.key[0] == '\0') continue;

		u64 id = Hash(node.key) % new_cap;
		nodes[id] = node;
	}

	map->capacity = new_cap;

	HashNode *new_ptr = realloc(map->nodes, sizeof(HashNode) * new_cap);
	map->nodes = new_ptr;

	memcpy(map->nodes, nodes, sizeof(HashNode) * new_cap);
	free(nodes);
}

void HashInsert(HashMap *map, char *key, int val) {
	if(map->count + 1 >= map->capacity) 
		HashResize(map);

	u64 id = Hash(key) % map->capacity;
	u32 attempt = 0;
	
	//printf("Checking collision...\n");

	while(map->nodes[id].key[0] != '\0')
		id = LinearProbe(id, ++attempt, map->capacity);

	/*
	printf("Making new node...\n");
	printf("key: %s\n", key);
	printf("val: %d\n", val);
	printf("Inserting new node...\n");
	*/

	HashNode node = (HashNode) { .val = val, .key = { '\0' } };
	memcpy(node.key, key, strlen(key));

	map->nodes[id] = node;
	map->count++;
}

int HashFetch(HashMap *map, char *key) {
	//printf("Fetching value for key [%s]\n", key);

	u64 id = Hash(key) % map->capacity;	
	u32 attempt = 0;

	short found = 0;
	while(attempt < map->capacity) {
		if(!strcmp(map->nodes[id].key, key)) { 	
			found = 1;
			break;
		}

		id = LinearProbe(id, ++attempt, map->capacity);
	}

	if(found) { 
		//printf("key found!, val = %d\n", map->nodes[id].val);
		return map->nodes[id].val;
	}

	return 0;
}

void DisplayNodes(HashMap *map) {
	for(uint32_t i = 0; i < map->capacity; i++) {
		HashNode *node = &map->nodes[i];
		//if(!node->key || !node->key) continue;
		if(node->key[0] == '\0') continue;

		puts("--------------------------");
		printf("%u:	", i);

		printf("%s, %d\n", node->key, node->val);
	}	
}

