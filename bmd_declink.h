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

#ifndef _BMD_DECLINK_H_
#define _BMD_DECLINK_H_

#define BMD_FLAGS_VIDEO_PRESENT 1
#define BMD_FLAGS_AUDIO_PRESENT 2

struct bmd_av_info
{
    int got_video; /* boolean */
    int got_audio; /* boolean */
    int vformat;
    int vwidth;
    int vheight;
    int vstride_bytes;
    int vtime;
    int pad0;
    char* vdata;
    int vdata_alloc_bytes;
    int aformat;
    int achannels;
    int abytes_per_sample;
    int asamples;
    int atime;
    char* adata;
    int adata_alloc_bytes;
    int pad1;
    int av_pipe[2];
    pthread_mutex_t av_mutex;
};

#ifdef __cplusplus
extern "C"
{
#endif

int
bmd_declink_create(struct bmd_av_info* av_info, void** obj);
int
bmd_declink_delete(void* obj);
int
bmd_declink_start(void* obj);
int
bmd_declink_stop(void* obj);

#ifdef __cplusplus
}
#endif

#endif

