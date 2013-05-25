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

#include "server/commandeer.h"
#include "client/commandeer.h"
#include "collection/circular_buffer.h"

#define HOST "127.0.0.1"
#define PORT 1031

int main(int argc, const char* argv[])
{
    // if arguments are specified, run in client mode for testing
    if (argc != 1 && argc != 3)
    {
        fprintf(stderr, "Must specify pin. Try running `%s ABC123 XYZ000`\n", argv[0]);
        return 1;
    }

    try
    {
        if (argc == 1)
        {
            // run in server mode
            cmdr::commandeer commandeer(PORT);
            commandeer.start();
        }
        else
        {
            // run in client mode for testing
            cmdr::cmdr_client cmdr_client(HOST, PORT, argv[1], argv[2]);
            cmdr_client.start();
        }

        return 0;
    }
    catch (const char* e)
    {
        fprintf(stderr, "Bad Wolf[%s]\n", e);
        return 2;
    }
}
