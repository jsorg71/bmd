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

#ifndef _BMD_LOG_H_
#define _BMD_LOG_H_

#define LOG_FLAG_FILE   1
#define LOG_FLAG_STDOUT 2

#define LOG_ERROR   0
#define LOG_WARN    1
#define LOG_INFO    2
#define LOG_DEBUG   3

#define LOGS "[%s][%d][%s]:"
#define LOGP __FILE__, __LINE__, __FUNCTION__

#if !defined(__FUNCTION__) && defined(__FUNC__)
#define LOG_PRE const char* __FUNCTION__ = __FUNC__; (void)__FUNCTION__;
#else
#define LOG_PRE
#endif

#define LOG_LEVEL 1
#if LOG_LEVEL > 0
#define LOGLN0(_args) do { LOG_PRE logln _args ; } while (0)
#else
#define LOGLN0(_args)
#endif
#if LOG_LEVEL > 10
#define LOGLN10(_args) do { LOG_PRE logln _args ; } while (0)
#else
#define LOGLN10(_args)
#endif

#ifdef __GNUC__
#define PRINTFLIKE(_n, _m) __attribute__((format(printf, _n, _m)))
#else
#define PRINTFLIKE(_n, _m)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int
log_init(int flags, int log_level, const char* filename);
int
log_deinit(void);
int
logln(int log_level, const char* format, ...) PRINTFLIKE(2, 3);

#ifdef __cplusplus
}
#endif

#endif
