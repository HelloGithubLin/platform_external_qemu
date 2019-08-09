// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "android/hw-sensors.h"
#include "android/mp4/MP4Dataset.h"
#include "android/mp4/SensorLocationEventProvider.h"
#include "android/recording/video/player/PacketQueue.h"
#include "android/recording/video/player/VideoPlayer.h"
#include "android/recording/video/player/VideoPlayerWaitInfo.h"

using android::videoplayer::PacketQueue;
using android::videoplayer::VideoPlayerWaitInfo;

namespace android {
namespace mp4 {

// The MP4 demultiplexer (a.k.a. demuxer) extract packets from an MP4
// input file and dispatch these packets to output corresponding to
// their stream index.
//
// A typical MP4 file has audio/video streams. Some might have other data
// streams.
class Mp4Demuxer {
public:
    virtual ~Mp4Demuxer() {};
    static std::unique_ptr<Mp4Demuxer> create(
            ::android::videoplayer::VideoPlayer* player,
            Mp4Dataset* dataset,
            VideoPlayerWaitInfo* readingWaitInfo);

    // Reads the next packet from the MP4 file, and puts the packet into its
    // corresponding PacketQueue if it is from audio/video stream, or creates
    // an event if it is from a known data stream that carries event info.
    //
    // When EOF is reached, puts a null packet in each PacketQueue if the video
    // player is not in looping mode, or seeks back to the start of the file if
    // the video player is in looping mode.
    //
    // Returns 0 on success, -1 on error.
    virtual int demuxNextPacket() = 0;

    // Seeks the MP4 file to timestamp and flushes packet queues.
    //
    // Parameter: timestamp is an absolute offset from the start of a video
    //            measured in seconds.
    virtual void seek(double timestamp) = 0;

    virtual void setAudioPacketQueue(PacketQueue* audioPacketQueue) = 0;
    virtual void setVideoPacketQueue(PacketQueue* videoPacketQueue) = 0;
    virtual void setSensorLocationEventProvider(
            std::shared_ptr<SensorLocationEventProvider> eventProvider) = 0;

protected:
    Mp4Demuxer() = default;
};

}  // namespace mp4
}  // namespace android
