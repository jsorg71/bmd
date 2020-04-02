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

#ifdef __cplusplus
extern "C"
{
#endif

int
bmd_declink_create(struct bmd_info* bmd, void** obj);
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

