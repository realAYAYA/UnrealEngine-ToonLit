//
//  tentacleDevice.h
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
 @brief Tentacle device.
 
 Cached data of a discovered device.
 */

#ifndef tentacledevice_h
#define tentacledevice_h

#ifdef __cplusplus
extern "C" {
#endif
    
#include <stdio.h>
#include <Tentacle/TentacleTimecode.h>
#include <Tentacle/TentacleAdvertisement.h>
    
/// Maximum of timecode values to cache for each device
#define TENTACLE_DEVICE_TIMECODE_CACHE_SIZE_MAX 10

/// Maximum of received signal strength indicator (RSSI) values for each device
#define TENTACLE_DEVICE_RSSI_CACHE_SIZE_MAX 5

/// If a device has not been seen after the defined time span it is considered
/// disappeared.
#define TENTACLE_DEVICE_DISAPPEARED_THRESHOLD 10.0

/// If a device has not been seen after the defined time span it is considered
/// unavailable.
#define TENTACLE_DEVICE_UNAVAILABLE_THRESHOLD 600.0

/**
 Cached data of a discovered device.
 */
typedef struct TentacleDevice {
    /// Last received advertisement for this device
    struct TentacleAdvertisement advertisement;
    
    /// Ring buffer of cached timecodes
    TentacleTimecode timecodeCache[TENTACLE_DEVICE_TIMECODE_CACHE_SIZE_MAX];

    /// Current index of timecode in ring buffer
    size_t timecodeCacheIndex;
    
    /// Current size of timecode ring buffer
    size_t timecodeCacheSize;
    
    /// Averaged timecode of all timecodes from timecode cache.
    TentacleTimecode timecode;
    
    /// Ring buffer of received signal strength indication (RSSI) values
    int rssiCache[TENTACLE_DEVICE_RSSI_CACHE_SIZE_MAX];
    
    /// Next index in RSSI ring buffer to use
    size_t rssiCacheNextIndex;
    
    /// @private Used to calculate precise timecode
    int startPhaseCounter;
    
    /// @private Used to calculate precise timecode
    int asyncCounter;
    
    /// @private Used to calculate precise timecode
    int severeAsyncCounter;
} TentacleDevice;

/**
 Returns a device struct with default values.
 
 @return TentacleDevice struct with default values.
 */
TentacleDevice TentacleDeviceMake(void);
    
/**
 Update precise, calculated timecode for this device.
 
 @param device Update cached timecodes of this device.
 @param advertisementNew Add timecode from this newly received advertisement
 to cache.
 @return Difference between elapsed time according to received timecodes and
 local device time, in milliseconds.
 */
double TentacleDeviceUpdateTimecode(TentacleDevice *device, const TentacleAdvertisement *advertisementNew);

/**
 If a device has not been seen after `TENTACLE_DEVICE_DISAPPEARED_THRESHOLD` seconds
 it is considered disappeared.
 
 @param device Device to get disappeared status for.
 @param atTimestamp Get disappeared status for this timestamp, i.e. the current
 time since boot in seconds (see TentacleTimecode.receivedTimestamp for specific timestamp-source).
 @return True if device has not been seen for `TENTACLE_DEVICE_DISAPPEARED_THRESHOLD`
 seconds.
 */
bool TentacleDeviceIsDisappeared(TentacleDevice *device, double atTimestamp);

/**
 If a device has not been seen after `TENTACLE_DEVICE_UNAVAILABLE_THRESHOLD` seconds
 it is considered unavailable.

 @param device Device to get unavailable status for.
 @param atTimestamp Get unavailable status for this timestamp, i.e. the current
 time since boot in seconds (see TentacleTimecode.receivedTimestamp for specific timestamp-source).
 @return True if device has not been seen for `TENTACLE_DEVICE_UNAVAILABLE_THRESHOLD`
 seconds.
 */
bool TentacleDeviceIsUnavailable(TentacleDevice *device, double atTimestamp);

/**
 Add a received signal strength indicator (RSSI) value to the ring buffer.

 @param device Add to ring buffer of this device.
 @param rssi Add this RSSI value to ring buffer.
 */
void TentacleDeviceAddRssi(TentacleDevice *device, int rssi);

/**
 Averaged received signal strength indicator (RSSI) value.
 
 @param device Get averaged RSSI value for this device.
 @return Averaged RSSI value
 */
int TentacleDeviceGetRssi(TentacleDevice *device);
    
#ifdef __cplusplus
}
#endif

#endif /* tentacledevice_h */
