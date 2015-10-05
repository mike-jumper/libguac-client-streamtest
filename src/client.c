/*
 * Copyright (C) 2015 Glyptodon LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include "client.h"

#include <guacamole/client.h>

#include <stdlib.h>

/**
 * NULL-terminated array of arguments accepted by this client plugin.
 */
const char* GUAC_CLIENT_ARGS[] = {
    NULL
};

/**
 * The array index of each argument accepted by this client plugin.
 */
enum STREAMTEST_ARGS_IDX {
    STREAMTEST_ARGS_COUNT
};

/**
 * Guacamole client plugin entry point. This function will be called by guacd
 * when the protocol associated with this plugin is selected.
 *
 * @param client
 *     A newly-allocated guac_client structure representing the client which
 *     connected to guacd.
 *
 * @param argc
 *     The number of arguments within the argv array.
 *
 * @param argv
 *     All arguments passed during the Guacamole protocol handshake. These
 *     arguments correspond identically in both order and number to the
 *     arguments listed in GUAC_CLIENT_ARGS.
 */
int guac_client_init(guac_client* client, int argc, char** argv) {

    /* STUB */    
    return 0;

}

