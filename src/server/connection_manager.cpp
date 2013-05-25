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

#include "connection_manager.h"

#include "commandeer.h"
#include "../cmdr_util.h"
#include "time/_time.h"

cmdr::connection_user::connection_user()
{
    command = 0;
    data = 0;
}

void cmdr::connection_user::close()
{
    close(cmdr::command);
    close(cmdr::data);
}

void cmdr::connection_user::close(connection_type type)
{
    if (type == cmdr::data && data)
    {
        ::close(data);
        data = 0;
    }
    else if (type == cmdr::command)
    {
        command->disconnect();
    }
}

cmdr::pipe_group::pipe_group()
{
    read_user = 0;
    write_user = 0;

    // a gop is about 60-64kb, and half a second
    // 60000 * 2 * 5 = 600000 (5 seconds)
    // on wifi the buffer is usually holding 1.41kb
    buffer = new hbc::circular_buffer(600000);

    read_bps = new bps_tracker();
    write_bps = new bps_tracker();

    pthread_mutex_init(&buffer_mutex, 0);
    pthread_cond_init(&read_cond, 0);
    pthread_cond_init(&write_cond, 0);
}

cmdr::pipe_group::~pipe_group()
{
    delete buffer;
    delete read_bps;
    delete write_bps;

    pthread_mutex_destroy(&buffer_mutex);
    pthread_cond_destroy(&read_cond);
    pthread_cond_destroy(&write_cond);
}

cmdr::pipe_group_shared::pipe_group_shared()
{
    user1 = 0;
    user2 = 0;

    group1 = 0;
    group2 = 0;

    pthread_mutex_init(&mutex, 0);
}

cmdr::pipe_group_shared::~pipe_group_shared()
{
    pthread_mutex_destroy(&mutex);
}

void cmdr::pipe_group_shared::print_bps()
{
    int locked = pthread_mutex_lock(&mutex);

    long timestamp = hbc::current_time_millis();
    static long last_timestamp = timestamp;

    if (timestamp - last_timestamp < 1000)
    {
        if (locked == 0) pthread_mutex_unlock(&mutex);
        return;
    }

    last_timestamp = timestamp;

    double r1;
    bps_type r1_typ = group1->read_bps->get_bps(&r1);

    double w1;
    bps_type w1_typ = group2->write_bps->get_bps(&w1);

    double r2;
    bps_type r2_typ = group2->read_bps->get_bps(&r2);

    double w2;
    bps_type w2_typ = group1->write_bps->get_bps(&w2);

    double bytes1 = group1->buffer->size();
    bps_type bytes1_typ = get_bps_type(bytes1, &bytes1);

    double bytes2 = group2->buffer->size();
    bps_type bytes2_typ = get_bps_type(bytes2, &bytes2);

    if (locked == 0) pthread_mutex_unlock(&mutex);

    fprintf(stdout, "transfer rate: user1[Tx:%.2f%s, Rx:%.2f%s, B:%.2f%s], user2[Tx:%.2f%s, Rx:%.2f%s, B:%.2f%s]\n",
            r1_typ == none ? 0 : r1, bps_type_str(r1_typ),
            w1_typ == none ? 0 : w1, bps_type_str(w1_typ),
            bytes1, b_type_str(bytes1_typ),
            r2_typ == none ? 0 : r2, bps_type_str(r2_typ),
            w2_typ == none ? 0 : w2, bps_type_str(w2_typ),
            bytes2, b_type_str(bytes2_typ));
}

cmdr::connection_manager::~connection_manager()
{
    disconnect_all();
}

void cmdr::connection_manager::disconnect_all()
{
    std::vector<socket_pair*> unmapped_pairs(this->unmapped_pairs);
    this->unmapped_pairs.clear();

    std::map<std::string, connection_user*> mapped_pairs(this->mapped_pairs);
    this->mapped_pairs.clear();

    for (mapped_entry i = mapped_pairs.begin(); i != mapped_pairs.end(); i++)
    {
        std::string id = i->first;
        connection_user* user = i->second;
        int fd = user->command->fd;
        user->close();
        delete user;

        fprintf(stdout, "[%d] Disconnected \"%s\"\n", fd, id.c_str());
    }

    for (unmapped_entry i = unmapped_pairs.begin(); i != unmapped_pairs.end(); i++)
    {
        socket_pair* socket = *i;
        int fd = socket->fd;
        socket->disconnect();
        fprintf(stdout, "[%d] Disconnected\n", fd);
    }
}

bool cmdr::connection_manager::erase(socket_pair* socket)
{
    connection_user* user = socket->user;
    if (user)
    {
        std::string id = user->id;

        mapped_entry i;
        if (find_mapped(id, &i))
        {
            mapped_pairs.erase(i);
            socket_pair* command = user->command;
            fprintf(stdout, "[%d] Erased \"%s\"\n", command->fd, id.c_str());
            return true;
        }
    }

    unmapped_entry i;
    if (find_unmapped(socket, &i))
    {
        unmapped_pairs.erase(i);
        fprintf(stdout, "[%d] Erased (%sadopted)\n",
                socket->fd, socket->adopted ? "" : "un");
        return true;
    }

    return false;
}

cmdr::connection_user* cmdr::connection_manager::find_mapped(std::string id, mapped_entry* entry)
{
    mapped_entry i = mapped_pairs.find(id);
    if (i == mapped_pairs.end()) return 0;
    if (entry) *entry = i;
    return i->second;
}

cmdr::socket_pair* cmdr::connection_manager::find_unmapped(socket_pair* socket,
        unmapped_entry* entry)
{
    for (unmapped_entry i = unmapped_pairs.begin(); i != unmapped_pairs.end(); i++)
    {
        if (*i != socket) continue;
        if (entry) *entry = i;
        return *i;
    }
    return 0;
}

void cmdr::connection_manager::insert(socket_pair* socket)
{
    unmapped_pairs.push_back(socket);
    fprintf(stdout, "[%d] Added\n", socket->fd);
}

void cmdr::connection_manager::insert(std::string id, socket_pair* socket)
{
    connection_user* user = new connection_user();
    user->id = id;
    user->command = socket;

    socket->user = user;

    mapped_pairs.insert(std::pair<std::string, connection_user*>(id, user));

    unmapped_entry unmapped;
    if (find_unmapped(socket, &unmapped)) unmapped_pairs.erase(unmapped);

    fprintf(stdout, "[%d] Mapped \"%s\"\n", socket->fd, id.c_str());
}
