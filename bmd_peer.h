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

#ifndef _BMD_PEER_H_
#define _BMD_PEER_H_

#define BMD_PEER_SUBSCRIBE_AUDIO        1
#define BMD_PEER_REQUEST_VIDEO          2

int
bmd_peer_get_fds(struct bmd_info* hdhrd, int* max_fd,
                 fd_set* rfds, fd_set* wfds);
int
bmd_peer_check_fds(struct bmd_info* bmd, fd_set* rfds, fd_set* wfds);
int
bmd_peer_add_fd(struct bmd_info* bmd, int sck);
int
bmd_peer_cleanup(struct bmd_info* bmd);
int
bmd_peer_queue_all_video(struct bmd_info* bmd);
int
bmd_peer_queue_all_audio(struct bmd_info* bmd, struct stream* out_s);
int
bmd_peer_queue(struct peer_info* peer, struct stream* out_s);

#endif

