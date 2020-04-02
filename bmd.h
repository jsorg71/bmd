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

#define BMD_BUFFER_SIZE (256 * 1024)
#define BMD_SELECT_MSTIME 15
#define BMD_SYNC_MSTIME 60000
#define BMD_VIDEO_DELAY_MSTIME 3000
#define BMD_AUDIO_DELAY_MSTIME 2000
#define BMD_UDS "/tmp/wtv_bmd_%d"
#define BMD_MAX_DECODE_BUF (100 * 1024 * 1024)

#define BMD_PDU_CODE_SUBSCRIBE_AUDIO        1
#define BMD_PDU_CODE_AUDIO                  2
#define BMD_PDU_CODE_REQUEST_VIDEO_FRAME    3
#define BMD_PDU_CODE_VIDEO                  4

struct bmd_info
{
    int listener;
    int yami_fd;
    void* yami;
    struct peer_info* peer_head;
    struct peer_info* peer_tail;
    struct video_info* video_head;
    struct video_info* video_tail;
    struct audio_info* audio_head;
    struct audio_info* audio_tail;
    int video_info_count;
    int video_info_bytes;
    int audio_info_count;
    int audio_info_bytes;
    int video_diff;
    int video_update_dts;
    int audio_diff;
    int audio_update_dts;
    int fd;
    int fd_width;
    int fd_height;
    int fd_stride;
    int fd_size;
    int fd_bpp;
    int fd_time;
    int video_frame_count;
    int last_decode_mstime;
    int is_running;

    //char* video_frame;
    //char* audio_frame;
    //int video_frame_bytes;
    //int audio_frame_bytes;

};

#endif

