#include <stdio.h>
#include <stdlib.h>

typedef struct node_s node_t;
typedef struct list_s list_t;

struct node_s {
	node_t *next;
	void *data;
};

struct list_s {
	node_t *head;
};

node_t *new_node(void *data) {
	node_t *n = (node_t*) malloc(sizeof(node_t));

	n->next = NULL;
	n->data = data;

	return n;
}

void list_prepend(list_t *l, node_t *n) {
	if (l->head) {
		n->next = l->head;
		l->head = n;
	} else {
		l->head = n;
	}
}

void list_remove(list_t *l, node_t *n) {
	node_t *p = NULL;
	node_t *c = NULL;

	if (l->head == n) {
		l->head = l->head->next;
	} else {
		p = l->head;
		c = l->head->next;

		while (c) {
			if (c == n) {
				p->next = c->next;
				return;
			} else {
				p = c;
				c = c->next;
			}
		}
	}
}

node_t *list_find(list_t *l, void *data) {
	node_t *n = l->head;

	while (n) {
		if (n->data == (void*) data) {
			return n;
		}

		n = n->next;
	}

	return NULL;
}

void list_debug(list_t *l) {
	node_t *c = l->head;

	if (!c) {
		printf("list is empty\n");
		return;
	}

	while(c) {
		printf("node %p, data %p, next %p\n", c, c->data, c->next);
		c = c->next;
	}
}
