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

#include <strings.h>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>

hbc::socket_pair::socket_pair(socket_server* server, int fd)
{
    this->server = server;
    this->fd = fd;
}

void hbc::socket_pair::start()
{
    while (read())
    {
        // do nothing
    }
}
