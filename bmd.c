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

#include "bmd_declink.h"
#include "bmd_log.h"

/*****************************************************************************/
int
main(int argc, char** argv)
{
    void* bmd_declink;

    (void)argc;
    (void)argv;

    log_init(LOG_FLAG_STDOUT, 4, "");

    if (bmd_declink_create(&bmd_declink) == 0)
    {
        bmd_declink_start(bmd_declink);
        usleep(3 * 1024 * 1024);
        bmd_declink_stop(bmd_declink);
        bmd_declink_delete(bmd_declink);
    }

    log_deinit();

    return 0;
}
