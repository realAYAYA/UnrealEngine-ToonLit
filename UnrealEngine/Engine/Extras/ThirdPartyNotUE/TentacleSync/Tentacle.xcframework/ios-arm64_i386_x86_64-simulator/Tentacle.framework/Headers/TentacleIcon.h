//
//  TentacleIcon.h
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

@import Foundation;

#if TARGET_OS_IOS
@import UIKit;
#endif

NS_ASSUME_NONNULL_BEGIN

@interface TentacleIcon: NSObject

#if TARGET_OS_IOS

/// Get Tentacle device icon image.
/// \param index Icon index of range 0 to 37
///
/// returns:
/// Device image
+ (UIImage * _Nonnull)tentacleIconWithIndex:(NSUInteger)index;

/// Get default battery image.
/// \param percent Percentage of battery status
///
/// returns:
/// Battery image
+ (UIImage * _Nonnull)tentacleBatteryIconForPercent:(NSUInteger)percent;

/// Get default signal strength image.
/// \param index Possible values are 0 to 3
///
/// returns:
/// Signal strength image
+ (UIImage * _Nonnull)tentacleSignalStrengthIconWithIndex:(NSUInteger)index;

/// Get default signal strength image for a given received signal strength indicator (RSSI) value.
/// \param rssiValue Get image corresponding to this RSSI
/// \param linkMode Internal use only, always pass false
///
/// returns:
/// Signal strength image
+ (UIImage * _Nonnull)tentacleSignalStrengthIconForRssiValue:(NSInteger)rssiValue linkMode:(BOOL)linkMode;

#endif // #if TARGET_OS_IOS

@end

NS_ASSUME_NONNULL_END

