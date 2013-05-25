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

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <map>
#include <vector>
#include <string>
#include <sys/time.h>

#include "packet_keys.h"
#include "collection/circular_buffer.h"

namespace cmdr
{

class socket_pair;
class bps_tracker;
class pipe_group;

class connection_user
{

public:

    connection_user();

    void close();
    void close(connection_type type);

    std::string id;
    socket_pair* command;
    int data;
};

class pipe_group_shared
{

public:

    pipe_group_shared();
    virtual ~pipe_group_shared();

    connection_user* user1;
    connection_user* user2;

    pipe_group* group1;
    pipe_group* group2;

    pthread_mutex_t mutex;

    void print_bps();
};

class pipe_group
{

public:

    pipe_group();
    virtual ~pipe_group();

    pipe_group_shared* shared;

    connection_user* read_user;
    connection_user* write_user;
    hbc::circular_buffer* buffer;

    pthread_t read_thread;
    pthread_t write_thread;

    pthread_mutex_t buffer_mutex;
    pthread_cond_t read_cond;
    pthread_cond_t write_cond;

    bps_tracker* read_bps;
    bps_tracker* write_bps;
};

class connection_manager
{

public:

    typedef std::map<std::string, connection_user*>::iterator mapped_entry;
    typedef std::vector<socket_pair*>::iterator unmapped_entry;

    virtual ~connection_manager();

    connection_user* find_mapped(std::string id, mapped_entry* entry);
    socket_pair* find_unmapped(socket_pair* socket, unmapped_entry* entry);

    void insert(socket_pair* socket);
    void insert(std::string id, socket_pair* socket);

    bool erase(socket_pair* socket);

private:

    void disconnect_all();

    std::map<std::string, connection_user*> mapped_pairs;
    std::vector<socket_pair*> unmapped_pairs;
};

}

#endif
