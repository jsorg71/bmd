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
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <yami_inf.h>

#include "arch.h"
#include "parse.h"
#include "bmd.h"
#include "bmd_error.h"
#include "bmd_declink.h"
#include "bmd_log.h"
#include "bmd_peer.h"
#include "bmd_utils.h"

static int g_term_pipe[2];

struct settings_info
{
    char bmd_uds[256];
    char bmd_uds_name[256];
    char bmd_log_filename[256];
    int daemonize;
    int mode_index;
};

#define NUM_MODE_NAMES 16
const char g_mode_names[NUM_MODE_NAMES][16] =
{
    "525i59.94 NTSC",
    "525p23.98 NTSC",
    "625i50 PAL",
    "525p59.94 NTSC",
    "625p50 PAL",
    "1080p23.98",
    "1080p24",
    "1080p25",
    "1080p29.97",
    "1080p30",
    "1080i50",
    "1080i59.94",
    "1080i60",
    "720p50",
    "720p59.94",
    "720p60"
};

/******************************************************************************/
/* convert yuy2 to nv12, 16 bit to 12 bit
   yuyv to y plane uv plane */
static int
yuy2_to_nv12(void* src, int src_stride_bytes,
             void* dst[], int dst_stride_bytes[],
             int width, int height)
{
    unsigned char* src8;
    unsigned char* src81;
    unsigned char* src82;
    unsigned char* ydst8;
    unsigned char* ydst81;
    unsigned char* ydst82;
    unsigned char* uvdst8;
    unsigned char* uvdst81;
    int index;
    int indexd2;
    int jndex;
    int sum;
    int y_stride_bytes;
    int uv_stride_bytes;

    src8 = (unsigned char*)src;
    ydst8 = (unsigned char*)(dst[0]);
    uvdst8 = (unsigned char*)(dst[1]);
    y_stride_bytes = dst_stride_bytes[0];
    uv_stride_bytes = dst_stride_bytes[1];
    for (index = 0; index < height; index += 2)
    {
        indexd2 = index / 2;
        src81 = src8 + (index * src_stride_bytes);
        src82 = src81 + src_stride_bytes;
        ydst81 = ydst8 + (index * y_stride_bytes);
        ydst82 = ydst81 + y_stride_bytes;
        uvdst81 = uvdst8 + (indexd2 * uv_stride_bytes);
        for (jndex = 0; jndex < width; jndex += 2)
        {
            ydst81[0] = src81[1];
            ydst81[1] = src81[3];
            ydst82[0] = src82[1];
            ydst82[1] = src82[3];
            sum = src81[0] + src82[0];
            uvdst81[0] = (sum + 1) / 2;
            sum = src81[2] + src82[2];
            uvdst81[1] = (sum + 1) / 2;
            src81 += 4;
            src82 += 4;
            ydst81 += 2;
            ydst82 += 2;
            uvdst81 += 2;
        }
    }
    return 0;
}

