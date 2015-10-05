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

#ifndef STREAMTEST_CLIENT_H
#define STREAMTEST_CLIENT_H

#include "config.h"

#include <stdbool.h>

/**
 * The width of the stream progress bar, in pixels.
 */
#define STREAMTEST_PROGRESS_WIDTH 640

/**
 * The height of the stream progress bar, in pixels.
 */
#define STREAMTEST_PROGRESS_HEIGHT 32 

/**
 * The current playback state.
 */
typedef struct streamtest_state {

    /**
     * The filename of the file to be streamed.
     */
    char* filename;

    /**
     * The mimetype of the file being streamed.
     */
    char* mimetype;

    /**
     * The duration of each frame, in microseconds.
     */
    int frame_duration;

    /**
     * The number of bytes to stream with each frame.
     */
    int frame_bytes;

    /**
     * The file descriptor of the file being streamed, if the file is currently
     * open. If no file is currently open, this will be -1.
     */
    int fd;

    /**
     * The number of bytes read from the file.
     */
    int bytes_read;

    /**
     * The number of bytes remaining to be read within the file.
     */
    int bytes_remaining;

    /**
     * Whether playback is currently paused.
     */
    bool paused;

} streamtest_state;

#endif

