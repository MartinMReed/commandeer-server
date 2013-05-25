/**
 * Copyright (c) 2009-2012 Martin M Reed
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "socket_server.h"

#include <stdio.h>
#include <strings.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>

/**
 * Set up the server connection by opening the socket and binding to
 * the given port address.
 *
 * @param port The port address to bind to.
 */
hbc::socket_server::socket_server(int port)
{
    this->port = port;
}

hbc::socket_error hbc::socket_server::bind()
{
    hbc::bind(&fd, port);
}

hbc::socket_error hbc::bind(int *fd, int port)
{
    // initialize and zero out a socket address
    struct sockaddr_in s_addr;
    bzero((char *) &s_addr, sizeof(s_addr));

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(port);

    // open a socket to listen on
    int _fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (_fd < 0) return SOCK_CANNOT_OPEN;

    // bind the socket to the given address
    if (::bind(_fd, (struct sockaddr*) &s_addr, sizeof(s_addr)) < 0)
    {
        close(_fd);
        return SOCK_CANNOT_BIND;
    }

    *fd = _fd;
}

void hbc::socket_server::start()
{
    // listen on the socket for client connections
    listen(fd, 2);

    // accept all incoming client connections
    while (accept_socket())
    {
        // do nothing
    }
}

/**
 * Accept the connection and then fork the reading to a new thread
 * so we can accept more clients.
 *
 * @return If we are still on the server thread then return true
 * to keep listening for more connections. Otherwise return false
 * since the client pair is done reading.
 */
int hbc::socket_server::accept_socket()
{
    struct sockaddr_in c_addr;
    int c_addr_len = sizeof(c_addr);
    int c_fd = accept(fd, (struct sockaddr*) &c_addr, (socklen_t*) &c_addr_len);

    if (c_fd < 0) throw "accept";

    if (!create_pair)
    {
        fprintf(stderr, "No pairing method available, closing connection!\n");

        // there is not a create pair callback
        // so just close the connection now
        close(c_fd);
    }
    else
    {
        socket_pair* socket = create_pair(this, c_fd);

        if (!socket)
        {
            close(c_fd);
            throw "create pair";
        }

        pthread_t pthread;
        pthread_create(&pthread, 0, &hbc::start_pairing, socket);
    }

    return 1;
}

void* hbc::start_pairing(void* cookie)
{
    socket_pair* socket = (socket_pair*) cookie;
    socket->start();
    delete socket;
    return 0;
}
