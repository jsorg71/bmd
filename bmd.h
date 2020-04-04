/**
 * black magic daemon
 *
 * Copyright 2020 Jay Sorg <jay.sorg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BMD_H_
#define _BMD_H_

#define BMD_UDS "/tmp/wtv_bmd_%d"

#define BMD_VERSION_MAJOR   0
#define BMD_VERSION_MINOR   1
#define BMD_AUDIO_LATENCY   64

#define BMD_PDU_CODE_SUBSCRIBE_AUDIO        1
#define BMD_PDU_CODE_AUDIO                  2
#define BMD_PDU_CODE_REQUEST_VIDEO_FRAME    3
#define BMD_PDU_CODE_VIDEO                  4
#define BMD_PDU_CODE_VERSION                5

struct bmd_info
{
    int listener;
    int yami_fd;
    int yami_width;
    int yami_height;
    void* yami;
    void* declink;
    int av_pipe[2];
    struct bmd_av_info* av_info;
    struct peer_info* peer_head;
    struct peer_info* peer_tail;
    int fd;
    int fd_width;
    int fd_height;
    int fd_stride;
    int fd_size;
    int fd_bpp;
    int fd_time;
    int video_frame_count;
    int is_running;
    int pad0;
};

#endif

