#include "easynet.h"
#include "easylogger.h"
#include "easymemory.h"
#include <string.h>

BOOL s_ez_network_initialized = FALSE;
ez_ClientList* s_ez_client_list = NULL;
ez_ServerList* s_ez_server_list = NULL;
ez_ConnectionList* s_ez_connection_list = NULL;

ez_Buffer* ez_generate_buffer(size_t size) {
	ez_Buffer* buffer = EZ_ALLOC(1, sizeof(ez_Buffer));
	buffer->bytes = EZ_ALLOC(size, sizeof(EZ_BYTE));
	buffer->max_length = size;
	buffer->current_length = 0;
	return buffer;
}

void ez_clean_buffer(ez_Buffer* buffer) {
	if (buffer != NULL) {
		if (buffer->bytes != NULL) {
			EZ_FREE(buffer->bytes);
			EZ_FREE(buffer);
		} else {
			EZ_WARN("Unable to clean empty bytes");
		}
	} else {
		EZ_WARN("Unable to clean a null pointer");
	}
}

BOOL ez_translate_buffer(ez_Buffer* buffer, void* destination, size_t destsize) {
	if (buffer->current_length > destsize) {
		EZ_ERROR("Cannot translate buffer to a smaller size destination");
		return FALSE;
	}
	memcpy(destination, buffer->bytes, buffer->current_length);
	return TRUE;
}

BOOL ez_record_buffer(ez_Buffer* buffer, void* source, size_t sourcesize) {
	if (buffer->max_length < sourcesize) {
		EZ_ERROR("Cannot translate source to a smaller size buffer");
		return FALSE;
	}
	buffer->current_length = sourcesize;
	memcpy(buffer->bytes, source, sourcesize);
	memset(buffer->bytes + sourcesize, 0, buffer->max_length - sourcesize);
	return TRUE;
}

BOOL ez_init_network() {
	#ifdef __WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		s_ez_network_initialized = FALSE;
        EZ_ERROR("Failed to initialize Winsock");
		return FALSE;
    }
	s_ez_network_initialized = TRUE;
	return TRUE;
	#elif !defined(__linux__) && !defined(__APPLE__)
	#error "Unsupported operating system detected"
	#endif
	s_ez_network_initialized = TRUE;
	return TRUE;
}

BOOL ez_clean_network() {
	#ifdef __WIN32
    WSACleanup();
	#elif !defined(__linux__) && !defined(__APPLE__)
	#error "Unsupported operating system detected"
	#endif
	while (s_ez_server_list != NULL) {
		BOOL res = ez_clean_server(s_ez_server_list->server);
		if (!res) {
			EZ_ERROR("Unable to clean servers");
			return FALSE;
		}
	}
	while (s_ez_client_list != NULL) {
		BOOL res = ez_clean_client(s_ez_client_list->client);
		if (!res) {
			EZ_ERROR("Unable to clean clients");
			return FALSE;
		}
	}
	while (s_ez_connection_list != NULL) {
		BOOL res = ez_close_connection(s_ez_connection_list->connection);
		if (!res) {
			EZ_ERROR("Unable to clean connections");
			return FALSE;
		}
	}
	s_ez_network_initialized = FALSE;
	return TRUE;
}

BOOL ez_check_network() {
	if (!s_ez_network_initialized) {
		EZ_WARN("EasyNet was not intialized. Please initialize it using EZ_INIT_NETWORK(). Implicitly intializing now...");
		return ez_init_network();
	}
	return TRUE;
}

Ipv4 ez_get_my_ip() {
	ez_check_network();
	Ipv4 ip = { 0 };
    struct sockaddr_in dest;
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
	EZ_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == EZ_INVALID_SOCK) {
        EZ_ERROR("Socket creation failed");
        return ip;
    }
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);
    connect(sock, (struct sockaddr*)&dest, sizeof(dest));
    if (getsockname(sock, (struct sockaddr*)&name, &namelen) == -1) {
        EZ_ERROR("Unable to get sock name");
        return ip;
	}
	memcpy(ip.address, &name.sin_addr.s_addr, 4);
    EZ_CLOSE_SOCKET(sock);
	return ip;
}

