/* This is a basic TCP/TLS echo server. */

#include <libusockets_new.h>
const int SSL = 1;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Our socket extension */
struct echo_socket {
	char *backpressure;
	int length;
};

/* Our socket context extension */
struct echo_context {

};

/* Loop wakeup handler */
void on_wakeup(struct us_loop *loop) {

}

/* Loop pre iteration handler */
void on_pre(struct us_loop *loop) {

}

/* Loop post iteration handler */
void on_post(struct us_loop *loop) {

}

/* Socket writable handler */
struct us_new_socket_t *on_echo_socket_writable(struct us_new_socket_t *s) {
	struct echo_socket *es = (struct echo_socket *) us_new_socket_ext(SSL, s);

	/* Continue writing out our backpressure */
	int written = us_new_socket_write(SSL, s, es->backpressure, es->length, 0);
	if (written != es->length) {
		char *new_buffer = (char *) malloc(es->length - written);
		memcpy(new_buffer, es->backpressure, es->length - written);
		free(es->backpressure);
		es->backpressure = new_buffer;
		es->length -= written;
	} else {
		free(es->backpressure);
		es->length = 0;
	}

	/* Client is not boring */
	us_new_socket_timeout(SSL, s, 30);

	return s;
}

/* Socket closed handler */
struct us_new_socket_t *on_echo_socket_close(struct us_new_socket_t *s) {
	struct echo_socket *es = (struct echo_socket *) us_new_socket_ext(SSL, s);

	printf("Client disconnected\n");

	free(es->backpressure);

	return s;
}

/* Socket half-closed handler */
struct us_new_socket_t *on_echo_socket_end(struct us_new_socket_t *s) {
	us_new_socket_shutdown(SSL, s);
	return us_new_socket_close(SSL, s);
}

/* Socket data handler */
struct us_new_socket_t *on_echo_socket_data(struct us_new_socket_t *s, char *data, int length) {
	struct echo_socket *es = (struct echo_socket *) us_new_socket_ext(SSL, s);

	/* Print the data we received */
	printf("Client sent <%.*s>\n", length, data);

	/* Send it back or buffer it up */
	int written = us_new_socket_write(SSL, s, data, length, 0);
	if (written != length) {
		char *new_buffer = (char *) malloc(es->length + length - written);
		memcpy(new_buffer, es->backpressure, es->length);
		memcpy(new_buffer + es->length, data + written, length - written);
		free(es->backpressure);
		es->backpressure = new_buffer;
		es->length += length - written;
	}

	/* Client is not boring */
	us_new_socket_timeout(SSL, s, 30);

	return s;
}

/* Socket opened handler */
struct us_new_socket_t *on_echo_socket_open(struct us_new_socket_t *s, int is_client) {
	struct echo_socket *es = (struct echo_socket *) us_new_socket_ext(SSL, s);

	/* Initialize the new socket's extension */
	es->backpressure = 0;
	es->length = 0;

	/* Start a timeout to close the socekt if boring */
	us_new_socket_timeout(SSL, s, 30);

	printf("Client connected\n");

	return s;
}

/* Socket timeout handler */
struct us_new_socket_t *on_echo_socket_timeout(struct us_new_socket_t *s) {
	printf("Client was idle for too long\n");
	return us_new_socket_close(SSL, s);
}

int main() {
	/* The event loop */
	struct us_loop *loop = us_create_loop(1, on_wakeup, on_pre, on_post, 0);

	/* Socket context */
	struct us_new_socket_context_options_t options = {};
	options.key_file_name = "/home/alexhultman/uWebSockets.js/misc/key.pem";
	options.cert_file_name = "/home/alexhultman/uWebSockets.js/misc/cert.pem";
	options.passphrase = "1234";

	struct us_new_socket_context_t *echo_context = us_new_create_socket_context(SSL, loop, sizeof(struct echo_context), options);


	/* Registering event handlers */
	us_new_socket_context_on_open(SSL, echo_context, on_echo_socket_open);
	us_new_socket_context_on_data(SSL, echo_context, on_echo_socket_data);
	us_new_socket_context_on_writable(SSL, echo_context, on_echo_socket_writable);
	us_new_socket_context_on_close(SSL, echo_context, on_echo_socket_close);
	us_new_socket_context_on_timeout(SSL, echo_context, on_echo_socket_timeout);
	us_new_socket_context_on_end(SSL, echo_context, on_echo_socket_end);

	/* Start accepting echo sockets */
	struct us_listen_socket *listen_socket = us_new_socket_context_listen(SSL, echo_context, 0, 3000, 0, sizeof(struct echo_socket));

	if (listen_socket) {
		printf("Listening on port 3000...\n");
		us_loop_run(loop);
	} else {
		printf("Failed to listen!\n");
	}
}
