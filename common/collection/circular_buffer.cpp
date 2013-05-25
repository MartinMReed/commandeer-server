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

#include "circular_buffer.h"

#include <stdio.h>
#include <algorithm>

hbc::circular_buffer::circular_buffer(int capacity)
{
    m_buffer = (unsigned char*) malloc(capacity * sizeof(unsigned char));
    m_capacity = capacity;
    m_start = 0;
    m_size = 0;
}

hbc::circular_buffer::~circular_buffer()
{
    free(m_buffer);
}

int hbc::circular_buffer::available()
{
    return m_capacity - m_size;
}

int hbc::circular_buffer::capacity()
{
    return m_capacity;
}

int hbc::circular_buffer::size()
{
    return m_size;
}

bool hbc::circular_buffer::empty()
{
    return m_size == 0;
}

int hbc::circular_buffer::find(unsigned char* buf, int len)
{
    return find(buf, 0, len);
}

int hbc::circular_buffer::find(unsigned char* buf, int offset, int len)
{
    if (len > m_size) return -1;

    unsigned char* b = (unsigned char*) malloc(len * sizeof(unsigned char));

    int i;
    int end = m_size - len;
    for (i = offset; i < end; i++)
    {
        int r = peek(b, i, len);
        if (memcmp(b, buf, r * sizeof(unsigned char)) == 0) break;
    }

    free(b);

    return i >= end ? -1 : i;
}

int hbc::circular_buffer::read(unsigned char* buf, int len)
{
    return read(buf, 0, len);
}

int hbc::circular_buffer::read(unsigned char* buf, int offset, int len)
{
    int r = peek(buf, offset, len);
    erase(offset, r);
    return r;
}

int hbc::circular_buffer::peek(unsigned char* buf, int len)
{
    return peek(buf, 0, len);
}

int hbc::circular_buffer::peek(unsigned char* buf, int offset, int len)
{
    if (m_size == 0 || len <= 0 || offset < 0) return 0;
    len = std::min(len, m_size - offset);

    int start = (m_start + offset) % m_capacity;

    int read = 0;
    do
    {
        int r = std::min(m_capacity - start, len - read);
        memcpy(&buf[read], &m_buffer[start], r);
        start = (start + r) % m_capacity;
        read += r;
    }
    while (read < len);

    return read;
}

int hbc::circular_buffer::write(unsigned char* buf, int len)
{
    int end = (m_start + m_size) % m_capacity;

    int o = 0;
    do
    {
        int w = std::min(m_capacity - end, len - o);
        memcpy(&m_buffer[end], &buf[o], w);
        end = (end + w) % m_capacity;
        o += w;
    }
    while (o < len);

    m_size += len;
    int overflow = m_size - m_capacity;

    if (overflow > 0)
    {
        m_start = (m_start + overflow) % m_capacity;
        m_size = m_capacity;
    }

    return o;
}

int hbc::circular_buffer::erase(int offset, int len)
{
    if (m_size == 0 || len <= 0 || offset < 0) return 0;

    // erasing the whole buffer
    if (len == m_size)
    {
//        fprintf(stderr, "#0\n");
        m_start = 0;
        m_size = 0;
        return len;
    }

    // erasing everything at the start of the buffer
    if (offset == 0)
    {
//        fprintf(stderr, "#1\n");
        m_start = (m_start + len) % m_capacity;
        m_size -= len;
        return len;
    }

    int available = m_size - offset;

    // erasing everything at the end of the buffer
    if (len == available)
    {
//        fprintf(stderr, "#2\n");
        m_size -= len;
        return len;
    }

    len = std::min(len, available);
    int remaining = available - len;

    // erasing in one pass, move remaining left
    if (m_start + m_size <= m_capacity)
    {
//        fprintf(stderr, "#3\n");
        int start = m_start + offset;
        int end = start + len;
        memmove(&m_buffer[start], &m_buffer[end], remaining * sizeof(unsigned char));
        m_size -= len;
        return len;
    }

    int r1_start = (m_start + offset) % m_capacity;
    int r1 = std::min(m_capacity - r1_start, len);

    // erasing in one pass, move remaining right
    if (r1 == len)
    {
//        fprintf(stderr, "#4\n");
        int end = (r1_start + len) - offset;
        memmove(&m_buffer[end], &m_buffer[m_start], offset * sizeof(unsigned char));
        m_start = (m_start + len) % m_capacity;
        m_size -= len;
        return len;
    }

    // by now we know r2_start < r1_start, because we didn't
    // get caught in the one pass scope

    int r2 = len - r1;
    int r2_end = r2;

    // the beginning can fit at the start of the linear buffer
    if (offset <= r2)
    {
//        fprintf(stderr, "#5\n");
        int start = r2_end - offset;
        memmove(&m_buffer[start], &m_buffer[m_start], offset * sizeof(unsigned char));
        m_start = start;
        m_size -= len;
        return len;
    }

    // the remaining can fit at the end of the linear buffer
    if (remaining <= r1)
    {
//        fprintf(stderr, "#6\n");
        memmove(&m_buffer[r1_start], &m_buffer[r2_end], remaining * sizeof(unsigned char));
        m_size -= len;
        return len;
    }

    // the beginning has less to move. fill the start of the
    // linear buffer and move the rest to the end
    if (offset <= remaining)
    {
//        fprintf(stderr, "#7\n");
        int leftover = offset - r2;
        memmove(m_buffer, &m_buffer[m_start + leftover], r2 * sizeof(unsigned char));
        int start = m_capacity - leftover;
        memmove(&m_buffer[start], &m_buffer[m_start], leftover * sizeof(unsigned char));
        m_start = start;
        m_size -= len;
        return len;
    }

//    fprintf(stderr, "#8\n");
    // the remaining has less to move. fill the end of the
    // linear buffer and move the rest to the start
    int leftover = remaining - r1;
    memmove(&m_buffer[r1_start], &m_buffer[r2_end], r1 * sizeof(unsigned char));
    memmove(m_buffer, &m_buffer[r2_end + r1], leftover * sizeof(unsigned char));
    m_size -= len;
    return len;
}

