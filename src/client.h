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

#include <guacamole/stream.h>

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
 * The mode of playback to use. While data will be streamed identically
 * regardless of its type, the manner of setup for the display is different.
 * Audio streams will require a progress bar, while video streams need to use
 * the display for their own output.
 */
typedef enum streamtest_playback_mode {

    /**
     * Audio is being streamed, and its current progress should be displayed as
     * a progress bar.
     */
    STREAMTEST_AUDIO,

    /**
     * Video is being streamed. No progress bar should be displayed, and the
     * Guacamole display should be used solely for video output.
     */
    STREAMTEST_VIDEO

} streamtest_playback_mode;

/**
 * The current playback state.
 */
typedef struct streamtest_state {

    /**
     * Whether we are currently playing audio or video.
     */
    streamtest_playback_mode mode;

    /**
     * The duration of each frame, in microseconds.
     */
    int frame_duration;

    /**
     * The number of bytes to stream with each frame.
     */
    int frame_bytes;

    /**
     * A buffer into which bytes pending streaming can be read. This buffer
     * will be at least frame_bytes in size.
     */
    unsigned char* frame_buffer;

    /**
     * The stream over which data from the specified file will be streamed.
     */
    guac_stream* stream;

    /**
     * The file descriptor of the file being streamed.
     */
    int fd;

    /**
     * The total number of bytes within the file.
     */
    int file_size;

    /**
     * Whether playback is currently paused.
     */
    bool paused;

} streamtest_state;

#endif

