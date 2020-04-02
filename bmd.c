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
    int pad0;
};

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
    if (argc < 1)
    {
        return BMD_ERROR_NONE;
    }
    printf("%s: command line options\n", argv[0]);
    printf("    -D      run daemon, example -D\n");
    printf("    -n      uds name, %%d will be pid, example -n %s\n", BMD_UDS);
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_cleanup(struct bmd_info* bmd)
{
    if (bmd->yami != NULL)
    {
        //yami_decoder_delete(hdhrd->yami);
        bmd->yami = NULL;
    }
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
    return BMD_ERROR_NONE;
}

/*****************************************************************************/
static int
bmd_stop(struct bmd_info* bmd)
{
    bmd_cleanup(bmd);
    LOGLN0((LOG_INFO, LOGS "bmd_cleanup called", LOGP));
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

    rv = BMD_ERROR_NONE;
    for (;;)
    {
        max_fd = bmd->listener;
        if (g_term_pipe[0] > max_fd)
        {
            max_fd = g_term_pipe[0];
        }
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(bmd->listener, &rfds);
        FD_SET(g_term_pipe[0], &rfds);
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
    void* bmd_declink;
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
        //close(bmd->listener);
        free(settings);
        free(bmd);
        return 1;
    }
    signal(SIGINT, sig_int);
    signal(SIGTERM, sig_int);
    signal(SIGPIPE, sig_pipe);

    if (bmd_declink_create(bmd, &bmd_declink) == 0)
    {
        bmd_declink_start(bmd_declink);
        usleep(3 * 1024 * 1024);
        bmd_declink_stop(bmd_declink);
        bmd_declink_delete(bmd_declink);
    }

    close(bmd->listener);
    unlink(settings->bmd_uds);

    yami_deinit();
    close(bmd->yami_fd);
    free(bmd);
    free(settings);
    close(g_term_pipe[0]);
    close(g_term_pipe[1]);
    log_deinit();

    return 0;
}
