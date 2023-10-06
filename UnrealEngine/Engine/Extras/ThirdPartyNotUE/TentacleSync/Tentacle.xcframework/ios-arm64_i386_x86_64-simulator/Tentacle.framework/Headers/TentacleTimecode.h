//
//  tentacleTimecode.h
//  Tentacle-SDK
//
//  Copyright 2019 Tentacle Sync GmbH
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//

/**
 @file
 @brief Tentacle timecode.
 
 Contains cached raw data of a received timecode as received from a Bluetooth
 advertisement.
 */

#ifndef tentacletimecode_h
#define tentacletimecode_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>   // for size_t
#include <stdint.h>  // for uint8_t
#include <stdbool.h> // for bool
    
/// Length of string returned by `tentacleTimecodeCacheGetString`.
#define TENTACLE_TIMECODE_STRING_LENGTH 35
    
/// 23.98 FPS =  24 FPS NTSC
#define TENTACLE_FPS_23_98 (24000.0/1001.0)
    
/// 29.97 FPS = 30 FPS NTSC
#define TENTACLE_FPS_29_97 (30000.0/1001.0)

/**
 Contains cached raw data of a received timecode as received from a Bluetooth
 advertisement.
 */
typedef struct TentacleTimecode {
    /// Hour of timecode.
    uint8_t hour;
    
    /// Minute of timecode.
    uint8_t minute;
    
    /// Second of timecode.
    uint8_t second;
    
    /// Frame of timecode.
    uint8_t frame;
    
    /// Microseconds since start of last frame.
    uint16_t microsecond;
    
    /// \brief Timestamp of when this timecode has been received, in seconds.
    ///
    /// On Apple Darwin, [machAbsoluteTime](https://github.com/apple/darwin-xnu/blob/0a798f6738bc1db01281fc08ae024145e84df927/osfmk/arm/rtclock.c#L162) is used.
    ///
    /// On Android, [ScanResult.getTimestampNanos()](https://developer.android.com/reference/android/bluetooth/le/ScanResult.html#getTimestampNanos()) is used.
    ///
    /// Caution: Due to maximum precision these Timestamps are based on the boot-time of the device. According to this, the timestamps are invalid after
    /// a device reboot and should not be used for long time comparison or alike.
    double receivedTimestamp;
} TentacleTimecode;
    
/**
 Returns a timecode struct with default values.

 @return TentacleTimecode struct with default values.
 */
TentacleTimecode TentacleTimecodeMake(void);
    
/**
 Check if two timecodes are equal.

 @param timecode1 First timecode to compare.
 @param timecode2 Second timecode to compare.
 @return True if timecodes are equal, false otherwise.
 */
bool TentacleTimecodeEqual(const TentacleTimecode *timecode1, const TentacleTimecode *timecode2);

/**
 Total number of seconds described by this timecode at specific timestamp.

 @param timecode Get total number of seconds for this timecode.
 @param frameRate Frame rate used to calculate number of seconds.
 @param dropFrame True if timecode is using drop frames.
 @param atTimestamp Total number of seconds of timecode at this timestamp.
 @return Frame rate used to calculate duration of frame count at timestamp.
 */
double TentacleTimecodeSecondsAtTimestamp(const TentacleTimecode *timecode, double frameRate, bool dropFrame, double atTimestamp);
    
/**
 Create a timecode from a total number of seconds.

 @param seconds Convert this total number of seconds to timecode.
 @param frameRate Frame rate used during conversion.
 @param dropFrame True if timecode is using drop frames.
 @param receivedTimestamp Use this as received timestamp.
 @return New timecode struct containing the passed total number of seconds.
 */
TentacleTimecode TentacleTimecodeFromSeconds(double seconds, double frameRate, bool dropFrame, double receivedTimestamp);

/**
 Given a received timecode, calculate what its value would be at a different
 point in time.

 @param timecode Get this timecode for a specific point in time
 @param frameRate Frame rate of timecode
 @param dropFrame True if timecode is using drop frames.
 @param atTimestamp Requested point in time
 @return Timecode value at specific point in time.
 */
TentacleTimecode TentacleTimecodeAtTimestamp(TentacleTimecode timecode, double frameRate, bool dropFrame, double atTimestamp);

/**
 Get timecode string representing a timecode.
 
 @param timecode Get string value representing this timecode struct.
 @param dropFrame True if timecode is drop frame.
 @param timecodeCacheString Resulting string value. Has to be of length
        `TENTACLE_TIMECODE_STRING_LENGTH`.
 @return Number of characters of `timecodeCacheString`, not including the
         terminating null character.
 */
int TentacleTimecodeString(TentacleTimecode timecode, bool dropFrame, char *timecodeCacheString);

/**
 Get timecode string representing a timecode, including microseconds since last
 frame and received timestamp.
 
 @param timecode Get string value representing this timecode struct.
 @param dropFrame True if timecode is drop frame.
 @param timecodeCacheString Resulting string value. Has to be of length
        `TENTACLE_TIMECODE_STRING_LENGTH`.
 @return Number of characters of `timecodeCacheString`, not including the
         terminating null character.
 */
int TentacleTimecodeStringVerbose(TentacleTimecode timecode, bool dropFrame, char *timecodeCacheString);

#ifdef __cplusplus
}
#endif

#endif /* tentacletimecode_h */
