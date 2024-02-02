#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "list.h"

#define HOST "0.0.0.0"
#define PORT 9000
#define BACKLOG 255
#define MAX_MESSAGE_LENGTH 64

static uv_loop_t *loop;
static list_t *users;

struct user_s {
	char id[24];
	uv_tcp_t handle;
};

typedef struct user_s user_t;

struct write_req_s {
	uv_write_t req;
	uv_buf_t buf;
	user_t *user;
};

typedef struct write_req_s write_req_t;

int check_error(const char *what, int code) {
	if (code == 0) {
		return 0;
	}

	fprintf(stderr, "[ERROR] %s: %s %s\n", what, uv_err_name(code), uv_strerror(code));

	return code;
}

void check_error_f(const char *what, int code) {
	if (check_error(what, code)) {
		exit(1);
	}
}

int init_user(user_t *user) {
	struct sockaddr_in client_addr;
	char addr[16];
	int client_addr_len = sizeof(client_addr);
	
	if (check_error("uv_tcp_getpeername", uv_tcp_getpeername(&user->handle, (struct sockaddr*) &client_addr, &client_addr_len))) {
		return -1;
	}

	if (check_error("uv_inet_ntop", uv_inet_ntop(AF_INET, &client_addr.sin_addr, addr, sizeof(addr)))) {
		return -1;
	}

	snprintf(user->id, sizeof(user->id), "%s:%d", addr, ntohs(client_addr.sin_port));

	return 0;
}

void alloc_client_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	buf->base = (char*) malloc(MAX_MESSAGE_LENGTH + 1);
	buf->len = MAX_MESSAGE_LENGTH + 1;
}

void on_client_close(uv_handle_t* handle) {
	user_t *user = (user_t*) handle->data;

	printf("[INFO] Client disconnected %s\n", user->id);

	node_t *n = list_find(users, user);
	if (n) {
		list_remove(users, n);
		free(n);
	}

	free(user);
}

void on_message_write(uv_write_t* req, int status) {
	if (status < 0) {
		check_error("Can't write", status);
		return;
	}

	write_req_t *write_req = (write_req_t*) req;
	free(write_req->buf.base);
	free(write_req);
}

void send_message(char *msg, int msg_len, user_t *recipient) {
	write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));

	req->user = recipient;
	req->buf = uv_buf_init(malloc(msg_len), msg_len);
	memcpy(req->buf.base, msg, msg_len);

	if (check_error("uv_write", uv_write((uv_write_t*) req, (uv_stream_t*) &recipient->handle, &req->buf, 1, on_message_write))) {
		free(req->buf.base);
		free(req);
		uv_close((uv_handle_t*) &recipient->handle, on_client_close);
	}
}

void broadcast(char *msg, int msg_len, user_t *author) {
	node_t *n = users->head;

	while (n) {
		if (n->data != (void*) author) {
			send_message(msg, msg_len, (user_t*) n->data);
		}
		n = n->next;
	}
}

void on_client_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
	user_t *user = (user_t*) client->data;

	if (nread == UV_EOF) {
		uv_close((uv_handle_t*) client, on_client_close);
	} else if (nread < 0) {
		fprintf(stderr, "[ERROR] Client %s read error %s\n", user->id, uv_strerror(nread));
	} else if (nread > MAX_MESSAGE_LENGTH) {
		printf("[INFO] Invalid message from %s\n", user->id);
		uv_close((uv_handle_t*) client, on_client_close);
	} else {
		buf->base[nread] = 0;
		printf("[INFO] Message from %s: %s", user->id, buf->base);

		broadcast(buf->base, strlen(buf->base), user);
	}

	free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) {
	if (status < 0) {
		check_error("New connection", status);
		return;
	}

	user_t *user = (user_t*) malloc(sizeof(user_t));

	if (check_error("Client uv_tcp_init", uv_tcp_init(loop, &user->handle))) {
		free(user);
		return;
	}

	user->handle.data = user;

	if (check_error("Client uv_accept", uv_accept(server, (uv_stream_t*) &user->handle))) {
		uv_close((uv_handle_t*) &user->handle, on_client_close);
		return;
	}

	if (init_user(user)) {
		uv_close((uv_handle_t*) &user->handle, on_client_close);
	}

	list_prepend(users, new_node((void*) user));

	printf("[INFO] New connection from %s\n", user->id);

	if (check_error("Client uv_read_start", uv_read_start((uv_stream_t*) &user->handle, alloc_client_buffer, on_client_read))) {
		uv_close((uv_handle_t*) &user->handle, on_client_close);
	}

	return;
}

int main() {
	uv_tcp_t server;
	struct sockaddr_in addr;

	loop = uv_default_loop();
	users = (list_t*) malloc(sizeof(list_t));

	check_error_f("uv_tcp_init", uv_tcp_init(loop, &server));
	check_error_f("uv_ip4_addr", uv_ip4_addr(HOST, PORT, &addr));
	check_error_f("uv_tcp_bind", uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0));
	check_error_f("uv_listen", uv_listen((uv_stream_t*) &server, BACKLOG, on_new_connection));

	printf("[INFO] Listen on %s:%d\n", HOST, PORT);

	return uv_run(loop, UV_RUN_DEFAULT);
}
