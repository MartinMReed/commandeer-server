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

#ifndef HBC_CIRCULAR_BUFFER_H
#define HBC_CIRCULAR_BUFFER_H

#include <sys/types.h>

namespace hbc
{

class circular_buffer
{
public:

    circular_buffer(int size);
    virtual ~circular_buffer();

    int peek(unsigned char* buf, int offset, int size);
    int peek(unsigned char* buf, int size);

    int read(unsigned char* buf, int size);
    int read(unsigned char* buf, int offset, int len);

    int write(unsigned char* buf, int size);

    int find(unsigned char* buf, int offset, int len);
    int find(unsigned char* buf, int len);

    int erase(int offset, int len);

    int size();
    int capacity();
    int available();
    bool empty();

private:

    int read(unsigned char* buf, int offset, int len, bool reduce);

    unsigned char* m_buffer;
    int m_capacity, m_size, m_start;
};

}

#endif
