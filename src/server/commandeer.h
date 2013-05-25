/**
 * Copyright (c) 2012 Martin M Reed
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

#ifndef CMDR_SERVER_H
#define CMDR_SERVER_H

#include "packet_keys.h"
#include "connection_manager.h"
#include "socket/socket_server.h"
#include "bps_tracker.h"

#include <string>
#include <deque>

namespace cmdr
{

class socket_server;
class socket_pair;

void notify_partner_disconnected(connection_user* user);
void start_piping(connection_user* user1, connection_user* user2);
void* pipe_group_read(void* cookie);
void* pipe_group_write(void* cookie);

int acknak(hbc::socket_packet* packet, void* cookie);
int acknak_cmdr_mapping(hbc::socket_packet* packet);
int acknak_cmdr_connect_to(hbc::socket_packet* packet);
int acknak_cmdr_partner_pipe(hbc::socket_packet* packet);

hbc::socket_pair* create_pair(hbc::socket_server* server, int fd);

class commandeer
{

public:

    commandeer(int port);
    virtual ~commandeer();

    void start();

private:

    socket_server* server;
};

class socket_server : public hbc::socket_server
{
    friend class socket_pair;
    friend int acknak(hbc::socket_packet* packet, void* cookie);
    friend int acknak_cmdr_mapping(hbc::socket_packet* packet);
    friend int acknak_cmdr_connect_to(hbc::socket_packet* packet);
    friend int acknak_cmdr_partner_pipe(hbc::socket_packet* packet);
    friend hbc::socket_pair* create_pair(hbc::socket_server* server, int fd);

public:

    socket_server(commandeer* cmdr, int port);
    virtual ~socket_server();

private:

    commandeer* cmdr;
    connection_manager* connection_mngr;
};

class socket_pair : public hbc::socket_pair
{
    friend class connection_manager;
    friend int acknak(hbc::socket_packet* packet, void* cookie);
    friend int acknak_cmdr_mapping(hbc::socket_packet* packet);
    friend int acknak_cmdr_connect_to(hbc::socket_packet* packet);
    friend int acknak_cmdr_partner_pipe(hbc::socket_packet* packet);

public:

    socket_pair(socket_server* client, int fd);
    virtual ~socket_pair();

    void write(char key, const unsigned char* payload, int length);

private:

    connection_user* user;
    std::string partner_id;
};

}

#endif
