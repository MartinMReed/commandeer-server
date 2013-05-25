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

#include "commandeer.h"
#include "../cmdr_util.h"

#include "math/_math.h"

#include <iostream>

cmdr::commandeer::commandeer(int port)
{
    server = new socket_server(this, port);
    server->create_pair = &cmdr::create_pair;
}

cmdr::commandeer::~commandeer()
{
    delete server;
}

void cmdr::commandeer::start()
{
    server->bind();
    server->start();
}

cmdr::socket_server::socket_server(commandeer* cmdr, int port)
        : hbc::socket_server(port)
{
    this->cmdr = cmdr;
    connection_mngr = new connection_manager();
}

cmdr::socket_server::~socket_server()
{
    delete connection_mngr;
}

hbc::socket_pair* cmdr::create_pair(hbc::socket_server* server, int fd)
{
    socket_server* cmdr_server = (socket_server*) server;
    socket_pair* socket = new socket_pair(cmdr_server, fd);

    connection_manager* connection_mngr = cmdr_server->connection_mngr;
    connection_mngr->insert(socket);

    return socket;
}

cmdr::socket_pair::socket_pair(socket_server* server, int fd)
        : hbc::socket_pair(server, fd)
{
    acknak = &cmdr::acknak;
    acknak_cookie = this;

    user = 0;
}

cmdr::socket_pair::~socket_pair()
{
    socket_server* server = (socket_server*) this->server;
    connection_manager* connection_mngr = server->connection_mngr;
    connection_mngr->erase(this);

    if (user)
    {
        user->close();
        delete user;
    }
}

void cmdr::socket_pair::write(char key, const unsigned char* payload, int length)
{
    int packet_len = length + 1;

    unsigned char buffer[packet_len];
    buffer[0] = key;

    if (length > 0)
    {
        memcpy(&buffer[1], payload, length);
    }

    hbc::socket_pair::write(buffer, packet_len);
}

int cmdr::acknak(hbc::socket_packet* packet, void* cookie)
{
    int key = packet->payload[0];

    int ack = 1;

    if (key == cmdr_mapping)
    {
        ack = acknak_cmdr_mapping(packet);
    }
    else if (key == cmdr_connect_to)
    {
        ack = acknak_cmdr_connect_to(packet);
    }
    else if (key == cmdr_partner_pipe)
    {
        ack = acknak_cmdr_partner_pipe(packet);
    }

    hbc::free_packet(packet);

    return ack;
}

int cmdr::acknak_cmdr_mapping(hbc::socket_packet* packet)
{
    socket_pair* socket = (socket_pair*) packet->sock;

    unsigned char* value = packet->length > 1 ? &packet->payload[1] : 0;

    int ack = 1;

    connection_type type = (connection_type) value[0];
    fprintf(stdout, "[%d] Identified as %s(%d) channel\n",
            socket->fd, connection_type_str(type), type);

    int pin_len = (connection_type) value[1];
    const char* _value = reinterpret_cast<const char*>(&value[2]);
    std::string pin = std::string(_value, pin_len);

    socket_server* server = (socket_server*) socket->server;
    connection_manager* connection_mngr = server->connection_mngr;

    connection_user* user = connection_mngr->find_mapped(pin, 0);

    socket_pair* ack_socket = socket;
    unsigned char response[2] = { type, 1 };

    if (type == command)
    {
        if (user)
        {
            int fd = user->command->fd;
            fprintf(stdout, "[%d] Removing previous \"%s\"\n", fd, pin.c_str());

            connection_mngr->erase(user->command);

            user->close();
            delete user;
        }

        connection_mngr->insert(pin, socket);
    }
    else if (user && type == data)
    {
        user->data = socket->fd;
        socket->adopted = true;
        ack_socket = user->command;
        ack = 0;
    }
    else
    {
        fprintf(stderr, "[%d] \"%s\" not found\n", socket->fd, pin.c_str());

        response[1] = 0;
    }

    ack_socket->write(cmdr_mapping_r, response, 2);

    return ack;
}

int cmdr::acknak_cmdr_connect_to(hbc::socket_packet* packet)
{
    socket_pair* socket = (socket_pair*) packet->sock;

    unsigned char* value = packet->length > 1 ? &packet->payload[1] : 0;
    int value_len = packet->length - 1;

    const char* _value = reinterpret_cast<const char*>(value);
    std::string pin = std::string(_value, value_len);
    socket->partner_id = pin;

    socket_server* server = (socket_server*) socket->server;
    connection_manager* connection_mngr = server->connection_mngr;

    unsigned char response;

    connection_user* partner_user = connection_mngr->find_mapped(pin, 0);
    if (partner_user && partner_user->data)
    {
        socket_pair* partner_command = partner_user->command;
        connection_user* user = socket->user;

        start_piping(partner_user, user);

        fprintf(stdout, "[%d/%d] Partners connected\n", socket->fd, partner_command->fd);

        response = cmdr::connected;
        partner_command->write(cmdr_connect_to_r, &response, 1);
    }
    else if (partner_user)
    {
        fprintf(stdout, "[%d] Cannot connect to partner: data channel not open\n", socket->fd);

        response = cmdr::failed;
    }
    else
    {
        fprintf(stdout, "[%d] Partner not yet connected\n", socket->fd);

        response = cmdr::waiting_for_partner;
    }

    socket->write(cmdr_connect_to_r, &response, 1);

    return 1;
}

