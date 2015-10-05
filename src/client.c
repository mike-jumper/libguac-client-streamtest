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
#include <guacamole/protocol.h>
#include <guacamole/socket.h>

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * NULL-terminated array of arguments accepted by this client plugin.
 */
const char* GUAC_CLIENT_ARGS[] = {
    "filename",
    "mimetype",
    "bytes-per-frame",
    "frame-usecs",
    NULL
};

/**
 * The array index of each argument accepted by this client plugin.
 */
enum STREAMTEST_ARGS_IDX {

    /**
     * The index of the argument containing the filename of the media file to
     * be streamed.
     */
    IDX_FILENAME,

    /**
     * The index of the argument containing the mimetype of the media file to
     * be streamed.
     */
    IDX_MIMETYPE,

    /**
     * The index of the argument containing the number of bytes to stream from
     * the provided file (per frame).
     */
    IDX_BYTES_PER_FRAME,

    /**
     * The index of the argument containing the duration of each frame in
     * microseconds.
     */
    IDX_FRAME_USECS,

    /**
     * The number of arguments that should be given to guac_client_init. If
     * argc does not contain this value, something has gone horribly wrong.
     */
    STREAMTEST_ARGS_COUNT

};

/**
 * Handler which will be invoked when the data associated with the given
 * guac_client needs to be freed.
 *
 * @param client
 *     The guac_client whose associated data should be freed.
 *
 * @return
 *     Non-zero if an error occurs while freeing data associated with the
 *     given guac_client, zero otherwise.
 */
static int streamtest_client_free_handler(guac_client* client) {

    /* Get stream state from client */
    streamtest_state* state = (streamtest_state*) client->data;

    /* Close file being streamed */
    close(state->fd);

    /* Free stream */
    guac_client_free_stream(client, state->stream);

    /* Free state itself */
    free(state);

    /* Success */
    return 0;

}

/**
 * Returns an arbitrary timestamp in microseconds. This timestamp is relative
 * only to previous calls of streamtest_utime() and is intended only for the
 * sake of determining relative or elapsed time.
 *
 * @return
 *     An arbitrary timestamp in microseconds.
 */
static int streamtest_utime() {

    struct timespec current;

    /* Get current time */
    clock_gettime(CLOCK_REALTIME, &current);
    
    /* Calculate microseconds */
    return (int) (current.tv_sec * 1000000 + current.tv_nsec / 1000);

}

/**
 * Suspends execution for the given number of microseconds.
 *
 * @param usecs
 *     The number of microseconds to sleep for.
 */
static void streamtest_usleep(int usecs) {

    /* Calculate sleep duration in seconds and nanoseconds */
    struct timespec sleep_duration = {
        .tv_sec  =  usecs / 1000000,
        .tv_nsec = (usecs % 1000000) * 1000
    };

    /* Sleep for specified amount of time */
    nanosleep(&sleep_duration, NULL);

}

/**
 * Display a progress bar which indicates the current stream status.
 *
 * @param client
 *     The guac_client associated with the libguac-client-streamtest
 *     connection whose current status should be redrawn.
 */
static void streamtest_render_progress(guac_client* client) {

    /* Get stream state from client */
    streamtest_state* state = (streamtest_state*) client->data;

    /*
     * Render background
     */

    guac_protocol_send_rect(client->socket,
            GUAC_DEFAULT_LAYER,
            0, 0, STREAMTEST_PROGRESS_WIDTH, STREAMTEST_PROGRESS_HEIGHT);

    guac_protocol_send_cfill(client->socket,
            GUAC_COMP_OVER, GUAC_DEFAULT_LAYER,
            0x40, 0x40, 0x40, 0xFF);

    /*
     * Render progress
     */

    guac_protocol_send_rect(client->socket,
            GUAC_DEFAULT_LAYER, 0, 0,
            (state->bytes_read+1)
                * STREAMTEST_PROGRESS_WIDTH
                / (state->file_size+1),
            STREAMTEST_PROGRESS_HEIGHT);

    if (state->paused)
        guac_protocol_send_cfill(client->socket,
                GUAC_COMP_OVER, GUAC_DEFAULT_LAYER,
                0x80, 0x80, 0x00, 0xFF);
    else
        guac_protocol_send_cfill(client->socket,
                GUAC_COMP_OVER, GUAC_DEFAULT_LAYER,
                0x00, 0x80, 0x00, 0xFF);

    guac_socket_flush(client->socket);

}

