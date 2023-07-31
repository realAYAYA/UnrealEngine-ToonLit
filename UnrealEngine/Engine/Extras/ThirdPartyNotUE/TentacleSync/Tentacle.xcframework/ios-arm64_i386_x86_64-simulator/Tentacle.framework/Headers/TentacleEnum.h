//
//  tentacleEnum.h
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
 @brief Tentacle enumerations.
 */

#ifndef tentacleenum_h
#define tentacleenum_h

#ifdef TentacleAdvertisement
extern "C" {
#endif
    
/**
 State the device is in.
 */
enum tentacleDeviceMode {
    /// Device is powered off.
    power_off = 0,
    
    /// Device is booting.
    startup = 1,
    
    /// Device is in standby.
    standby = 2,
    
    /// No cable is plugged into the device, no audio timecode is generated.
    unplugged = 3,
    
    /// Cable is plugged in and device is generating timecode.
    generateTimecode = 4,
    
    /// Device is reading timecode.
    readTimecode = 5,
    
    /// Device is stopped.
    stopped = 6
};

/**
 When producing timecode to be recorded on an audio track, the timecode is
 encoded as an audio signal using a certain volume. This volume can be
 low (Mic), high (Line), something in between (Custom) or be automatically
 detected (AutoMic).
 */
enum TentacleAudioLevel {
    /// Audio timecode is produced at a low volume
    mic = 0,
    
    /// Audio timecode is produced at a high volume
    line = 1 ,
    
    /// Audio timecode is produced at a medium volume
    custom = 2,
    
    /// Audio timecode is automatically set to a low volume
    autoMic = 3
};

#endif /* tentacleenum_h */
