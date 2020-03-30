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
#include <pthread.h>

#include <DeckLinkAPI.h>

#include "bmd_declink.h"

class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
    public:
        DeckLinkCaptureDelegate(void);
        ~DeckLinkCaptureDelegate(void);
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);
        virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents,
                                                                  IDeckLinkDisplayMode*,
                                                                  BMDDetectedVideoInputFormatFlags);
        virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*,
                                                                 IDeckLinkAudioInputPacket*);
    private:
        ULONG m_refCount;
        pthread_mutex_t m_mutex;
};

/******************************************************************************/
DeckLinkCaptureDelegate::DeckLinkCaptureDelegate(void)
{
    printf("DeckLinkCaptureDelegate:\n");
    m_refCount = 0;
    pthread_mutex_init(&m_mutex, NULL);
}

/******************************************************************************/
DeckLinkCaptureDelegate::~DeckLinkCaptureDelegate(void)
{
    printf("~DeckLinkCaptureDelegate:\n");
    pthread_mutex_destroy(&m_mutex);
}

/******************************************************************************/
HRESULT DeckLinkCaptureDelegate::QueryInterface(REFIID iid, LPVOID *ppv)
{
    (void)iid;
    (void)ppv;
    printf("QueryInterface:\n");
    return E_NOINTERFACE;
}

/******************************************************************************/
ULONG DeckLinkCaptureDelegate::AddRef(void)
{
    printf("AddRef:\n");
    pthread_mutex_lock(&m_mutex);
    m_refCount++;
    pthread_mutex_unlock(&m_mutex);
    return (ULONG)m_refCount;
}

/******************************************************************************/
ULONG DeckLinkCaptureDelegate::Release(void)
{
    printf("Release:\n");
    pthread_mutex_lock(&m_mutex);
    m_refCount--;
    pthread_mutex_unlock(&m_mutex);
    if (m_refCount == 0)
    {
        delete this;
        return 0;
    }
    return (ULONG)m_refCount;
}

/******************************************************************************/
HRESULT DeckLinkCaptureDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events,
                                                         IDeckLinkDisplayMode *mode,
                                                         BMDDetectedVideoInputFormatFlags flags)
{
    (void)events;
    (void)mode;
    (void)flags;
    printf("VideoInputFormatChanged:\n");
    return S_OK;
}

/******************************************************************************/
HRESULT DeckLinkCaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame,
                                                        IDeckLinkAudioInputPacket* audioFrame)
{
    (void)videoFrame;
    (void)audioFrame;
    printf("VideoInputFrameArrived:\n");
    return S_OK;
}