/*****************************************************************************/
static int
bmd_process_av(struct bmd_info* bmd)
{
    struct bmd_av_info* av_info;
    struct stream* out_s;
    int bytes;
    char* nv12_data;
    void* dst_data[2];
    int dst_stride[2];
    void* ydata;
    void* uvdata;
    int ydata_stride_bytes;
    int uvdata_stride_bytes;
    int index;
    char* src8;
    char* dst8;

    LOGLN10((LOG_INFO, LOGS, LOGP));
    av_info = bmd->av_info;
    if (av_info == NULL)
    {
        return BMD_ERROR_NONE;
    }
    nv12_data = NULL;
    out_s = NULL;
    pthread_mutex_lock(&(av_info->av_mutex));
    if (av_info->got_video)
    {
        LOGLN10((LOG_INFO, LOGS "got video", LOGP));
        av_info->got_video = 0;
        bytes = av_info->vwidth * av_info->vheight * 2;
        nv12_data = (char*)malloc(bytes);
        if (nv12_data != NULL)
        {
            dst_data[0] = nv12_data;
            dst_data[1] = nv12_data + (av_info->vwidth * av_info->vheight);
            dst_stride[0] = av_info->vwidth;
            dst_stride[1] = av_info->vwidth;
            yuy2_to_nv12(av_info->vdata, av_info->vstride_bytes,
                         dst_data, dst_stride,
                         av_info->vwidth, av_info->vheight);
        }
    }
    if (av_info->got_audio)
    {
        LOGLN10((LOG_INFO, LOGS "got audio", LOGP));
        av_info->got_audio = 0;
        bytes = av_info->achannels * av_info->abytes_per_sample *
                av_info->asamples;
        out_s = (struct stream*)calloc(1, sizeof(struct stream));
        if (out_s != NULL)
        {
            out_s->size = bytes + 1024;
            out_s->data = (char*)malloc(out_s->size);
            if (out_s->data != NULL)
            {
                out_s->p = out_s->data;
                out_uint32_le(out_s, BMD_PDU_CODE_AUDIO);
                out_uint32_le(out_s, 24 + bytes);
                out_uint32_le(out_s, av_info->atime);
                out_uint8s(out_s, 4);
                out_uint32_le(out_s, av_info->achannels);
                out_uint32_le(out_s, bytes);
                out_uint8p(out_s, av_info->adata, bytes);
                out_s->end = out_s->p;
                out_s->p = out_s->data;
            }
        }
    }
    pthread_mutex_unlock(&(av_info->av_mutex));
    if (out_s != NULL)
    {
        if (out_s->data != NULL)
        {
            bmd_peer_queue_all_audio(bmd, out_s);
            free(out_s->data);
        }
        free(out_s);
    }
    if (nv12_data != NULL)
    {
        if ((bmd->yami == NULL) ||
            (bmd->yami_width != av_info->vwidth) ||
            (bmd->yami_height != av_info->vheight))
        {
            LOGLN0((LOG_INFO, LOGS "yami_surface_create width %d height %d",
                    LOGP, av_info->vwidth, av_info->vheight));
            yami_surface_delete(bmd->yami);
            if (yami_surface_create(&(bmd->yami),
                                    av_info->vwidth, av_info->vheight,
                                    0, 0) != YI_SUCCESS)
            {
                LOGLN0((LOG_ERROR, LOGS "yami_surface_create failed", LOGP));
                bmd->yami = NULL;
                free(nv12_data);
                return 1;
            }
            bmd->video_frame_count = 0;
            bmd->yami_width = av_info->vwidth;
            bmd->yami_height = av_info->vheight;
        }
        if (yami_surface_get_ybuffer(bmd->yami, &ydata,
                                     &ydata_stride_bytes) != YI_SUCCESS)
        {
            LOGLN0((LOG_ERROR, LOGS "yami_surface_get_ybuffer failed", LOGP));
            free(nv12_data);
            return 1;
        }
        src8 = nv12_data;
        dst8 = ydata;
        bytes = av_info->vwidth;
        if (bytes > ydata_stride_bytes)
        {
            bytes = ydata_stride_bytes;
        }
        for (index = 0; index < av_info->vheight; index++)
        {
            memcpy(dst8, src8, bytes);
            src8 += av_info->vwidth;
            dst8 += ydata_stride_bytes;
        }
        if (yami_surface_get_uvbuffer(bmd->yami, &uvdata,
                                      &uvdata_stride_bytes) != YI_SUCCESS)
        {
            LOGLN0((LOG_ERROR, LOGS "yami_surface_get_uvbuffer failed", LOGP));
            free(nv12_data);
            return 1;
        }
        src8 = nv12_data + av_info->vwidth * av_info->vheight;
        dst8 = uvdata;
        bytes = av_info->vwidth;
        if (bytes > uvdata_stride_bytes)
        {
            bytes = uvdata_stride_bytes;
        }
        for (index = 0; index < av_info->vheight; index += 2)
        {
            memcpy(dst8, src8, bytes);
            src8 += av_info->vwidth;
            dst8 += uvdata_stride_bytes;
        }
        free(nv12_data);
        if (bmd->fd > 0)
        {
            close(bmd->fd);
            bmd->fd = 0;
        }
        if (yami_surface_get_fd_dst(bmd->yami, &(bmd->fd),
                                    &(bmd->fd_width),
                                    &(bmd->fd_height),
                                    &(bmd->fd_stride),
                                    &(bmd->fd_size),
                                    &(bmd->fd_bpp))!= YI_SUCCESS)
        {
            LOGLN0((LOG_ERROR, LOGS "yami_surface_get_fd_dst failed", LOGP));
            return 1;
        }
        bmd->fd_time = av_info->vtime;
        bmd->video_frame_count++;
        bmd_peer_queue_all_video(bmd);
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static void
sig_int(int sig)
{
    (void)sig;
    if (write(g_term_pipe[1], "sig", 4) != 4)
    {
    }
}

/*****************************************************************************/
static void
sig_pipe(int sig)
{
    (void)sig;
}

/*****************************************************************************/
static int
process_args(int argc, char** argv, struct settings_info* settings)
{
    int index;

    if (argc < 1)
    {
        return BMD_ERROR_PARAM;
    }
    strncpy(settings->bmd_uds_name, BMD_UDS, 255);
    for (index = 1; index < argc; index++)
    {
        if (strcmp("-D", argv[index]) == 0)
        {
            settings->daemonize = 1;
        }
        else if (strcmp("-n", argv[index]) == 0)
        {
            index++;
            strncpy(settings->bmd_uds_name, argv[index], 255);
        }
        else if (strcmp("-m", argv[index]) == 0)
        {
            index++;
            settings->mode_index = atoi(argv[index]) % NUM_MODE_NAMES;
        }
        else
        {
            return BMD_ERROR_PARAM;
        }
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
printf_help(int argc, char** argv)
{
    int index;

    if (argc < 1)
    {
        return BMD_ERROR_NONE;
    }
    printf("%s: command line options\n", argv[0]);
    printf("    -D      run daemon, example -D\n");
    printf("    -n      uds name, %%d will be pid, example -n %s\n", BMD_UDS);
    printf("    -m      mode index, default 14\n");
    for (index = 0; index < NUM_MODE_NAMES; index++)
    {
        printf("                %d - %s\n", index, g_mode_names[index]);
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_cleanup(struct bmd_info* bmd)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    if (bmd->declink != NULL)
    {
        bmd_declink_stop(bmd->declink);
        bmd_declink_delete(bmd->declink);
        bmd->declink = NULL;
    }
    if (bmd->av_info != NULL)
    {
        LOGLN0((LOG_INFO, LOGS "av_info cleanup", LOGP));
        free(bmd->av_info->vdata);
        free(bmd->av_info->adata);
        pthread_mutex_destroy(&(bmd->av_info->av_mutex));
        close(bmd->av_info->av_pipe[0]);
        close(bmd->av_info->av_pipe[1]);
        free(bmd->av_info);
        bmd->av_info = NULL;
    }
    if (bmd->yami != NULL)
    {
        yami_surface_delete(bmd->yami);
        bmd->yami = NULL;
    }
    bmd->yami_width = 0;
    bmd->yami_height = 0;
    if (bmd->fd > 0)
    {
        close(bmd->fd);
        bmd->fd = 0;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_start(struct bmd_info* bmd, struct settings_info* settings)
{
    int error;

    (void)settings;
    LOGLN0((LOG_INFO, LOGS, LOGP));
    bmd->av_info = (struct bmd_av_info*)calloc(1, sizeof(struct bmd_av_info));
    if (bmd->av_info == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    if (pthread_mutex_init(&(bmd->av_info->av_mutex), NULL) != 0)
    {
        free(bmd->av_info);
        bmd->av_info = NULL;
        return BMD_ERROR_MUTEX;
    }
    if (pipe(bmd->av_info->av_pipe) != 0)
    {
        pthread_mutex_destroy(&(bmd->av_info->av_mutex));
        free(bmd->av_info);
        bmd->av_info = NULL;
        return BMD_ERROR_PIPE;
    }
    error = bmd_declink_create(settings->mode_index, bmd->av_info,
                               &(bmd->declink));
    if (error != BMD_ERROR_NONE)
    {
        /* bmd_cleanup will cleanup */
        return error;
    }
    error = bmd_declink_start(bmd->declink);
    if (error != BMD_ERROR_NONE)
    {
        /* bmd_cleanup will cleanup */
        return error;
    }
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_stop(struct bmd_info* bmd)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    bmd_cleanup(bmd);
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_process_fds(struct bmd_info* bmd, struct settings_info* settings,
                int mstime)
{
    int max_fd;
    int now;
    int rv;
    int millis;
    int error;
    int sck;
    fd_set rfds;
    fd_set wfds;
    struct timeval time;
    struct timeval* ptime;
    socklen_t sock_len;
    struct sockaddr_un s;
    char char4[4];

    rv = BMD_ERROR_NONE;
    for (;;)
    {
        max_fd = bmd->listener;
        if (g_term_pipe[0] > max_fd)
        {
            max_fd = g_term_pipe[0];
        }
        if (bmd->av_info != NULL)
        {
            if (bmd->av_info->av_pipe[0] > max_fd)
            {
                max_fd = bmd->av_info->av_pipe[0];
            }
        }
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(bmd->listener, &rfds);
        FD_SET(g_term_pipe[0], &rfds);
        if (bmd->av_info != NULL)
        {
            FD_SET(bmd->av_info->av_pipe[0], &rfds);
        }
        if (bmd_peer_get_fds(bmd, &max_fd, &rfds, &wfds) != 0)
        {
            LOGLN0((LOG_ERROR, LOGS "bmd_peer_get_fds failed", LOGP));
        }
        if (mstime == -1)
        {
            ptime = NULL;
        }
        else
        {
            if (get_mstime(&now) != BMD_ERROR_NONE)
            {
                LOGLN0((LOG_ERROR, LOGS "get_mstime failed", LOGP));
                break;
            }
            millis = mstime - now;
            if (millis < 0)
            {
                millis = 0;
            }
            time.tv_sec = millis / 1000;
            time.tv_usec = (millis * 1000) % 1000000;
            LOGLN10((LOG_INFO, LOGS "millis %d", LOGP, millis));
            ptime = &time;
        }
        error = select(max_fd + 1, &rfds, &wfds, 0, ptime);
        if (error > 0)
        {
            if (FD_ISSET(g_term_pipe[0], &rfds))
            {
                LOGLN0((LOG_INFO, LOGS "g_term_pipe set", LOGP));
                rv = BMD_ERROR_TERM;
                break;
            }
            if (bmd->av_info != NULL)
            {
                if (FD_ISSET(bmd->av_info->av_pipe[0], &rfds))
                {
                    LOGLN10((LOG_INFO, LOGS "av_pipe set", LOGP));
                    if (read(bmd->av_info->av_pipe[0], char4, 4) != 4)
                    {
                        LOGLN0((LOG_INFO, LOGS "read failed", LOGP));
                        break;
                    }
                    bmd_process_av(bmd);
                }
            }
            if (FD_ISSET(bmd->listener, &rfds))
            {
                sock_len = sizeof(struct sockaddr_un);
                sck = accept(bmd->listener, (struct sockaddr*)&s, &sock_len);
                LOGLN0((LOG_INFO, LOGS "got connection sck %d", LOGP, sck));
                if (sck != -1)
                {
                    if (bmd_peer_add_fd(bmd, sck) != BMD_ERROR_NONE)
                    {
                        LOGLN0((LOG_ERROR, LOGS "bmd_peer_add_fd failed",
                                LOGP));
                        close(sck);
                    }
                    else
                    {
                        if (bmd->is_running == 0)
                        {
                            if (bmd_start(bmd, settings) == 0)
                            {
                                bmd->is_running = 1;
                                break;
                            }
                            else
                            {
                                bmd_stop(bmd);
                            }
                        }
                    }
                }
            }
            error = bmd_peer_check_fds(bmd, &rfds, &wfds);
            if (error != BMD_ERROR_NONE)
            {
                LOGLN0((LOG_ERROR, LOGS "bmd_peer_check_fds error %d",
                        LOGP, error));
                if (bmd->peer_head == NULL)
                {
                    if (bmd->is_running)
                    {
                        if (bmd_stop(bmd) == 0)
                        {
                            bmd->is_running = 0;
                            break;
                        }
                    }
                }
            }
        }
        if (mstime == -1)
        {
            continue;
        }
        if (get_mstime(&now) != BMD_ERROR_NONE)
        {
            LOGLN0((LOG_ERROR, LOGS "get_mstime failed", LOGP));
            break;
        }
        if (now >= mstime)
        {
            break;
        }
    }
    return rv;
}

/*****************************************************************************/
int
main(int argc, char** argv)
{
    struct bmd_info* bmd;
    //void* bmd_declink;
    struct settings_info* settings;
    int error;
    int pid;
    struct sockaddr_un s;
    socklen_t sock_len;

    settings = (struct settings_info*)calloc(1, sizeof(struct settings_info));
    if (settings == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "calloc failed", LOGP));
        return 1;
    }
    settings->mode_index = 14;
    if (process_args(argc, argv, settings) != 0)
    {
        printf_help(argc, argv);
        free(settings);
        return 0;
    }
    if (settings->daemonize)
    {
        error = fork();
        if (error == 0)
        {
            close(0);
            close(1);
            close(2);
            open("/dev/null", O_RDONLY);
            open("/dev/null", O_WRONLY);
            open("/dev/null", O_WRONLY);
            pid = getpid();
            if (settings->bmd_log_filename[0] == 0)
            {
                snprintf(settings->bmd_log_filename, 255,
                         "/tmp/bmd_%d.log", pid);
            }
            log_init(LOG_FLAG_FILE, 4, settings->bmd_log_filename);
        }
        else if (error > 0)
        {
            printf("start daemon with pid %d\n", error);
            free(settings);
            return 0;
        }
        else
        {
            printf("fork failed\n");
            free(settings);
            return 1;
        }
    }
    else
    {
        pid = getpid();
        log_init(LOG_FLAG_STDOUT, 4, NULL);
    }
    bmd = (struct bmd_info*)calloc(1, sizeof(struct bmd_info));
    if (bmd == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "calloc failed", LOGP));
        free(settings);
        return 1;
    }
    bmd->yami_fd = open("/dev/dri/renderD128", O_RDWR);
    if (bmd->yami_fd == -1)
    {
        LOGLN0((LOG_ERROR, LOGS "open /dev/dri/renderD128 failed", LOGP));
        free(settings);
        free(bmd);
        return 1;
    }
    error = yami_init(YI_TYPE_DRM, (void*)(long)(bmd->yami_fd));
    LOGLN0((LOG_INFO, LOGS "yami_init rv %d", LOGP, error));
    if (error != 0)
    {
        LOGLN0((LOG_ERROR, LOGS "yami_init failed %d", LOGP, error));
    }
    snprintf(settings->bmd_uds, 255, settings->bmd_uds_name, pid);
    unlink(settings->bmd_uds);
    bmd->listener = socket(PF_LOCAL, SOCK_STREAM, 0);
    if (bmd->listener == -1)
    {
        LOGLN0((LOG_ERROR, LOGS "socket failed", LOGP));
        free(settings);
        free(bmd);
        return 1;
    }
    memset(&s, 0, sizeof(struct sockaddr_un));
    s.sun_family = AF_UNIX;
    strncpy(s.sun_path, settings->bmd_uds, sizeof(s.sun_path));
    s.sun_path[sizeof(s.sun_path) - 1] = 0;
    sock_len = sizeof(struct sockaddr_un);
    error = bind(bmd->listener, (struct sockaddr*)&s, sock_len);
    if (error != 0)
    {
        LOGLN0((LOG_ERROR, LOGS "bind failed", LOGP));
        close(bmd->listener);
        free(settings);
        free(bmd);
        return 1;
    }
    error = listen(bmd->listener, 2);
    if (error != 0)
    {
        LOGLN0((LOG_ERROR, LOGS "listen failed", LOGP));
        close(bmd->listener);
        free(settings);
        free(bmd);
        return 1;
    }
    error = chmod(settings->bmd_uds, 0666);
    if (error != 0)
    {
        LOGLN0((LOG_ERROR, LOGS "chmod failed for %s",
                LOGP, settings->bmd_uds));
        close(bmd->listener);
        free(settings);
        free(bmd);
        return 1;
    }
    LOGLN0((LOG_INFO, LOGS "listen ok socket %d uds %s",
            LOGP, bmd->listener, settings->bmd_uds));
    error = pipe(g_term_pipe);
    if (error != 0)
    {
        LOGLN0((LOG_ERROR, LOGS "pipe failed", LOGP));
        close(bmd->listener);
        free(settings);
        free(bmd);
        return 1;
    }

    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);
    signal(SIGPIPE, sig_pipe);

    for (;;)
    {
        error = bmd_process_fds(bmd, settings, -1);
        if (error != BMD_ERROR_NONE)
        {
            LOGLN0((LOG_ERROR, LOGS "bmd_process_fds failed error %d",
                    LOGP, error));
            break;
        }
    }

    close(bmd->listener);
    unlink(settings->bmd_uds);
    bmd_cleanup(bmd);
    yami_deinit();
    close(bmd->yami_fd);
    free(bmd);
    free(settings);
    close(g_term_pipe[0]);
    close(g_term_pipe[1]);
    log_deinit();

    return 0;
}