//void p(hbc::circular_buffer* c, const char* expected)
//{
//    unsigned char b[5];
//    c->peek(b, 0, c->size());
//    fprintf(stderr, "size[%d], actual[", c->size());
//    for (int i = 0; i < c->size(); i++)
//    {
//        fprintf(stderr, "%d", b[i]);
//    }
//    fprintf(stderr, "], expected[%s]\n", expected);
//}
//
//{
//    hbc::circular_buffer c(5);
//    fprintf(stderr, "size[%d], cap[%d]\n", c.size(), c.capacity());
//
//    unsigned char w[5] = { 0, 1, 2, 3, 4 };
//
//    c.write(w, 5);
//    p(&c, "01234");
//
//    w[0] = 5;
//    c.write(w, 1);
//    p(&c, "12345");
//
//    w[0] = 6;
//    w[1] = 7;
//    w[2] = 8;
//    w[3] = 9;
//    w[4] = 0;
//    c.write(w, 5);
//    p(&c, "67890");
//
//    c.erase(0, 1); // #1
//    p(&c, "7890");
//
//    c.erase(0, 4); // #0
//    p(&c, "");
//
//    w[0] = 9;
//    w[1] = 8;
//    w[2] = 7;
//    w[3] = 6;
//    w[4] = 5;
//    c.write(w, 5);
//    p(&c, "98765");
//
//    c.erase(1, 3); // #3
//    p(&c, "95");
//
//    w[0] = 0;
//    w[1] = 1;
//    w[2] = 2;
//    w[3] = 3;
//    w[4] = 4;
//    c.write(w, 5);
//    p(&c, "01234");
//
//    c.erase(1, 4); // #2
//    p(&c, "0");
//
//    w[0] = 0;
//    w[1] = 1;
//    w[2] = 2;
//    w[3] = 3;
//    w[4] = 4;
//    c.write(w, 5);
//    p(&c, "01234");
//
//    c.erase(4, 1); // #2
//    p(&c, "0123");
//
//    c.erase(0, c.size()); // #0
//    p(&c, "");
//
//    w[0] = 8;
//    w[1] = 9;
//    w[2] = 0;
//    w[3] = 1;
//    w[4] = 2;
//    c.write(w, 5);
//    w[0] = 3;
//    w[1] = 4;
//    c.write(w, 2);
//    p(&c, "01234");
//
//    c.erase(1, 1); // #4
//    p(&c, "0234");
//
//    w[0] = 5;
//    c.write(w, 1);
//    p(&c, "02345");
//
//    c.erase(0, c.size()); // #0
//    p(&c, "");
//
//    w[0] = 8;
//    w[1] = 9;
//    w[2] = 0;
//    w[3] = 1;
//    w[4] = 2; // 89012
//    c.write(w, 5);
//    w[0] = 3;
//    w[1] = 4; // 34012
//    c.write(w, 2);
//    p(&c, "01234");
//
//    c.erase(1, 3); // #5
//    p(&c, "04");
//
//    c.erase(0, c.size()); // #0
//    p(&c, "");
//
//    w[0] = 8;
//    w[1] = 9;
//    w[2] = 0;
//    w[3] = 1;
//    w[4] = 2; // 89012
//    c.write(w, 5);
//    w[0] = 3;
//    w[1] = 4; // 34012
//    c.write(w, 2);
//    p(&c, "01234");
//
//    c.erase(2, 2); // #6
//    p(&c, "014");
//
//    c.erase(0, c.size()); // #0
//    p(&c, "");
//}
//
//{
//    hbc::circular_buffer c(10);
//    fprintf(stderr, "size[%d], cap[%d]\n", c.size(), c.capacity());
//
//    unsigned char w[10] = { 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 };
//
//    c.write(w, 10);
//    w[0] = 5;
//    w[1] = 6;
//    w[2] = 7;
//    w[3] = 8;
//    w[4] = 9; // 5678901234
//    c.write(w, 5);
//    p(&c, "0123456789");
//
//    c.erase(2, 4); // #7
//    p(&c, "016789");
//}
//
//{
//    hbc::circular_buffer c(10);
//    fprintf(stderr, "size[%d], cap[%d]\n", c.size(), c.capacity());
//
//    unsigned char w[10] = { 5, 6, 7, 8, 9, 0, 1, 2, 3, 4 };
//
//    c.write(w, 10);
//    w[0] = 5;
//    w[1] = 6;
//    w[2] = 7;
//    w[3] = 8;
//    w[4] = 9; // 5678901234
//    c.write(w, 5);
//    p(&c, "0123456789");
//
//    c.erase(4, 4); // #8
//    p(&c, "012389");
//}