ez_Server* ez_generate_server() {
	ez_check_network();
	ez_Server* server = EZ_ALLOC(1, sizeof(ez_Server));
	server->port = 0;
	server->socket = EZ_INVALID_SOCK;
	server->open = FALSE;
	server->udp = FALSE;
	if (s_ez_server_list == NULL) {
		s_ez_server_list = EZ_ALLOC(1, sizeof(ez_ServerList));
		s_ez_server_list->server = server;
		s_ez_server_list->next = NULL;
	} else {
		ez_ServerList* t = s_ez_server_list;
		s_ez_server_list = EZ_ALLOC(1, sizeof(ez_ServerList));
		s_ez_server_list->server = server;
		s_ez_server_list->next = t;
	}
	return server;
}

BOOL ez_open_server(ez_Server* server, uint16_t port) {
	ez_check_network();
	if (server->open) {
		EZ_ERROR("Unable to open a server that's already open");
		return FALSE;
	}
	if (port < 1024) {
		EZ_ERROR("Unable to open on reserved port \"%d\"", (int)port);
		return FALSE;
	}
	server->port = port;
	if (server->udp) {
		struct addrinfo hints, *res, *p;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		char portbuf[32] = { 0 };
		sprintf(portbuf, "%d", (int)server->port);
		int status;
		if ((status = getaddrinfo(NULL, portbuf, &hints, &res)) != 0) {
			EZ_ERROR("Unable to get address info %d", status);
			return FALSE;
		}
		for (p = res; p != NULL; p = p->ai_next) {
			if ((server->socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == EZ_INVALID_SOCK) {
				EZ_WARN("Bad server socket detected");
				continue;
			}
			EZ_OPT_TYPE optval = 1;
			if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
				EZ_ERROR("Unable to set server socket options");
				EZ_CLOSE_SOCKET(server->socket);
				return FALSE;
			}
			#ifdef __linux__
			optval = IP_PMTUDISC_DO;
			if (setsockopt(server->socket, IPPROTO_IP, IP_MTU_DISCOVER, &optval, sizeof(optval)) < 0) {
				EZ_ERROR("Unable to set server socket options");
				EZ_CLOSE_SOCKET(server->socket);
				return FALSE;
			}
			#endif
			if (bind(server->socket, p->ai_addr, p->ai_addrlen) == (int)EZ_INVALID_SOCK) {
				EZ_ERROR("Unable to bind server socket");
				EZ_CLOSE_SOCKET(server->socket);
				return FALSE;
			}
			break;
		}
		if (p == NULL) {
			EZ_ERROR("Unable to find viable server socket");
			return FALSE;
		}
		freeaddrinfo(res);
	} else {
		server->socket = socket(AF_INET, SOCK_STREAM, 0);
		if (server->socket == EZ_INVALID_SOCK) {
			EZ_ERROR("Unable to create a new socket");
			return FALSE;
		}
		EZ_OPT_TYPE optval = 1;
		if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
			EZ_ERROR("Unable to set server socket options");
			EZ_CLOSE_SOCKET(server->socket);
			return FALSE;
		}
		struct sockaddr_in serverAddr;
		memset(&serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons((u_short)(server->port));
		if (bind(server->socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == (int)EZ_INVALID_SOCK) {
			EZ_ERROR("Unable to bind server socket");
			EZ_CLOSE_SOCKET(server->socket);
			return FALSE;
		}
		if (listen(server->socket, (SOMAXCONN) == EZ_INVALID_SOCK)) {
			EZ_ERROR("Unable to listen for connections");
			EZ_CLOSE_SOCKET(server->socket);
			return FALSE;
		}
	}
    server->open = TRUE;
	return TRUE;
}

ez_Connection* ez_server_accept(ez_Server* server) {
	ez_Connection* connection = EZ_ALLOC(1, sizeof(ez_Connection));
	ez_check_network();
    if (!server->open) {
        EZ_WARN("Unable to accept connections on a server that is not open!");
        return NULL;
    }
    EZ_SOCKET clientSocket;
    clientSocket = accept(server->socket, NULL, NULL);
    if (clientSocket == EZ_INVALID_SOCK) {
        EZ_ERROR("Failed to accept a connection");
        return NULL;
    }
	connection->socket = clientSocket;
	connection->server = server;
	if (s_ez_connection_list == NULL) {
		s_ez_connection_list = EZ_ALLOC(1, sizeof(ez_ConnectionList));
		s_ez_connection_list->connection = connection;
		s_ez_connection_list->next = NULL;
	} else {
		ez_ConnectionList* t = s_ez_connection_list;
		s_ez_connection_list = EZ_ALLOC(1, sizeof(ez_ConnectionList));
		s_ez_connection_list->connection = connection;
		s_ez_connection_list->next = t;
	}
    return connection;
}

ez_Connection* ez_server_accept_timed(ez_Server* server, size_t timeout) {
	fd_set readfds;
	struct timeval tout;
	FD_ZERO(&readfds);
	FD_SET(server->socket, &readfds);
	tout.tv_sec = 0;
	tout.tv_usec = timeout;
	int found = select(server->socket + 1, &readfds, NULL, NULL, &tout);
	if (found <= 0) return NULL;
	return ez_server_accept(server);
}

BOOL ez_close_connection(ez_Connection* connection) {
	if (connection == NULL) {
		EZ_ERROR("Cannot clean a null pointer");
		return FALSE;
	}
	ez_ConnectionList* tracker = s_ez_connection_list;
	if (tracker->connection == connection) {
		s_ez_connection_list = tracker->next;
		EZ_CLOSE_SOCKET(tracker->connection->socket);
		EZ_FREE(tracker->connection);
		EZ_FREE(tracker);
	} else {
		while (tracker->next != NULL) {
			if (((ez_ConnectionList*)tracker->next)->connection == connection) {
				ez_ConnectionList* dead = tracker->next;
				tracker->next = ((ez_ConnectionList*)tracker->next)->next;
				EZ_CLOSE_SOCKET(dead->connection->socket);
				EZ_FREE(dead->connection);
				EZ_FREE(dead);
				return TRUE;
			}
			tracker = tracker->next;
		}
		EZ_WARN("Unable to clean an untracked connection object - please use the dedicated EZ_SERVER_ACCPET() function to get connection objects");
		return FALSE;
	}
	return TRUE;
}

BOOL ez_close_server(ez_Server* server) {
	ez_check_network();
	if (!server->open) {
		EZ_ERROR("Cannot close a server that isn't already open");
		return FALSE;
	}
	EZ_CLOSE_SOCKET(server->socket);
	server->open = FALSE;
	return TRUE;
}

BOOL ez_clean_server(ez_Server* server) {
	if (server == NULL) {
		EZ_ERROR("Cannot clean a null pointer");
		return FALSE;
	}
	ez_ServerList* tracker = s_ez_server_list;
	if (tracker->server == server) {
		s_ez_server_list = tracker->next;
		if (tracker->server->open) ez_close_server(tracker->server);
		EZ_FREE(tracker->server);
		EZ_FREE(tracker);
	} else {
		while (tracker->next != NULL) {
			if (((ez_ServerList*)tracker->next)->server == server) {
				ez_ServerList* dead = tracker->next;
				tracker->next = ((ez_ServerList*)tracker->next)->next;
				if (dead->server->open) ez_close_server(dead->server);
				EZ_FREE(dead->server);
				EZ_FREE(dead);
				return TRUE;
			}
			tracker = tracker->next;
		}
		EZ_WARN("Unable to clean an untracked server object - please use the dedicated EZ_GENERATE_SERVER() function to create server objects");
		return FALSE;
	}
	return TRUE;
}

BOOL ez_server_ask(ez_Connection* connection, ez_Buffer* buffer) {
	ez_check_network();
	#ifdef _WIN32
    unsigned long l;
    ioctlsocket(connection->socket, FIONREAD, &l);
    if (l > 0) {
		int64_t recbytes = recv(connection->socket, (char*)buffer->bytes, buffer->max_length, 0);
		if (recbytes <= 0) {
			buffer->current_length = 0;
			return FALSE;
		}
		buffer->current_length = (size_t)recbytes;
		return TRUE;
	}
	return FALSE;
    #elif defined(__linux__) || defined(__APPLE__)
	ssize_t retval = recv(connection->socket, (char*)buffer->bytes, buffer->max_length, MSG_DONTWAIT);
	if (retval <= 0) {
		buffer->current_length = 0;
		return FALSE;
	}
	buffer->current_length = (size_t)retval;
	return TRUE;
	#else
    #error "Unsupported operating system detected!"
    #endif
}

BOOL ez_server_recieve(ez_Connection* connection, ez_Buffer* buffer) {
	ez_check_network();
	int64_t retval = recv(connection->socket, (char*)buffer->bytes, buffer->max_length, 0);
    if (retval < 0) {
        EZ_WARN("An error occured while recieving data");
        return FALSE;
    }
	if (retval == 0) return FALSE;
    buffer->current_length = (size_t)retval;
	return TRUE;
}

Destination ez_server_recieve_from(ez_Server* server, ez_Buffer* buffer) {
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	Destination dest = { 0 };
	int recieved = recvfrom(server->socket, (char*)buffer->bytes, buffer->max_length, 0,
		(struct sockaddr*)&client_addr, &addr_len);
	if (recieved < 0) {
		EZ_WARN("An error occured while recieving data");
		return dest;
	}
	buffer->current_length = (size_t)recieved;
	dest.port = ntohs(client_addr.sin_port);
	memcpy(dest.address.address, &client_addr.sin_addr.s_addr, 4);
	return dest;
}

Destination ez_server_recieve_from_timed(ez_Server* server, ez_Buffer* buffer, size_t timeout) {
	fd_set readfds;
	struct timeval tout;
	FD_ZERO(&readfds);
	FD_SET(server->socket, &readfds);
	tout.tv_sec = 0;
	tout.tv_usec = timeout;
	int found = select(server->socket + 1, &readfds, NULL, NULL, &tout);
	if (found <= 0) {
		Destination dest = { 0 };
		return dest;
	}
	return ez_server_recieve_from(server, buffer);
}

BOOL ez_server_throw(ez_Server* server, Destination destination, ez_Buffer* buffer) {
	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(destination.port);
	memcpy(&dest_addr.sin_addr.s_addr, destination.address.address, 4);
	char prev = buffer->bytes[buffer->current_length];
	buffer->bytes[buffer->current_length] = '\0';
	ssize_t sent = sendto(server->socket, (char*)buffer->bytes, buffer->current_length, 0,
		(struct sockaddr*)&dest_addr, sizeof(dest_addr));
	buffer->bytes[buffer->current_length] = prev;
	if (sent < 0) {
		EZ_WARN("An error occured while sending data");
		return FALSE;
	}
	return TRUE;
}

BOOL ez_server_send(ez_Connection* connection, ez_Buffer* buffer) {
	ez_check_network();
	#if defined(__linux__)
    int64_t sent = send(connection->socket, (char*)buffer->bytes, buffer->current_length, MSG_NOSIGNAL);
	#else 
	int64_t sent = send(connection->socket, (char*)buffer->bytes, buffer->current_length, 0);
	#endif
    if (sent < 0) {
        EZ_WARN("An error occured while sending data");
        return FALSE;
    }
	return TRUE;
}

ez_Client* ez_generate_client() {
	ez_check_network();
	ez_Client* client = EZ_ALLOC(1, sizeof(ez_Client));
	client->open = FALSE;
	if (s_ez_client_list == NULL) {
		s_ez_client_list = EZ_ALLOC(1, sizeof(ez_ClientList));
		s_ez_client_list->client = client;
		s_ez_client_list->next = NULL;
	} else {
		ez_ClientList* t = s_ez_client_list;
		s_ez_client_list = EZ_ALLOC(1, sizeof(ez_ClientList));
		s_ez_client_list->client = client;
		s_ez_client_list->next = t;
	}
	return client;
}

BOOL ez_connect_client(ez_Client* client, Ipv4 address, uint16_t port) {
	ez_check_network();
	if (client->open) {
		EZ_ERROR("Unable to connect a client that's already connected");
		return FALSE;
	}
	if (port < 1024) {
		EZ_ERROR("Unable to connect to reserved port \"%d\"", (int)port);
		return FALSE;
	}
	client->port = port;
	client->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client->socket == EZ_INVALID_SOCK) {
		EZ_ERROR("Unable to create a new socket");
		return FALSE;
	}
	struct sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons((u_short)(client->port));
	memcpy(&client->address, &address, sizeof(Ipv4));
	char addrstr[64];
	sprintf(addrstr, "%d.%d.%d.%d",
		 client->address.address[0],
		 client->address.address[1],
		 client->address.address[2],
		 client->address.address[3]);
	if (inet_pton(AF_INET, addrstr, &serverAddr.sin_addr) <= 0) {
		EZ_ERROR("Invalid IP address, unable to connect");
		EZ_CLOSE_SOCKET(client->socket);
		return FALSE;
	}
	if (connect(client->socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
		EZ_ERROR("Unable to connect configured client to server");
		EZ_CLOSE_SOCKET(client->socket);
		return FALSE;
	}
	client->open = TRUE;
	return TRUE;
}

BOOL ez_disconnect_client(ez_Client* client) {
	ez_check_network();
	if (!client->open) {
		EZ_ERROR("Cannot disconnect a client that isn't already connected");
		return FALSE;
	}
	EZ_CLOSE_SOCKET(client->socket);
	client->open = FALSE;
	return TRUE;
}

BOOL ez_clean_client(ez_Client* client) {
	if (client == NULL) {
		EZ_ERROR("Cannot clean a null pointer");
		return FALSE;
	}
	ez_ClientList* tracker = s_ez_client_list;
	if (tracker->client == client) {
		s_ez_client_list = tracker->next;
		if (tracker->client->open) ez_disconnect_client(tracker->client);
		EZ_FREE(tracker->client);
		EZ_FREE(tracker);
	} else {
		while (tracker->next != NULL) {
			if (((ez_ClientList*)tracker->next)->client == client) {
				ez_ClientList* dead = tracker->next;
				tracker->next = ((ez_ClientList*)tracker->next)->next;
				if (dead->client->open) ez_disconnect_client(dead->client);
				EZ_FREE(dead->client);
				EZ_FREE(dead);
				return TRUE;
			}
			tracker = tracker->next;
		}
		EZ_WARN("Unable to clean an untracked client object - please use the dedicated EZ_GENERATE_CLIENT() function to create client objects");
		return FALSE;
	}
	return TRUE;
}

BOOL ez_client_ask(ez_Client* client, ez_Buffer* buffer) {
	ez_check_network();
	#ifdef _WIN32
    unsigned long l;
    ioctlsocket(client->socket, FIONREAD, &l);
    if (l > 0) {
		int64_t recbytes = recv(client->socket, (char*)buffer->bytes, buffer->max_length, 0);
		if (recbytes <= 0) {
			buffer->current_length = 0;
			return FALSE;
		}
		buffer->current_length = (size_t)recbytes;
		return TRUE;
	}
	return FALSE;
    #elif defined(__linux__) || defined(__APPLE__)
	ssize_t retval = recv(client->socket, (char*)buffer->bytes, buffer->max_length, MSG_DONTWAIT);
	if (retval <= 0) {
		buffer->current_length = 0;
		return FALSE;
	}
	buffer->current_length = (size_t)retval;
	return TRUE;
	#else
    #error "Unsupported operating system detected!"
    #endif
}

BOOL ez_client_recieve(ez_Client* client, ez_Buffer* buffer) {
	ez_check_network();
	int64_t retval = recv(client->socket, (char*)buffer->bytes, buffer->max_length, 0);
    if (retval < 0) {
        EZ_WARN("An error occured while recieving data");
        return FALSE;
    }
	if (retval == 0) return FALSE;
    buffer->current_length = (size_t)retval;
	return TRUE;
}

BOOL ez_client_send(ez_Client* client, ez_Buffer* buffer) {
	ez_check_network();
	#ifdef __linux__
    int64_t sent = send(client->socket, (char*)buffer->bytes, buffer->current_length, MSG_NOSIGNAL);
	#else 
	int64_t sent = send(client->socket, (char*)buffer->bytes, buffer->current_length, 0);
	#endif
    if (sent < 0) {
        EZ_WARN("An error occured while sending data");
        return FALSE;
    }
	return TRUE;
}
