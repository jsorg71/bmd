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

#include "bmd.h"
#include "bmd_declink.h"
#include "bmd_error.h"
#include "bmd_log.h"

class DeckLinkCaptureDelegate : public IDeckLinkInputCallback
{
    public:
        DeckLinkCaptureDelegate(void);
        ~DeckLinkCaptureDelegate(void);
        virtual HRESULT STDMETHODCALLTYPE
            QueryInterface(REFIID iid, LPVOID *ppv);
        virtual ULONG STDMETHODCALLTYPE AddRef(void);
        virtual ULONG STDMETHODCALLTYPE Release(void);
        virtual HRESULT STDMETHODCALLTYPE
            VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events,
                                    IDeckLinkDisplayMode* mode,
                                    BMDDetectedVideoInputFormatFlags flags);
        virtual HRESULT STDMETHODCALLTYPE
            VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame,
                                   IDeckLinkAudioInputPacket* audioFrame);
    private:
        ULONG m_refCount;
        pthread_mutex_t m_mutex;
    public:
        struct bmd_info* m_bmd;
};

struct bmd_declink
{
    IDeckLinkInput* deckLinkInput;
};

/******************************************************************************/
DeckLinkCaptureDelegate::DeckLinkCaptureDelegate(void)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    m_refCount = 0;
    pthread_mutex_init(&m_mutex, NULL);
}

/******************************************************************************/
DeckLinkCaptureDelegate::~DeckLinkCaptureDelegate(void)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    pthread_mutex_destroy(&m_mutex);
}

/******************************************************************************/
HRESULT DeckLinkCaptureDelegate::QueryInterface(REFIID iid, LPVOID* ppv)
{
    (void)iid;
    (void)ppv;
    LOGLN0((LOG_INFO, LOGS, LOGP));
    return E_NOINTERFACE;
}

/******************************************************************************/
ULONG DeckLinkCaptureDelegate::AddRef(void)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    pthread_mutex_lock(&m_mutex);
    m_refCount++;
    pthread_mutex_unlock(&m_mutex);
    return m_refCount;
}

/******************************************************************************/
ULONG DeckLinkCaptureDelegate::Release(void)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
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
HRESULT DeckLinkCaptureDelegate::
    VideoInputFormatChanged(BMDVideoInputFormatChangedEvents events,
                            IDeckLinkDisplayMode* mode,
                            BMDDetectedVideoInputFormatFlags flags)
{
    (void)events;
    (void)mode;
    (void)flags;
    LOGLN0((LOG_INFO, LOGS, LOGP));
    return S_OK;
}

/******************************************************************************/
HRESULT DeckLinkCaptureDelegate::
    VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame,
                           IDeckLinkAudioInputPacket* audioFrame)
{
    void* audio_data;
    long audio_frame_count;

    void* video_data;
    int video_width;
    int video_height;

    LOGLN0((LOG_INFO, LOGS, LOGP));
    if (videoFrame != NULL)
    {
        video_data = NULL;
        videoFrame->GetBytes(&video_data);
        video_width = videoFrame->GetWidth();
        video_height = videoFrame->GetHeight();
        LOGLN0((LOG_INFO, LOGS "video_data %p video_width %d video_height %d",
                LOGP, video_data, video_width, video_height));
    }
    if (audioFrame != NULL)
    {
        audio_data = NULL;
        audioFrame->GetBytes(&audio_data);
        audio_frame_count = audioFrame->GetSampleFrameCount();
        LOGLN0((LOG_INFO, LOGS "audio_data %p audio_frame_count %d",
                LOGP, audio_data, audio_frame_count));
    }
    return S_OK;
}

/******************************************************************************/
static IDeckLink*
bmd_declink_get_IDeckLink(IDeckLinkIterator* deckLinkIterator)
{
    IDeckLink* deckLink;

    for (;;)
    {
        if (deckLinkIterator->Next(&deckLink) != S_OK)
        {
            break;
        }
        if (deckLink != NULL)
        {
            return deckLink;
        }
    }
    return NULL;
}

/******************************************************************************/
static IDeckLinkInput*
bmd_declink_get_IDeckLinkInput(IDeckLink* deckLink)
{
    void* rv;
    HRESULT result;

    result = deckLink->QueryInterface(IID_IDeckLinkInput, &rv);
    if (result == S_OK)
    {
        return (IDeckLinkInput*)rv;
    }
    return NULL;
}