/**
 * Called periodically by guacd whenever the plugin should handle accumulated
 * data and render a frame.
 *
 * @param client
 *     The guac_client associated with the plugin that should render a frame.
 *
 * @return
 *     Non-zero if an error occurs, zero otherwise.
 */
static int streamtest_client_message_handler(guac_client* client) {

    /* Get stream state from client */
    streamtest_state* state = (streamtest_state*) client->data;

    int frame_start;
    int frame_duration;

    /* Record start of frame */
    frame_start = streamtest_utime();

    /* STUB: Simulate read from stream */
    if (!state->paused) {
        state->bytes_read += state->frame_bytes;
        if (state->bytes_read > state->file_size)
            state->bytes_read = state->file_size;
    }

    /* Update progress bar */
    streamtest_render_progress(client);

    /* Sleep for remainder of frame */
    frame_duration = streamtest_utime() - frame_start;
    if (frame_duration < state->frame_duration)
        streamtest_usleep(state->frame_duration - frame_duration);

    /* Warn (at debug level) if frame takes too long */
    else
        guac_client_log(client, GUAC_LOG_DEBUG,
                "Frame took longer than requested duration: %i microseconds",
                frame_duration);

    /* Success */
    return 0;

}

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

    int fd;
    guac_stream* stream;
    streamtest_state* state;

    /* Validate argument count */
    if (argc != STREAMTEST_ARGS_COUNT) {
        guac_client_log(client, GUAC_LOG_ERROR, "Wrong number of arguments.");
        return 1;
    }

    /* Allocate stream for media */
    stream = guac_client_alloc_stream(client);

    /* Begin audio stream for audio mimetypes */
    if (strncmp(argv[IDX_MIMETYPE], "audio/", 6) == 0) {
        guac_protocol_send_audio(client->socket, stream, argv[IDX_MIMETYPE]);
        guac_client_log(client, GUAC_LOG_DEBUG,
                "Recognized type \"%s\" as audio",
                argv[IDX_MIMETYPE]);
    }

    /* Begin video stream for video mimetypes */
#if 0
    else if (strncmp(argv[IDX_MIMETYPE], "video/", 6) == 0) {
        guac_protocol_send_video(client->socket, stream, argv[IDX_MIMETYPE]);
        guac_client_log(client, GUAC_LOG_DEBUG,
                "Recognized type \"%s\" as video",
                argv[IDX_MIMETYPE]);
    }
#endif

    /* Abort if type cannot be recognized */
    else {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Invalid media type \"%s\" (not audio nor video)",
                argv[IDX_MIMETYPE]);
        return 1;
    }

    /* Attempt to open specified file, abort on error */
    fd = open(argv[IDX_FILENAME], O_RDONLY);
    if (fd == -1) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to open \"%s\": %s",
                argv[IDX_FILENAME], strerror(errno));
        return 1;
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Successfully opened file \"%s\"",
            argv[IDX_FILENAME]);

    /* Allocate state structure */
    state = malloc(sizeof(streamtest_state));

    /* Set frame duration/size */
    state->frame_duration = atoi(argv[IDX_FRAME_USECS]);
    state->frame_bytes    = atoi(argv[IDX_BYTES_PER_FRAME]);

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Frames will last %i microseconds and contain %i bytes",
            state->frame_duration, state->frame_bytes);

    /* Start with the file closed, playback not paused */
    state->stream = stream;
    state->fd = fd;
    state->paused = false;
    state->bytes_read = 0;
    state->file_size = 1000000;

    /* Set client handlers and data */
    client->handle_messages = streamtest_client_message_handler;
    client->free_handler    = streamtest_client_free_handler;
    client->data = state;

    /* Init display */
    guac_protocol_send_size(client->socket, GUAC_DEFAULT_LAYER,
            STREAMTEST_PROGRESS_WIDTH,
            STREAMTEST_PROGRESS_HEIGHT);

    /* Render initial progress bar */
    streamtest_render_progress(client);

    /* Initialization complete */
    return 0;

}