int cmdr::acknak_cmdr_partner_pipe(hbc::socket_packet* packet)
{
    socket_pair* socket = (socket_pair*) packet->sock;

    if (socket->partner_id.size())
    {
        socket_server* server = (socket_server*) socket->server;
        connection_manager* connection_mngr = server->connection_mngr;
        connection_user* partner_user = connection_mngr->find_mapped(socket->partner_id, 0);

        if (partner_user)
        {
            socket_pair* partner_command = partner_user->command;
            partner_command->hbc::socket_pair::write(&packet->payload[1], packet->length - 1);
        }
    }

    return 1;
}

void cmdr::start_piping(connection_user* user1, connection_user* user2)
{
    pipe_group_shared* shared = new pipe_group_shared();
    shared->user1 = user1;
    shared->user2 = user2;

    pipe_group* group1 = new pipe_group();
    group1->read_user = user1;
    group1->write_user = user2;
    group1->shared = shared;
    shared->group1 = group1;

    pipe_group* group2 = new pipe_group();
    group2->read_user = user2;
    group2->write_user = user1;
    group2->shared = shared;
    shared->group2 = group2;

    // osx to device
    pthread_create(&group1->read_thread, 0, &cmdr::pipe_group_read, group1);
    pthread_create(&group1->write_thread, 0, &cmdr::pipe_group_write, group1);

    // device to osx
    pthread_create(&group2->read_thread, 0, &cmdr::pipe_group_read, group2);
    pthread_create(&group2->write_thread, 0, &cmdr::pipe_group_write, group2);
}

void cmdr::notify_partner_disconnected(connection_user* user)
{
    socket_pair* socket = user->command;
    if (hbc::is_open(user->data) && hbc::is_open(socket->fd))
    {
        socket->write(cmdr_partner_disconnected, 0, 0);
    }
}

void* cmdr::pipe_group_read(void* cookie)
{
    pipe_group* group = (pipe_group*) cookie;
    connection_user* r_user = group->read_user;
    connection_user* w_user = group->write_user;
    hbc::circular_buffer* buffer = group->buffer;

    int blen = 1024 * 64;
    unsigned char b[blen];

    int r_cmd = r_user->command->fd;
    int r_data = r_user->data;

    int w_cmd = w_user->command->fd;
    int w_data = w_user->data;

    bps_tracker* bps = group->read_bps;
    bps->start();

    do
    {
        int r = ::read(r_data, b, blen * sizeof(unsigned char));
        if (r <= 0)
        {
            r_user->close(cmdr::data);
            break;
        }

        bps->update(r);

        pthread_mutex_lock(&group->buffer_mutex);

        if (r <= buffer->available())
        {
            buffer->write(b, r);
        }
        else
        {
            int o = 0;
            do
            {
                int available;
                while ((available = buffer->available()) == 0)
                {
                    pthread_cond_signal(&group->read_cond);
                    pthread_cond_wait(&group->write_cond, &group->buffer_mutex);
                }

                int len = std::min(r, buffer->available());
                o += buffer->write(b, len);
            }
            while (o < r);
        }

        group->shared->print_bps();

        pthread_cond_signal(&group->read_cond);
        pthread_mutex_unlock(&group->buffer_mutex);
    }
    while (hbc::is_open(r_data) && hbc::is_open(w_data));

    pthread_cond_signal(&group->read_cond);
    pthread_join(group->write_thread, NULL);

    // there are four threads, we only want to do this once
    if (r_user == group->shared->user1)
    {
        fprintf(stdout, "channels... r_cmd[%d/%s], r_data[%d/%s], w_cmd[%d/%s], w_data[%d/%s]\n",
                r_cmd, hbc::is_open(r_cmd) ? "o" : "x",
                r_data, hbc::is_open(r_data) ? "o" : "x",
                w_cmd, hbc::is_open(w_cmd) ? "o" : "x",
                w_data, hbc::is_open(w_data) ? "o" : "x");

        notify_partner_disconnected(r_user);
        notify_partner_disconnected(w_user);

        delete group->shared;
    }

    delete group;

    return 0;
}

void* cmdr::pipe_group_write(void* cookie)
{
    pipe_group* group = (pipe_group*) cookie;
    connection_user* r_user = group->read_user;
    connection_user* w_user = group->write_user;
    hbc::circular_buffer* buffer = group->buffer;

    int blen = 4096;
    unsigned char b[blen];

    int r_data = r_user->data;
    int w_data = w_user->data;

    bps_tracker* bps = group->write_bps;
    bps->start();

    do
    {
        pthread_mutex_lock(&group->buffer_mutex);

        if (buffer->empty())
        {
            pthread_cond_signal(&group->write_cond);
            pthread_cond_wait(&group->read_cond, &group->buffer_mutex);
            pthread_mutex_unlock(&group->buffer_mutex);
            continue;
        }

//        if (buffer->size() > 64000)
//        {
//            erase_all_gops(buffer, "write");
//        }

        group->shared->print_bps();

        int r = buffer->read(b, blen);

        pthread_cond_signal(&group->write_cond);
        pthread_mutex_unlock(&group->buffer_mutex);

        int o = 0;
        do
        {
            int w = hbc::write_all(w_data, &b[o], r - o);
            if (w < 0)
            {
                w_user->close(cmdr::data);
                break;
            }
            o += w;
        }
        while (o < r);

        bps->update(r);
    }
    while (hbc::is_open(r_data) && hbc::is_open(w_data));

    pthread_cond_signal(&group->write_cond);

    return 0;
}
