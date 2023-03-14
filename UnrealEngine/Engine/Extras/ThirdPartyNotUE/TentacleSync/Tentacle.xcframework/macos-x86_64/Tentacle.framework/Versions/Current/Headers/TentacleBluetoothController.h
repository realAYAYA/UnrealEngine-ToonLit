//
//  TentacleBluetoothController.h
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

@import CoreBluetooth;
#include <Tentacle/TentacleDevice.h>

NS_ASSUME_NONNULL_BEGIN

/// State of the Bluetooth controller. Uses the same values as <code>CBManagerState</code>.
/// See https://developer.apple.com/documentation/corebluetooth/cbmanagerstate
typedef NS_ENUM(NSUInteger, TentacleBluetoothState) {
    TentacleBluetoothStateUnknown = 0,
    TentacleBluetoothStateResetting = 1,
    TentacleBluetoothStateUnsupported = 2,
    TentacleBluetoothStateUnauthorized = 3,
    TentacleBluetoothStatePoweredOff = 4,
    TentacleBluetoothStatePoweredOn = 5,
};

/// The delegate of a <code>TentacleBluetoothController</code> object must adopt
/// this protocol. The delegate uses this protocol’s methods to notify about the
/// discovery, state changes and disappearance of Bluetooth devices.
@protocol TentacleBluetoothControllerDelegate <NSObject>
@optional
/// Called if state of <code>TentacleBluetoothController</code> changed.
/// \param state New state the Bluetooth controller is in.
- (void)didUpdateToState:(enum TentacleBluetoothState)state;

/// Called if scanning for Bluetooth devices has started.
- (void)didStartScanning;

@required
/// Called each time advertisment data has been received from a Tentacle.
/// Advertisment broadcast rate is multiple times per second, thus this may
/// be called frequently.
/// \param deviceIndex Device with this index has been updated
- (void)didReceiveAdvertismentForDeviceIndex:(NSInteger)deviceIndex;
@end


@interface TentacleBluetoothController : NSObject <CBCentralManagerDelegate>
/// Dictionary of all devices which have been seen.
/// Peripherals are only added to this array but never removed.
@property (nonatomic, strong, readonly) NSMutableDictionary<NSString *, CBPeripheral *> * _Nonnull discoveredPeripherals;
    
/// <code>CBCentralManager</code> instance used to scan for devices.
@property (nonatomic, strong, readonly) CBCentralManager * _Nonnull centralManager;

/// UUID of the Tentacle Bluetooth service.
@property (nonatomic, strong, readonly) CBUUID * _Nonnull tentacleServiceUUID;
    
/// Delegate to receive the Bluetooth events.
@property (nonatomic, weak) id <TentacleBluetoothControllerDelegate> delegate;

/// Resets the central manager
- (void)reset;

/// Starts scanning for Bluetooth Tentacles. Scanning is started with the
/// <code>CBCentralManagerScanOptionAllowDuplicatesKey</code> option set.
- (void)startScanning;

/// Stops scanning for Bluetooth Tentacles.
- (void)stopScanning;

/// Returns a Tentacle device with a specific index from device cache.
/// \param deviceIndex Get device with this index in device cache.
///
/// returns:
/// Device with specified index.
- (TentacleDevice)retrieveDeviceWithIndex:(int)deviceIndex;

/// Precise timestamp in seconds.
///
/// Uses the `mach_absolute_time()` mach kernel syscall.
///
/// See section
/// "Converting Mach absolute time to nanoseconds using mach_timebase_info" in
/// Apple's technical Q&A [Mach Absolute Time Units](https://developer.apple.com/library/content/qa/qa1398/_index.html)
/// and the [mach_time.h](https://github.com/apple/darwin-xnu/blob/master/osfmk/mach/mach_time.h)
/// kernel sources for the [mach_timebase_info](https://github.com/apple/darwin-xnu/blob/xnu-4570.1.46/osfmk/mach/mach_time.h#L36) struct.
///
/// returns:
/// Precise timestamp in seconds.
+ (double)timestamp;
@end

NS_ASSUME_NONNULL_END
