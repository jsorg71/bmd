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
#include <pthread.h>
#include <stdint.h>

#include <DeckLinkAPI.h>
#include <DeckLinkAPIVersion.h>

#include "bmd.h"
#include "bmd_declink.h"
#include "bmd_error.h"
#include "bmd_log.h"
#include "bmd_utils.h"

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
        struct bmd_av_info* m_av_info;
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
HRESULT
DeckLinkCaptureDelegate::QueryInterface(REFIID iid, LPVOID* ppv)
{
    (void)iid;
    (void)ppv;
    LOGLN0((LOG_INFO, LOGS, LOGP));
    return E_NOINTERFACE;
}

/******************************************************************************/
ULONG
DeckLinkCaptureDelegate::AddRef(void)
{
    LOGLN0((LOG_INFO, LOGS, LOGP));
    pthread_mutex_lock(&m_mutex);
    m_refCount++;
    pthread_mutex_unlock(&m_mutex);
    return m_refCount;
}

/******************************************************************************/
ULONG
DeckLinkCaptureDelegate::Release(void)
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
HRESULT
DeckLinkCaptureDelegate::
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
HRESULT
DeckLinkCaptureDelegate::
    VideoInputFrameArrived(IDeckLinkVideoInputFrame* videoFrame,
                           IDeckLinkAudioInputPacket* audioFrame)
{
    void* audio_data;
    long audio_frame_count;
    void* video_data;
    int video_width;
    int video_height;
    int stride_bytes;
    struct bmd_av_info* av_info;
    int do_sig;
    int bytes;
    int now;

    LOGLN10((LOG_INFO, LOGS "videoFrame %p audioFrame %p", LOGP,
             videoFrame, audioFrame));
    if (get_mstime(&now) != BMD_ERROR_NONE)
    {
        return S_OK;
    }
    do_sig = 0;
    av_info = m_av_info;
    pthread_mutex_lock(&(av_info->av_mutex));
    if ((videoFrame != NULL) && (!(av_info->got_video)))
    {
        video_data = NULL;
        videoFrame->GetBytes(&video_data);
        video_width = videoFrame->GetWidth();
        video_height = videoFrame->GetHeight();
        stride_bytes = videoFrame->GetRowBytes();
        LOGLN10((LOG_INFO, LOGS "video_data %p video_width %d video_height %d "
                 "stride_bytes %d", LOGP, video_data,
                 video_width, video_height, stride_bytes));
        bytes = stride_bytes * video_height;
        if (bytes > av_info->vdata_alloc_bytes)
        {
            LOGLN0((LOG_INFO, LOGS "free, alloc vdata old %d new %d", LOGP,
                    av_info->vdata_alloc_bytes, bytes));
            free(av_info->vdata);
            av_info->vdata = xnew(char, bytes);
            av_info->vdata_alloc_bytes = av_info->vdata == NULL ? 0 : bytes;
        }
        if (av_info->vdata != NULL)
        {
            av_info->vformat = 0;
            av_info->vwidth = video_width;
            av_info->vheight = video_height;
            av_info->vstride_bytes = stride_bytes;
            av_info->vtime = now;
            memcpy(av_info->vdata, video_data, bytes);
            av_info->got_video = 1;
            do_sig = 1;
        }
    }
    if ((audioFrame != NULL) && (!(av_info->got_audio)))
    {
        audio_data = NULL;
        audioFrame->GetBytes(&audio_data);
        audio_frame_count = audioFrame->GetSampleFrameCount();
        LOGLN10((LOG_INFO, LOGS "audio_data %p audio_frame_count %d",
                 LOGP, audio_data, (int)audio_frame_count));
        bytes = audio_frame_count * 2 * 2;
        if (bytes > av_info->adata_alloc_bytes)
        {
            LOGLN0((LOG_INFO, LOGS "free, alloc adata old %d new %d", LOGP,
                    av_info->adata_alloc_bytes, bytes));
            free(av_info->adata);
            av_info->adata = xnew(char, bytes);
            av_info->adata_alloc_bytes = av_info->adata == NULL ? 0 : bytes;
        }
        if (av_info->adata != NULL)
        {
            av_info->aformat = 0;
            av_info->achannels = 2;
            av_info->abytes_per_sample = 2;
            av_info->asamples = audio_frame_count;
            av_info->atime = now;
            memcpy(av_info->adata, audio_data, bytes);
            av_info->got_audio = 1;
            do_sig = 1;
        }
    }
    pthread_mutex_unlock(&(av_info->av_mutex));
    if (do_sig)
    {
        if (write(av_info->av_pipe[1], "sig", 4) != 4)
        {
            LOGLN0((LOG_ERROR, LOGS "write failed", LOGP));
        }
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
        if (FAILED(deckLinkIterator->Next(&deckLink)))
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
    if (SUCCEEDED(result))
    {
        return (IDeckLinkInput*)rv;
    }
    return NULL;
}

/******************************************************************************/
static IDeckLinkDisplayMode*
bmd_declink_get_IDeckLinkDisplayMode(int mode_index,
                                     IDeckLinkInput* deckLinkInput)
{
    IDeckLinkDisplayModeIterator* displayModeIterator;
    IDeckLinkDisplayMode* displayMode;
    const char* displayModeName;
    const char* requested_mode_name;

    if (FAILED(deckLinkInput->GetDisplayModeIterator(&displayModeIterator)))
    {
        return NULL;
    }
    requested_mode_name = g_mode_names[mode_index];
    LOGLN0((LOG_INFO, LOGS "requested_mode_name %s", LOGP,
            requested_mode_name));
    for (;;)
    {
        if (FAILED(displayModeIterator->Next(&displayMode)))
        {
            break;
        }
        if (FAILED(displayMode->GetName(&displayModeName)))
        {
            displayMode->Release();
            break;
        }
        LOGLN10((LOG_ERROR, LOGS "displayModeName %s", LOGP, displayModeName));
        if (strcmp(displayModeName, requested_mode_name) == 0)
        {
            free((void*)displayModeName); /* yup, the API needs cast */
            displayModeIterator->Release();
            return displayMode;
        }
        free((void*)displayModeName); /* yup, the API needs cast */
        displayMode->Release();
    }
    displayModeIterator->Release();
    return NULL;
}

/******************************************************************************/
int
bmd_declink_create(int mode_index, struct bmd_av_info* av_info, void** obj)
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
    int ctversion;
    int rtversion;
    int64_t i64;
    IDeckLinkAPIInformation* deckLinkAPIInfo;

    deckLinkAPIInfo = CreateDeckLinkAPIInformationInstance();
    if (deckLinkAPIInfo != NULL)
    {
        result = deckLinkAPIInfo->GetInt(BMDDeckLinkAPIVersion, &i64);
        if (SUCCEEDED(result))
        {
            ctversion = BLACKMAGIC_DECKLINK_API_VERSION;
            rtversion = (int)i64;
            LOGLN0((LOG_INFO, LOGS "compile time version %d.%d.%d "
                    "run time version %d.%d.%d", LOGP,
                    (ctversion >> 24) & 0xFF,
                    (ctversion >> 16) & 0xFF,
                    (ctversion >> 8) & 0xFF,
                    (rtversion >> 24) & 0xFF,
                    (rtversion >> 16) & 0xFF,
                    (rtversion >> 8) & 0xFF));
        }
        else
        {
            LOGLN0((LOG_ERROR, LOGS "BMDDeckLinkAPIVersion failed", LOGP));
        }
        deckLinkAPIInfo->Release();
    }
    else
    {
        LOGLN0((LOG_ERROR, LOGS "CreateDeckLinkAPIInformationInstance "
                "failed", LOGP));
    }
    deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (deckLinkIterator == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "CreateDeckLinkIteratorInstance failed",
                LOGP));
        return BMD_ERROR_DECKLINK;
    }
    deckLink = bmd_declink_get_IDeckLink(deckLinkIterator);
    deckLinkIterator->Release();
    if (deckLink == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "bmd_declink_get_IDeckLink failed", LOGP));
        return BMD_ERROR_DECKLINK;
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
        return BMD_ERROR_DECKLINK;
    }
    displayMode = bmd_declink_get_IDeckLinkDisplayMode(mode_index, deckLinkInput);
    if (displayMode == NULL)
    {
        LOGLN0((LOG_ERROR, LOGS "bmd_declink_get_IDeckLinkDisplayMode failed",
                LOGP));
        deckLinkInput->Release();
        return BMD_ERROR_DECKLINK;

    }
    dmode = displayMode->GetDisplayMode();
    displayMode->Release();
    myDelegate = new DeckLinkCaptureDelegate();
    myDelegate->m_av_info = av_info;
    deckLinkInput->SetCallback(myDelegate);
    result = deckLinkInput->EnableVideoInput(dmode, bmdFormat8BitYUV,
                                             bmdVideoInputFlagDefault);
    if (FAILED(result))
    {
        LOGLN0((LOG_ERROR, LOGS "EnableVideoInput failed result 0x%x",
                LOGP, result));
        deckLinkInput->Release();
        return BMD_ERROR_DECKLINK;
    }
    result = deckLinkInput->EnableAudioInput(bmdAudioSampleRate48kHz,
                                             bmdAudioSampleType16bitInteger,
                                             2);
    if (FAILED(result))
    {
        LOGLN0((LOG_ERROR, LOGS "EnableAudioInput failed result 0x%8.8x",
                LOGP, result));
        deckLinkInput->Release();
        return BMD_ERROR_DECKLINK;
    }
    self = xnew0(struct bmd_declink, 1);
    if (self == NULL)
    {
        deckLinkInput->Release();
        return BMD_ERROR_MEMORY;
    }
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
    if (self == NULL)
    {
        return BMD_ERROR_NONE;
    }
    self->deckLinkInput->Release();
    free(self);
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
    if (FAILED(result))
    {
        return BMD_ERROR_START;
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
    if (FAILED(result))
    {
        return BMD_ERROR_STOP;
    }
    return BMD_ERROR_NONE;
}