/******************************************************************************/
static IDeckLinkDisplayMode*
bmd_declink_get_IDeckLinkDisplayMode(IDeckLinkInput* deckLinkInput)
{
    IDeckLinkDisplayModeIterator* displayModeIterator;
    IDeckLinkDisplayMode* displayMode;
    const char* displayModeName;

    if (deckLinkInput->GetDisplayModeIterator(&displayModeIterator) != S_OK)
    {
        return NULL;
    }
    for (;;)
    {
        if (displayModeIterator->Next(&displayMode) != S_OK)
        {
            break;
        }
        if (displayMode->GetName(&displayModeName) != S_OK)
        {
            displayMode->Release();
            break;
        }
        if (strcmp(displayModeName, "720p59.94") == 0)
        {
            delete displayModeName;
            displayModeIterator->Release();
            return displayMode;
        }
        delete displayModeName;
        displayMode->Release();
    }
    displayModeIterator->Release();
    return NULL;
}

/******************************************************************************/
int
bmd_declink_create(struct bmd_info* bmd, void** obj)
{
    HRESULT result;
    struct bmd_declink* self;
    IDeckLink* deckLink;
    IDeckLinkDisplayMode* displayMode;
    BMDDisplayMode dmode;
    const char* modelName;
    const char* displayName;
    DeckLinkCaptureDelegate* myDelegate;
    IDeckLinkIterator* deckLinkIterator;
    IDeckLinkInput* deckLinkInput;

    deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (deckLinkIterator == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "CreateDeckLinkIteratorInstance failed",
                LOGP));
        return 1;
    }
    deckLink = bmd_declink_get_IDeckLink(deckLinkIterator);
    deckLinkIterator->Release();
    if (deckLink == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "bmd_declink_get_IDeckLink failed", LOGP));
        return 1;
    }
    deckLink->GetModelName(&modelName);
    deckLink->GetDisplayName(&displayName);
    LOGLN0((LOG_INFO, LOGS "deckLink %p modelName [%s] displayName [%s]",
            LOGP, deckLink, modelName, displayName));
    delete modelName;
    delete displayName;
    deckLinkInput = bmd_declink_get_IDeckLinkInput(deckLink);
    deckLink->Release();
    if (deckLinkInput == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "bmd_declink_get_IDeckLinkInput failed",
                LOGP));
        return 1;
    }
    displayMode = bmd_declink_get_IDeckLinkDisplayMode(deckLinkInput);
    if (displayMode == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "bmd_declink_get_IDeckLinkDisplayMode failed",
                LOGP));
        deckLinkInput->Release();
        return 1;

    }
    dmode = displayMode->GetDisplayMode();
    displayMode->Release();
    myDelegate = new DeckLinkCaptureDelegate();
    myDelegate->m_bmd = bmd;
    deckLinkInput->SetCallback(myDelegate);
    result = deckLinkInput->EnableVideoInput(dmode, bmdFormat8BitYUV,
                                             bmdVideoInputFlagDefault);
    if (result != S_OK)
    { 
        LOGLN0((LOG_ERROR, LOGS "EnableVideoInput failed", LOGP));
        deckLinkInput->Release();
        return 1;
    }
    result = deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz,
                                             bmdAudioSampleType16bitInteger,
                                             2);
    if (result != S_OK)
    {
        LOGLN0((LOG_ERROR, LOGS "EnableAudioInput failed", LOGP));
        deckLinkInput->Release();
        return 1;
    }
    self = (struct bmd_declink*)calloc(1, sizeof(struct bmd_declink));
    self->deckLinkInput = deckLinkInput;
    *obj = self;
    return BMD_ERROR_NONE;
}

/******************************************************************************/
int
bmd_declink_delete(void* obj)
{
    struct bmd_declink* self;

    self = (struct bmd_declink*)obj;
    if (self != NULL)
    {
        self->deckLinkInput->Release();
        free(self);
    }
    return BMD_ERROR_NONE;
}

/******************************************************************************/
int
bmd_declink_start(void* obj)
{
    HRESULT result;
    struct bmd_declink* self;

    LOGLN0((LOG_INFO, LOGS, LOGP));
    self = (struct bmd_declink*)obj;
    result = self->deckLinkInput->StartStreams();
    if (result != S_OK)
    {
        return 1;
    }
    return BMD_ERROR_NONE;
}

/******************************************************************************/
int
bmd_declink_stop(void* obj)
{
    HRESULT result;
    struct bmd_declink* self;

    LOGLN0((LOG_INFO, LOGS, LOGP));
    self = (struct bmd_declink*)obj;
    result = self->deckLinkInput->StopStreams();
    if (result != S_OK)
    {
        return 1;
    }
    return BMD_ERROR_NONE;
}

