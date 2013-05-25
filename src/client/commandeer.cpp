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

#include "packet_keys.h"

#include <netdb.h>

#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>

#include <string.h>
#include <iostream>

cmdr::cmdr_client::cmdr_client(const char* hostname, int port,
        const char* pin, const char* partner_pin)
{
    socket = new hbc::socket_client(hostname, port);
    socket->acknak = &cmdr::acknak_client;
    socket->acknak_cookie = this;

    this->pin = pin;
    this->partner_pin = partner_pin;
    data_channel = 0;
}

cmdr::cmdr_client::~cmdr_client()
{
    if (socket) delete socket;
    if (data_channel) ::close(data_channel);
}

void cmdr::cmdr_client::start()
{
    socket->connect();

    pthread_t pthread;
    pthread_create(&pthread, 0, &cmdr::socket_start, socket);

    fprintf(stdout, "[%d] Mapping command channel\n", socket->fd);
    std::string mapping(strlen(pin) + 2, 0);
    mapping[0] = (int) command;
    mapping[1] = (int) strlen(pin);
    memcpy(&mapping[2], pin, strlen(pin));
    cmdr::write(socket->fd, cmdr_mapping, (unsigned char*) mapping.c_str(), mapping.size());

    pthread_join(pthread, NULL);
}

void cmdr::write(int fd, char key, const unsigned char* payload, int length)
{
    int burst_length = length + 1;

    unsigned char buffer[burst_length];
    buffer[0] = key;

    if (length > 0)
    {
        memcpy(&buffer[1], payload, length);
    }

    hbc::write(fd, buffer, burst_length);
}

void* cmdr::socket_start(void* cookie)
{
    hbc::socket_client* socket = (hbc::socket_client*) cookie;
    socket->start();
    return 0;
}

int cmdr::acknak_client(hbc::socket_packet* packet, void* cookie)
{
    cmdr_client* client = (cmdr_client*) cookie;
    hbc::socket_client* socket = (hbc::socket_client*) packet->sock;

    int key = packet->payload[0];
    unsigned char* value = packet->length > 1 ? &packet->payload[1] : 0;
    int value_len = packet->length - 1;

    int ack = 1;

    if (key == cmdr_mapping_r)
    {
        connection_type type = (connection_type) value[0];

        fprintf(stdout, "[%d] Mapped as type: %s (%d)\n",
                socket->fd, connection_type_str(type), type);

        if (type == command)
        {
            hbc::connect(socket->hostname, socket->port, &client->data_channel);

            const char* pin = client->pin;

            fprintf(stdout, "[%d] Mapping data channel\n", socket->fd);
            std::string mapping(strlen(pin) + 2, 0);
            mapping[0] = (int) data;
            mapping[1] = (int) strlen(pin);
            memcpy(&mapping[2], pin, strlen(pin));
            cmdr::write(client->data_channel, cmdr_mapping, (unsigned char*) mapping.c_str(),
                    mapping.size());
        }
        else if (type == data)
        {
            fprintf(stdout, "[%d] Connecting to partner: %s\n",
                    socket->fd, client->partner_pin);
            cmdr::write(socket->fd, cmdr_connect_to,
                    (unsigned char*) client->partner_pin, strlen(client->partner_pin));
        }
    }
    else if (key == cmdr_connect_to_r)
    {
        connect_to_pin_response type = (connect_to_pin_response) value[0];
        fprintf(stdout, "[%d] Partner connection response: %s (%d)\n",
                socket->fd, connect_to_pin_str(type), type);

        if (type == connected)
        {
            pthread_t pthread;
            pthread_create(&pthread, 0, &cmdr::start_piping_client, client);
        }
    }
    else if (key == cmdr_partner_disconnected)
    {
        fprintf(stdout, "[%d] Partner disconnected\n", socket->fd);

        ::close(client->data_channel);
        client->data_channel = 0;
    }

    return ack;
}

void* cmdr::start_piping_client(void* cookie)
{
    cmdr_client* client = (cmdr_client*) cookie;
    int fd = client->data_channel;

    FILE* file;

    if (strcmp("2", client->pin) == 0)
    {
        // test code for reading in one at a time
        unsigned char buffer = 0;
        while (hbc::is_open(fd))
        {
            sleep(1);
            fprintf(stdout, "<- %d\n", buffer);
            int w = hbc::write_all(fd, &buffer, 1);
            if (w < 0) break;
            buffer++;
        }

//        file = fopen("in.mpg", "rb");
//        int buffer_len = 2048;
//        unsigned char buffer[buffer_len];
//
//        do
//        {
//            int r = ::fread(buffer, sizeof(unsigned char), buffer_len, file);
//            if (r < 0)
//            {
//                ::close(fd);
//                break;
//            }
//
//            int w = hbc::write_all(fd, buffer, r);
//            if (w < 0)
//            {
//                ::close(fd);
//                break;
//            }
//        }
//        while (hbc::is_open(fd));
    }
    else
    {
        // test code for reading in one at a time
        unsigned char buffer;
        int r;
        while ((r = ::read(fd, &buffer, sizeof(unsigned char))) > 0)
        {
            fprintf(stdout, "-> %d\n", buffer);
        }

//        file = fopen("out.mpg", "wb");
//        int buffer_len = 2048;
//        unsigned char buffer[buffer_len];
//
//        do
//        {
//            int r = ::read(fd, buffer, buffer_len * sizeof(unsigned char));
//            if (r <= 0)
//            {
//                ::close(fd);
//                break;
//            }
//
//            fwrite(buffer, sizeof(unsigned char), r, file);
//        }
//        while (hbc::is_open(fd));
    }

    fclose(file);

    return 0;
}
