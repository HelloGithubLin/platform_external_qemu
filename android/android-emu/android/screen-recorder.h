// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "android/utils/compiler.h"
#include "android/utils/looper.h"

#include <stdbool.h>

ANDROID_BEGIN_HEADER
// This callback will be called in the following scenarios:
//
//    1) The encoding has been stopped
//    2) The encoding is finished
//    3) An error has occurred while encoding was trying to finish.
//
// When screen_recorder_stop_async() is called, this callback will get called,
// with success set to 0. There is some time elapsed when we want to stop
// recording and when the encoding is actually finished, so we'll get a second
// call to the callback once the encoding is finished, with success set to 1. If
// any errors occur while stopping the recording, success will be set to -1.
typedef enum {
    RECORD_STOP_INITIATED,
    RECORD_STOP_FINISHED,
    RECORD_STOP_FAILED,
} RecordStopStatus;

typedef void (*RecordingStoppedCallback)(void* opaque, RecordStopStatus status);

typedef struct RecordingInfo {
    const char* fileName;
    uint32_t width;
    uint32_t height;
    uint32_t videoBitrate;
    uint32_t timeLimit;
    RecordingStoppedCallback cb;
    void* opaque;
} RecordingInfo;

// Initializes internal global structure. Call this before doing any recording
// operations. |w| and |h| are the FrameBuffer width and height.
extern void screen_recorder_init(bool isGuestMode, int w, int h);
// Starts recording the screen. When stopped, the file will be saved as
// |info->filename|. Returns true if recorder started recording, false if it
// failed.
extern int screen_recorder_start(const RecordingInfo* info);
// Stop recording. After calling this function, the encoder will stop processing
// frames. The encoder still needs to process any remaining frames it has, so
// calling this does not mean that the encoder has finished and |filename| is
// ready. Attach a RecordingStoppedCallback to get an update when the encoder
// has finished.
extern void screen_recorder_stop_async(void);
ANDROID_END_HEADER


