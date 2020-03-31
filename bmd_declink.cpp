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
    (void)videoFrame;
    (void)audioFrame;
    LOGLN0((LOG_INFO, LOGS, LOGP));
    return S_OK;
}

/******************************************************************************/
int
bmd_declink_create(void** obj)
{
    HRESULT result;
    struct bmd_declink* self;
    IDeckLink* deckLink;
    IDeckLinkAttributes* deckLinkAttributes;
    IDeckLinkDisplayModeIterator* displayModeIterator;
    IDeckLinkDisplayMode* displayMode;
    BMDDisplayMode dmode;
    bool formatDetectionSupported;
    char* displayModeName;
    DeckLinkCaptureDelegate* delegate1;
    IDeckLinkIterator* deckLinkIterator;

    self = (struct bmd_declink*)calloc(1, sizeof(struct bmd_declink));
    if (self == NULL)
    {
        return BMD_ERROR_MEMORY;
    }
    deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (deckLinkIterator == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "CreateDeckLinkIteratorInstance failed",
                LOGP));
        return 1;
    }
    deckLink = NULL;
    result = deckLinkIterator->Next(&deckLink);
    if ((result != S_OK) || (deckLink == NULL))
    {
        LOGLN0((LOG_ERROR, LOGS "Unable to get DeckLink device", LOGP));
        return 1;
    }
    deckLinkIterator->Release();
    result = deckLink->QueryInterface(IID_IDeckLinkInput,
                                      (void**)(&(self->deckLinkInput)));
    if (result != S_OK)
    {
        LOGLN0((LOG_ERROR, LOGS "IID_IDeckLinkInput failed", LOGP));
        return 1;
    }
    deckLinkAttributes = NULL;
    result = deckLink->QueryInterface(IID_IDeckLinkAttributes,
                                      (void**)(&deckLinkAttributes));
    if (result != S_OK)
    {
        LOGLN0((LOG_ERROR, LOGS "IID_IDeckLinkAttributes failed", LOGP));
        return 1;
    }
    result = deckLinkAttributes->GetFlag
        (BMDDeckLinkSupportsInputFormatDetection, &formatDetectionSupported);
    if (result == S_OK)
    {
        if (formatDetectionSupported)
        {
            LOGLN0((LOG_ERROR, LOGS "Format detection is supported on "
                    "this device", LOGP));
        }
    }
    result = self->deckLinkInput->GetDisplayModeIterator(&displayModeIterator);
    if (result != S_OK)
    {
        LOGLN0((LOG_ERROR, LOGS "GetDisplayModeIterator failed", LOGP));
        return 1;
    }
    displayMode = NULL;
    result = displayModeIterator->Next(&displayMode);
    while (result == S_OK)
    {
        result = displayMode->GetName((const char**)&displayModeName);
        if (result == S_OK)
        {
            LOGLN0((LOG_INFO, LOGS "name %s", LOGP, displayModeName));
            //if (strcmp(displayModeName, "NTSC") == 0)
            //if (strcmp(displayModeName, "HD 720p 60") == 0)
            //if (strcmp(displayModeName, "HD 1080i 59.94") == 0)
            //if (strcmp(displayModeName, "HD 1080p 23.98") == 0)
            //if (strcmp(displayModeName, "NTSC Progressive") == 0)
            //if (strcmp(displayModeName, "HD 720p 59.94") == 0)
            if (strcmp(displayModeName, "720p59.94") == 0)
            {
                break;
            }
            /*
            name NTSC                ok
            name NTSC 23.98          not ok
            name PAL                 not ok
            name NTSC Progressive    not ok
            name PAL Progressive     not ok
            name HD 1080p 23.98      not ok
            name HD 1080p 24         not ok
            name HD 1080p 25
            name HD 1080p 29.97
            name HD 1080p 30
            name HD 1080i 50
            name HD 1080i 59.94      ok
            * HD 720p 60
            */
        }
        else
        {
            LOGLN0((LOG_ERROR, LOGS "error getting name", LOGP));
        }
        displayMode->Release();
        result = displayModeIterator->Next(&displayMode);
    }
    result = displayMode->GetName((const char**)&displayModeName);
    if (result != S_OK)
    {
        displayModeName = (char *)malloc(32);
        snprintf(displayModeName, 32, "[index %d]", 0);
    }
    LOGLN0((LOG_INFO, LOGS "display mode %s", LOGP, displayModeName));
    delegate1 = new DeckLinkCaptureDelegate();
    self->deckLinkInput->SetCallback(delegate1);
    dmode = displayMode->GetDisplayMode();
    result = self->deckLinkInput->EnableVideoInput(dmode, bmdFormat8BitYUV,
                                                   bmdVideoInputFlagDefault);
    if (result != S_OK)
    { 
        LOGLN0((LOG_ERROR, LOGS "EnableVideoInput failed", LOGP));
        return 1;
    }
    result = self->deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz,
                                                   16, 2);
    if (result != S_OK)
    {
        LOGLN0((LOG_ERROR, LOGS "EnableAudioInput failed", LOGP));
        return 1;
    }
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

