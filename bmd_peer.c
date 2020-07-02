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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "arch.h"
#include "parse.h"
#include "bmd.h"
#include "bmd_peer.h"
#include "bmd_log.h"
#include "bmd_utils.h"
#include "bmd_error.h"

struct peer_info
{
    int sck;
    int got_subscribe_audio; /* boolean */
    int got_request_video; /* boolean */
    int video_frame_count;
    struct stream* out_s_head;
    struct stream* out_s_tail;
    struct stream* in_s;
    struct peer_info* next;
};

/*****************************************************************************/
static int
bmd_peer_delete_one(struct peer_info* peer)
{
    struct stream* out_s;
    struct stream* lout_s;

    close(peer->sck);
    out_s = peer->out_s_head;
    while (out_s != NULL)
    {
        lout_s = out_s;
        out_s = out_s->next;
        if (lout_s->data == NULL)
        {
            if (lout_s->fd > 0)
            {
                close(lout_s->fd);
            }
        }
        else
        {
            free(lout_s->data);
        }
        free(lout_s);
    }
    if (peer->in_s != NULL)
    {
        free(peer->in_s->data);
        free(peer->in_s);
    }
    free(peer);
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_peer_remove_one(struct bmd_info* bmd, struct peer_info** apeer,
                    struct peer_info* last_peer)
{
    struct peer_info* peer;
    struct peer_info* next_peer;

    peer = *apeer;
    if ((bmd->peer_head == peer) && (bmd->peer_tail == peer))
    {
        /* remove only item */
        LOGLN10((LOG_INFO, LOGS "remove only item", LOGP));
        bmd->peer_head = NULL;
        bmd->peer_tail = NULL;
        bmd_peer_delete_one(peer);
        peer = NULL;
    }
    else if (bmd->peer_head == peer)
    {
        /* remove first item */
        LOGLN10((LOG_INFO, LOGS "remove first item", LOGP));
        bmd->peer_head = peer->next;
        next_peer = peer->next;
        bmd_peer_delete_one(peer);
        peer = next_peer;
    }
    else if (bmd->peer_tail == peer)
    {
        /* remove last item */
        LOGLN10((LOG_INFO, LOGS "remove last item", LOGP));
        bmd->peer_tail = last_peer;
        last_peer->next = NULL;
        bmd_peer_delete_one(peer);
        peer = NULL;
    }
    else
    {
        /* remove middle item */
        LOGLN10((LOG_INFO, LOGS "remove middle item", LOGP));
        last_peer->next = peer->next;
        next_peer = peer->next;
        bmd_peer_delete_one(peer);
        peer = next_peer;
    }
    *apeer = peer;
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_peer_queue_frame(struct bmd_info* bmd, struct peer_info* peer)
{
    struct stream* out_s;
    int rv;

    if (bmd->fd < 1)
    {
        return BMD_ERROR_FD;
    }
    out_s = (struct stream*)calloc(1, sizeof(struct stream));
    if (out_s == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    out_s->size = 1024 * 1024;
    out_s->data = (char*)malloc(out_s->size);
    if (out_s->data == NULL)
    {
        free(out_s);
        return BMD_ERROR_MEMORY;
    }
    if (peer->video_frame_count == bmd->video_frame_count)
    {
        LOGLN0((LOG_INFO, LOGS "peer->video_frame_count %d "
                "bmd->video_frame_count %d",
                LOGP, peer->video_frame_count,
                bmd->video_frame_count));
    }
    peer->video_frame_count = bmd->video_frame_count;
    out_s->p = out_s->data;
    out_uint32_le(out_s, BMD_PDU_CODE_VIDEO);
    out_uint32_le(out_s, 40);
    out_uint32_le(out_s, bmd->fd_time);
    out_uint8s(out_s, 4);
    out_uint32_le(out_s, bmd->fd);
    out_uint32_le(out_s, bmd->fd_width);
    out_uint32_le(out_s, bmd->fd_height);
    out_uint32_le(out_s, bmd->fd_stride);
    out_uint32_le(out_s, bmd->fd_size);
    out_uint32_le(out_s, bmd->fd_bpp);
    out_s->end = out_s->p;
    rv = bmd_peer_queue(peer, out_s);
    free(out_s->data);
    if (rv == BMD_ERROR_NONE)
    {
        memset(out_s, 0, sizeof(struct stream));
        out_s->fd = bmd->fd;
        rv = bmd_peer_queue(peer, out_s);
    }
    free(out_s);
    return rv;
}

/*****************************************************************************/
static int
bmd_peer_process_msg_request_video_frame(struct bmd_info* bmd,
                                         struct peer_info* peer,
                                         struct stream* in_s)
{
    int rv;

    (void)in_s;

    if (peer->got_request_video)
    {
        LOGLN10((LOG_INFO, LOGS "already requested", LOGP));
        return BMD_ERROR_NONE;
    }
    if ((bmd->fd < 1) ||
        (peer->video_frame_count == bmd->video_frame_count))
    {
        LOGLN10((LOG_INFO, LOGS "set to get next frame", LOGP));
        peer->got_request_video = 1;
        return BMD_ERROR_NONE;
    }
    LOGLN10((LOG_INFO, LOGS "sending frame now", LOGP));
    rv = bmd_peer_queue_frame(bmd, peer);
    return rv;
}

/*****************************************************************************/
static int
bmd_peer_process_msg_subscribe_audio(struct bmd_info* bmd,
                                     struct peer_info* peer,
                                     struct stream* in_s)
{
    unsigned char val8;

    (void)bmd;

    in_uint8(in_s, val8);
    if (val8)
    {
        peer->got_subscribe_audio = 1;
    }
    else
    {
        peer->got_subscribe_audio = 0;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_peer_process_msg_version(struct bmd_info* bmd,
                             struct peer_info* peer,
                             struct stream* in_s)
{
    int version_major;
    int version_minor;

    (void)bmd;
    (void)peer;
    if (!s_check_rem(in_s, 8))
    {
        return BMD_ERROR_RANGE;
    }
    in_uint32_le(in_s, version_major);
    in_uint32_le(in_s, version_minor);
    LOGLN0((LOG_INFO, LOGS "connection client version %d %d",
            LOGP, version_major, version_minor));
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_peer_process_msg(struct bmd_info* bmd, struct peer_info* peer)
{
    int pdu_code;
    int pdu_bytes;
    int rv;
    struct stream* in_s;

    (void)bmd;

    rv = BMD_ERROR_NONE;
    in_s = peer->in_s;
    in_uint32_le(in_s, pdu_code);
    in_uint32_le(in_s, pdu_bytes);
    LOGLN10((LOG_INFO, LOGS "sck %d pdu_code %d pdu_bytes %d",
             LOGP, peer->sck, pdu_code, pdu_bytes));
    if ((pdu_bytes < 8) || !s_check_rem(in_s, pdu_bytes - 8))
    {
        LOGLN0((LOG_INFO, LOGS "bad pdu_bytes, sck %d pdu_code %d "
                "pdu_bytes %d", LOGP, peer->sck, pdu_code, pdu_bytes));
        return BMD_ERROR_RANGE;
    }
    switch (pdu_code)
    {
        case BMD_PDU_CODE_SUBSCRIBE_AUDIO:
            rv = bmd_peer_process_msg_subscribe_audio(bmd, peer, in_s);
            break;
        case BMD_PDU_CODE_REQUEST_VIDEO_FRAME:
            rv = bmd_peer_process_msg_request_video_frame(bmd, peer, in_s);
            break;
        case BMD_PDU_CODE_VERSION:
            rv = bmd_peer_process_msg_version(bmd, peer, in_s);
            break;
    }
    return rv;
}

/*****************************************************************************/
int
bmd_peer_get_fds(struct bmd_info* bmd, int* max_fd,
                 fd_set* rfds, fd_set* wfds)
{
    struct peer_info* peer;
    int lmax_fd;

    lmax_fd = *max_fd;
    peer = bmd->peer_head;
    while (peer != NULL)
    {
        if (peer->sck > lmax_fd)
        {
            lmax_fd = peer->sck;
        }
        FD_SET(peer->sck, rfds);
        if (peer->out_s_head != NULL)
        {
            FD_SET(peer->sck, wfds);
        }
        peer = peer->next;
    }
    *max_fd = lmax_fd;
    return BMD_ERROR_NONE;
}

/******************************************************************************/
static int
bmd_peer_send_fd(int sck, int fd)
{
    ssize_t size;
    struct msghdr msg;
    struct iovec iov;
    union _cmsgu
    {
        struct cmsghdr cmsghdr;
        char control[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    struct cmsghdr *cmsg;
    int *fds;
    char text[4] = "int";

    iov.iov_base = text;
    iov.iov_len = 4;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    fds = (int *) CMSG_DATA(cmsg);
    *fds = fd;
    size = sendmsg(sck, &msg, 0);
    LOGLN10((LOG_INFO, LOGS "size %d", LOGP, (int)size));
    if (size != 4)
    {
        return BMD_ERROR_FD;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
int
bmd_peer_check_fds(struct bmd_info* bmd, fd_set* rfds, fd_set* wfds)
{
    struct peer_info* peer;
    struct peer_info* last_peer;
    struct stream* out_s;
    struct stream* in_s;
    int out_bytes;
    int in_bytes;
    int sent;
    int reed;
    int pdu_bytes;
    int rv;
    int error;

    rv = BMD_ERROR_NONE;
    last_peer = NULL;
    peer = bmd->peer_head;
    while (peer != NULL)
    {
        if (FD_ISSET(peer->sck, rfds))
        {
            in_s = peer->in_s;
            if (in_s == NULL)
            {
                in_s = (struct stream*)calloc(1, sizeof(struct stream));
                if (in_s == NULL)
                {
                    return BMD_ERROR_MEMORY;
                }
                in_s->size = 1024 * 1024;
                in_s->data = (char*)malloc(in_s->size);
                if (in_s->data == NULL)
                {
                    free(in_s);
                    return BMD_ERROR_MEMORY;
                }
                in_s->p = in_s->data;
                in_s->end = in_s->data;
                peer->in_s = in_s;
            }
            if (in_s->p == in_s->data)
            {
                in_s->end = in_s->data + 8;
            }
            in_bytes = (int)(in_s->end - in_s->p);
            reed = recv(peer->sck, in_s->p, in_bytes, 0);
            if (reed < 1)
            {
                /* error */
                LOGLN0((LOG_ERROR, LOGS "recv failed sck %d reed %d",
                        LOGP, peer->sck, reed));
                error = bmd_peer_remove_one(bmd, &peer, last_peer);
                if (error != BMD_ERROR_NONE)
                {
                    return error;
                }
                rv = BMD_ERROR_PEER_REMOVED;
                continue;
            }
            else
            {
                in_s->p += reed;
                if (in_s->p >= in_s->end)
                {
                    if (in_s->p == in_s->data + 8)
                    {
                        /* finished reading in header */
                        in_s->p = in_s->data;
                        in_uint8s(in_s, 4); /* pdu_code */
                        in_uint32_le(in_s, pdu_bytes);
                        if ((pdu_bytes < 8) || (pdu_bytes > in_s->size))
                        {
                            LOGLN0((LOG_ERROR, LOGS "bad pdu_bytes %d",
                                    LOGP, pdu_bytes));
                            error = bmd_peer_remove_one(bmd, &peer, last_peer);
                            if (error != BMD_ERROR_NONE)
                            {
                                return error;
                            }
                            rv = BMD_ERROR_PEER_REMOVED;
                            continue;
                        }
                        in_s->end = in_s->data + pdu_bytes;
                    }
                    if (in_s->p >= in_s->end)
                    {
                        /* finished reading in header and payload */
                        in_s->p = in_s->data;
                        rv = bmd_peer_process_msg(bmd, peer);
                        if (rv != BMD_ERROR_NONE)
                        {
                            LOGLN0((LOG_ERROR, LOGS "bmd_peer_process_msg "
                                   "failed", LOGP));
                            error = bmd_peer_remove_one(bmd, &peer, last_peer);
                            if (error != BMD_ERROR_NONE)
                            {
                                return error;
                            }
                            rv = BMD_ERROR_PEER_REMOVED;
                            continue;
                        }
                        in_s->p = in_s->data;
                    }
                }
            }
        }
        if (FD_ISSET(peer->sck, wfds))
        {
            out_s = peer->out_s_head;
            if (out_s != NULL)
            {
                if (out_s->data == NULL)
                {
                    rv = bmd_peer_send_fd(peer->sck, out_s->fd);
                    if (rv != BMD_ERROR_NONE)
                    {
                        /* error */
                        LOGLN0((LOG_ERROR, LOGS "bmd_peer_send_fd failed "
                                "fd %d", LOGP, out_s->fd));
                        error = bmd_peer_remove_one(bmd, &peer, last_peer);
                        if (error != BMD_ERROR_NONE)
                        {
                            return error;
                        }
                        rv = BMD_ERROR_PEER_REMOVED;
                        continue;
                    }
                    LOGLN10((LOG_DEBUG, LOGS "bmd_peer_send_fd ok", LOGP));
                    if (out_s->next == NULL)
                    {
                        peer->out_s_head = NULL;
                        peer->out_s_tail = NULL;
                    }
                    else
                    {
                        peer->out_s_head = out_s->next;
                    }
                    close(out_s->fd);
                    free(out_s);
                }
                else
                {
                    out_bytes = (int)(out_s->end - out_s->p);
                    sent = send(peer->sck, out_s->p, out_bytes, 0);
                    if (sent < 1)
                    {
                        /* error */
                        LOGLN0((LOG_ERROR, LOGS "send failed", LOGP));
                        error = bmd_peer_remove_one(bmd, &peer, last_peer);
                        if (error != BMD_ERROR_NONE)
                        {
                            return error;
                        }
                        rv = BMD_ERROR_PEER_REMOVED;
                        continue;
                    }
                    LOGLN10((LOG_DEBUG, LOGS "send ok, sent %d", LOGP, sent));
                    out_s->p += sent;
                    if (out_s->p >= out_s->end)
                    {
                        if (out_s->next == NULL)
                        {
                            peer->out_s_head = NULL;
                            peer->out_s_tail = NULL;
                        }
                        else
                        {
                            peer->out_s_head = out_s->next;
                        }
                        free(out_s->data);
                        free(out_s);
                    }
                }
            }
        }
        last_peer = peer;
        peer = peer->next;
    }
    return rv;
}

/*****************************************************************************/
static int
bmd_queue_version(struct bmd_info* bmd, struct peer_info* peer)
{
    struct stream* out_s;
    int rv;

    (void)bmd;
    out_s = (struct stream*)calloc(1, sizeof(struct stream));
    if (out_s == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    out_s->data = (char*)malloc(1024);
    if (out_s->data == NULL)
    {
        free(out_s);
        return BMD_ERROR_MEMORY;
    }
    out_s->p = out_s->data;
    out_uint32_le(out_s, BMD_PDU_CODE_VERSION);
    out_uint32_le(out_s, 32);
    out_uint32_le(out_s, BMD_VERSION_MAJOR);
    out_uint32_le(out_s, BMD_VERSION_MINOR);
    out_uint32_le(out_s, BMD_AUDIO_LATENCY);
    out_uint8s(out_s, 12);
    out_s->end = out_s->p;
    out_s->p = out_s->data;
    rv = bmd_peer_queue(peer, out_s);
    free(out_s->data);
    free(out_s);
    return rv;
}

/*****************************************************************************/
int
bmd_peer_add_fd(struct bmd_info* bmd, int sck)
{
    struct peer_info* peer;

    peer = (struct peer_info*)calloc(1, sizeof(struct peer_info));
    if (peer == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    peer->sck = sck;
    if (bmd->peer_head == NULL)
    {
        bmd->peer_head = peer;
        bmd->peer_tail = peer;
    }
    else
    {
        bmd->peer_tail->next = peer;
        bmd->peer_tail = peer;
    }
    bmd_queue_version(bmd, peer);
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
int
bmd_peer_cleanup(struct bmd_info* bmd)
{
    struct peer_info* peer;
    struct peer_info* lpeer;

    peer = bmd->peer_head;
    while (peer != NULL)
    {
        lpeer = peer;
        peer = peer->next;
        bmd_peer_delete_one(lpeer);
    }
    bmd->peer_head = NULL;
    bmd->peer_tail = NULL;
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
int
bmd_peer_queue_all_video(struct bmd_info* bmd)
{
    int rv;
    struct peer_info* peer;

    peer = bmd->peer_head;
    while (peer != NULL)
    {
        if (peer->got_request_video)
        {
            rv = bmd_peer_queue_frame(bmd, peer);
            if (rv != BMD_ERROR_NONE)
            {
                return rv;
            }
            peer->got_request_video = 0;
        }
        peer = peer->next;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
int
bmd_peer_queue_all_audio(struct bmd_info* bmd, struct stream* out_s)
{
    int rv;
    struct peer_info* peer;

    peer = bmd->peer_head;
    while (peer != NULL)
    {
        if (peer->got_subscribe_audio)
        {
            rv = bmd_peer_queue(peer, out_s);
            if (rv != BMD_ERROR_NONE)
            {
                return rv;
            }
        }
        peer = peer->next;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
int
bmd_peer_queue(struct peer_info* peer, struct stream* out_s)
{
    struct stream* lout_s;
    int bytes;

    lout_s = (struct stream*)calloc(1, sizeof(struct stream));
    if (lout_s == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    if (out_s->data == NULL)
    {
        if (out_s->fd < 1)
        {
            free(lout_s);
            return BMD_ERROR_FD;
        }
        lout_s->fd = dup(out_s->fd);
        if (lout_s->fd == -1)
        {
            free(lout_s);
            return BMD_ERROR_DUP;
        }
        LOGLN10((LOG_INFO, LOGS "fd %d", LOGP, lout_s->fd));
    }
    else
    {
        bytes = (int)(out_s->end - out_s->data);
        if ((bytes < 1) || (bytes > 1024 * 1024))
        {
            free(lout_s);
            return BMD_ERROR_PARAM;
        }
        lout_s->size = bytes;
        lout_s->data = (char*)malloc(lout_s->size);
        if (lout_s->data == NULL)
        {
            free(lout_s);
            return BMD_ERROR_MEMORY;
        }
        lout_s->p = lout_s->data;
        out_uint8p(lout_s, out_s->data, bytes);
        lout_s->end = lout_s->p;
        lout_s->p = lout_s->data;
    }
    if (peer->out_s_tail == NULL)
    {
        peer->out_s_head = lout_s;
        peer->out_s_tail = lout_s;
    }
    else
    {
        peer->out_s_tail->next = lout_s;
        peer->out_s_tail = lout_s;
    }
    return BMD_ERROR_NONE;
}

