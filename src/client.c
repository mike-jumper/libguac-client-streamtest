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
 * Handler which will be invoked when a key event is received along the socket
 * associated with the given guac_client.
 *
 * @param client
 *     The guac_client associated with the received key event.
 *
 * @param keysym
 *     The X11 keysym of the key that was pressed or released.
 *
 * @param pressed
 *     Non-zero of the given key was pressed, zero if the key was released.
 *
 * @return
 *     Non-zero if an error occurs while handling th ekey event, zero
 *     otherwise.
 */
static int streamtest_client_key_handler(guac_client* client, int keysym,
        int pressed) {

    /* Get stream state from client */
    streamtest_state* state = (streamtest_state*) client->data;

    /* Toggle paused state when space is pressed */
    if (pressed && keysym == 0x20)
        state->paused = !state->paused;

    /* Success */
    return 0;

}


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
 * Writes the given buffer as a set of blob instructions to the given socket.
 * The buffer will be split into as many blob instructions as necessary.
 *
 * @param socket
 *     The guac_socket over which the blob instructions should be sent.
 *
 * @param stream
 *     The stream to associate with each blob.
 *
 * @param buffer
 *     The buffer containing the data that should be sent over the given
 *     guac_socket as blobs.
 *
 * @param length
 *     The number of bytes within the given buffer.
 */
static void streamtest_write_blobs(guac_socket* socket, guac_stream* stream,
        unsigned char* buffer, int length) {

    /* Flush all data in buffer as blobs */
    while (length > 0) {

        /* Determine size of blob to be written */
        int chunk_size = length;
        if (chunk_size > 6048)
            chunk_size = 6048;

        /* Send audio data */
        guac_protocol_send_blob(socket, stream, buffer, chunk_size);

        /* Advance to next blob */
        buffer += chunk_size;
        length -= chunk_size;

    }

}

/**
 * Attempts to fill the given buffer with bytes from the provided file
 * descriptor, returning the number of bytes successfully read. Multiple read
 * attempts will be made automatically until the entire buffer is full, end-of-
 * file is encountered, or an error occurs.
 *
 * @param fd
 *     The file descriptor to read data from.
 *
 * @param buffer
 *     The buffer into which data should be read.
 *
 * @param length
 *     The number of bytes to store within the given buffer.
 *
 * @return
 *     The number of bytes read. This will ALWAYS be the size of the buffer
 *     unless end-of-file is encountered or an error occurs. If end-of-file is
 *     reached, zero is returned. If an error occurs, -1 is returned, and errno
 *     is set appropriately.
 */
static int streamtest_fill_buffer(int fd, unsigned char* buffer, int length) {

    int bytes_read = 0;

    /* Continue reading until buffer is full */
    while (length > 0) {

        /* Attempt to fill remaining space in buffer */
        int result = read(fd, buffer, length);

        /* Stop if end-of-file is reached */
        if (result == 0)
            return bytes_read;

        /* Abort on error */
        if (result == -1)
            return -1;

        /* Advance to next block of data (if any) */
        bytes_read += result;
        buffer += result;
        length -= result;

    }

    return bytes_read;

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

    /* Get current position within file */
    int position = lseek(state->fd, 0, SEEK_CUR);
    if (position == -1) {
        guac_client_log(client, GUAC_LOG_WARNING,
                "Unable to determine current position in stream: %s",
                strerror(errno));
        return;
    }

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
            (position+1) / ((state->file_size+1) / STREAMTEST_PROGRESS_WIDTH),
            STREAMTEST_PROGRESS_HEIGHT);

    if (state->paused)
        guac_protocol_send_cfill(client->socket,
                GUAC_COMP_OVER, GUAC_DEFAULT_LAYER,
                0x80, 0x80, 0x00, 0xFF);
    else
        guac_protocol_send_cfill(client->socket,
                GUAC_COMP_OVER, GUAC_DEFAULT_LAYER,
                0x00, 0x80, 0x00, 0xFF);

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

    /* Read from stream and write as blob(s) */
    if (!state->paused) {

        /* Attempt to fill the available buffer space */
        int length = streamtest_fill_buffer(state->fd,
                state->frame_buffer, state->frame_bytes);

        /* Abort connection if we cannot read */
        if (length == -1) {
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Unable to read from specified file: %s",
                    strerror(errno));
            return 1;
        }

        /* Write all data read as blobs */
        if (length > 0)
            streamtest_write_blobs(client->socket, state->stream,
                    state->frame_buffer, length);

        /* Disconnect on EOF */
        else {
            guac_client_log(client, GUAC_LOG_INFO, "Media streaming complete");
            guac_client_stop(client);
        }

    }

    /* Update progress bar */
    streamtest_render_progress(client);
    guac_socket_flush(client->socket);

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
 * Returns the size of the file associated with the given file descriptor, in
 * bytes. If an error occurs, -1 is returned, and errno is set appropriately.
 *
 * @param fd
 *     The file descriptor assocaited with the file whose size should be
 *     returned.
 *
 * @return
 *     The size of the file assocaited with the given file descriptor, in
 *     bytes, or -1 if an error occurs.
 */
static int streamtest_get_file_size(int fd) {

    struct stat stat_buf;

    /* Attempt to read file stats */
    int result = fstat(fd, &stat_buf);
    if (result != 0)
        return -1;

    /* Return file size only */
    return stat_buf.st_size;

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

    /* Validate argument count */
    if (argc != STREAMTEST_ARGS_COUNT) {
        guac_client_log(client, GUAC_LOG_ERROR, "Wrong number of arguments.");
        return 1;
    }

    /* Allocate stream for media */
    guac_stream* stream = guac_client_alloc_stream(client);

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
    int fd = open(argv[IDX_FILENAME], O_RDONLY);
    if (fd == -1) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to open \"%s\": %s",
                argv[IDX_FILENAME], strerror(errno));
        return 1;
    }

    int file_size = streamtest_get_file_size(fd);
    if (file_size == -1) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to determine size of file \"%s\": %s",
                argv[IDX_FILENAME], strerror(errno));
        return 1;
    }

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Successfully opened file \"%s\" (%i bytes)",
            argv[IDX_FILENAME], file_size);

    /* Allocate state structure */
    streamtest_state* state = malloc(sizeof(streamtest_state));

    /* Set frame duration/size */
    state->frame_duration = atoi(argv[IDX_FRAME_USECS]);
    state->frame_bytes    = atoi(argv[IDX_BYTES_PER_FRAME]);
    state->frame_buffer   = malloc(state->frame_bytes);

    guac_client_log(client, GUAC_LOG_DEBUG,
            "Frames will last %i microseconds and contain %i bytes",
            state->frame_duration, state->frame_bytes);

    /* Start with the file closed, playback not paused */
    state->stream = stream;
    state->fd = fd;
    state->paused = false;
    state->file_size = file_size;

    /* Set client handlers and data */
    client->handle_messages = streamtest_client_message_handler;
    client->key_handler     = streamtest_client_key_handler;
    client->free_handler    = streamtest_client_free_handler;
    client->data = state;

    /* Init display */
    guac_protocol_send_size(client->socket, GUAC_DEFAULT_LAYER,
            STREAMTEST_PROGRESS_WIDTH,
            STREAMTEST_PROGRESS_HEIGHT);

    /* Render initial progress bar */
    streamtest_render_progress(client);
    guac_socket_flush(client->socket);

    /* Initialization complete */
    return 0;

}

