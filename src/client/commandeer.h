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

#ifndef CMDR_CLIENT_H
#define CMDR_CLIENT_H

#include "socket/socket_client.h"

#include <stdio.h>

namespace cmdr
{

void* start_piping_client(void* cookie);

void write(int fd, char key, const unsigned char* payload, int length);

int acknak_client(hbc::socket_packet* packet, void* cookie);
void* socket_start(void* cookie);

class cmdr_client
{
    friend void* start_piping_client(void* cookie);
    friend int acknak_client(hbc::socket_packet* packet, void* cookie);

public:

    cmdr_client(const char* hostname, int port, const char* pin, const char* connect_to_pin);
    virtual ~cmdr_client();

    void start();

private:

    hbc::socket_client* socket;
    const char* pin;
    const char* partner_pin;
    int data_channel;
};

}

#endif
