/**
 * AngstromDB server.
 *
 * @author	Amaury Bouchard <amaury@amaury.net>
 * @copyright	© 2013, Amaury Bouchard
 */
#include "angstrom.h"

/* *** Prototypes of private functions. *** */
static int _create_listening_socket(unsigned short port);
static void _main_thread_loop(angstrom_t *angstrom);

/**
 * Main function of the program.
 */
int main() {
	angstrom_t *angstrom;
	int i;

	// server init
	angstrom = calloc(1, sizeof(angstrom_t));
	angstrom->socket = angstrom->threads_socket = -1;
	angstrom->comm_threads = calloc(NBR_THREADS, sizeof(comm_thread_t));
	// open the database
	angstrom->db = database_open(DEFAULT_DB_PATH, DEFAULT_MAPSIZE, NBR_THREADS);
	// create the nanomsg socket for threads communication
	angstrom->threads_socket = nn_socket(AF_SP, NN_PUSH);
	nn_bind(angstrom->threads_socket, ENDPOINT_THREADS_SOCKET);
	// create the writer thread
	pthread_create(&angstrom->writer_tid, NULL, thread_writer_loop, angstrom);
	// create communication threads
	for (i = 0; i < NBR_THREADS; i++) {
		comm_thread_t *thread = &(angstrom->comm_threads[i]);

		thread->client_sock = -1;
		thread->angstrom = angstrom;
		pthread_create(&thread->tid, 0, thread_comm_loop, thread);
		pthread_detach(thread->tid);
	}
	// create listening socket
	angstrom->socket = _create_listening_socket(DEFAULT_PORT);
	// server loop
	_main_thread_loop(angstrom);
	return (0);
}

/* ************** PRIVATE FUNCTIONS **************** */
/**
 * @function	_main_thread_loop
 *		Main thread's execution loop. Wait for new incoming connections
 *		and send them to the communication threads.
 * @param	angstrom	Pointer to the server's structure.
 */
static void _main_thread_loop(angstrom_t *angstrom) {
	int fd;
	struct sockaddr_in addr;
	unsigned int addr_size;
	const int on = 1;

	addr_size = sizeof(addr);
	for (; ; ) {
		bzero(&addr, addr_size);
		// accept a new connection
		if ((fd = accept(angstrom->socket, (struct sockaddr*)&addr,
				 &addr_size)) < 0)
			continue ;
		setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on));
		// send the file descriptor number to communication threads
		nn_send(angstrom->threads_socket, &fd, sizeof(fd), 0);
	}
}

/**
 * @function	_create_listening_socket
 *		Create a socket that will listening for incoming connections.
 * @param	port	Port number.
 * @return	The created socket.
 */
static int _create_listening_socket(unsigned short port) {
	int sock;
	struct sockaddr_in addr;
	unsigned int addr_size;
	const int on = 1;

	// create the socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	// some options
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&on, sizeof(on));
	setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&on, sizeof(on));
	// binding to any interface
	addr_size = sizeof(addr);
	bzero(&addr, addr_size);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	bind(sock, (struct sockaddr*)&addr, addr_size);
	listen(sock, SOMAXCONN);
	return (sock);
}
