//
//  Tentacle.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/22/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation
import Tentacle

protocol TentacleDelegate {
    func tentacleDidUpdateAvailableDevices(_ tentacle : Tentacle)
    func tentacle(_ tentacle : Tentacle, didSetActiveDevice device : TentacleDevice?)
    func tentacle(_ tentacle : Tentacle, didUpdateDevice device : inout TentacleDevice)
}

class Tentacle : NSObject {

    static var shared : Tentacle?

    var delegate : TentacleDelegate?
    
    var uptime : Double {
        get {
            Date().timeIntervalSince(self.startupTime)
        }
    }
    
    
    var activeDevice : TentacleDevice? {
        didSet {
            // if we have a valid peripheral, then we automatically save the UUID. Otherwise if
            // this is being set to nil, we keep the previous value and allow clearing the UUID
            // by the forgetActiveDevice() function
            var device = self.activeDevice
            if device != nil {
                AppSettings.shared.tentaclePeripheralUUID = Tentacle.advertisementIdString(advertisement: device!.advertisement)
                AppSettings.shared.tentaclePeripheralName = Tentacle.nameString(advertisement: &device!.advertisement)
            }
        }
    }
    var availableDevices : [TentacleDevice] {
        get {
            var devices = [TentacleDevice]()
            for i in 0..<TentacleDeviceCacheGetSize() {
                devices.append(TentacleDeviceCacheGetDevice(Int32(i)))
            }
            
            return devices
        }
    }

    private var bluetooth : TentacleBluetoothController!
    private var startupTime : Date!
    private var uuids = Set<String>()
    private var timer : Timer!

    override init() {
        super.init()
        self.bluetooth = TentacleBluetoothController()
        self.bluetooth.delegate = self
        
        //self.bluetooth.deleteThreshold = 300  // 5 min
        self.startupTime = Date()
        
            //self.timer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true, block: { _ in
        //    self.bluetooth.deleteUnavailableTentacles()
        //})
    }
    
    deinit {
        self.bluetooth = nil
    }
    
    func forgetActiveDevice() {
        self.activeDevice = nil
        AppSettings.shared.tentaclePeripheralUUID = ""
        AppSettings.shared.tentaclePeripheralName = ""

        if let d = self.delegate {
            d.tentacle(self, didSetActiveDevice: nil)
        }
    }
    
    class func advertisementIdString(advertisement:  TentacleAdvertisement) -> String {
        withUnsafePointer(to: advertisement.identifier) {
            $0.withMemoryRebound(to: UInt8.self, capacity: MemoryLayout.size(ofValue: advertisement.identifierLength)) {
                String(cString: $0)
            }
        }
    }
    
    class func advertisementString(advertisement: inout TentacleAdvertisement) -> String {
        let stringPointer = initStringPointer(capacity: TENTACLE_ADVERTISEMENT_STRING_LENGTH)
        TentacleAdvertisementNameString(&advertisement, stringPointer)
        return String(cString: stringPointer)
    }
    
    class func nameString(advertisement: inout TentacleAdvertisement) -> String {
        let stringPointer = initStringPointer(capacity: TENTACLE_ADVERTISEMENT_NAME_LENGTH_MAX)
        TentacleAdvertisementNameString(&advertisement, stringPointer)
        return String(cString: stringPointer)
    }
    
    class func initStringPointer(capacity: Int32) -> UnsafeMutablePointer<Int8> {
        let stringPointer = UnsafeMutablePointer<Int8>.allocate(capacity: Int(capacity))
        stringPointer.initialize(repeating: 0, count: Int(capacity))
        return stringPointer
    }
    
    class func devicesIdentical(_ device0 : TentacleDevice?, _ device1 : TentacleDevice?) -> Bool {
        if let d0 = device0, let d1 = device1 {
            let id0 = advertisementIdString(advertisement: d0.advertisement)
            let id1 = advertisementIdString(advertisement: d1.advertisement)
            
            return id0 == id1
        } else {
            return device0 == nil && device1 == nil
        }
    }

    class func timecodeString(device: inout TentacleDevice, forTimestamp timestamp: Double) -> String {
        let frameRate = TentacleAdvertisementGetFrameRate(&device.advertisement)
        let dropFrame = device.advertisement.dropFrame
        let timecode  = TentacleTimecodeAtTimestamp(device.timecode, frameRate, dropFrame, timestamp);

        let stringPointer = initStringPointer(capacity: TENTACLE_TIMECODE_STRING_LENGTH)
        TentacleTimecodeString(timecode, frameRate, dropFrame, stringPointer)
        return String(cString: stringPointer)
    }
}


extension Tentacle : TentacleBluetoothControllerDelegate {

    func didReceiveAdvertisement(forDeviceIndex deviceIndex: Int) {

        
        var device = TentacleDeviceCacheGetDevice(Int32(deviceIndex))
        let identifier = Tentacle.advertisementIdString(advertisement: device.advertisement)

        // keep track of which UUIDs have been advertised
        
        // if we already have this UDID,
        if uuids.contains(identifier) {
            
            if let d = self.delegate {
                d.tentacle(self, didUpdateDevice : &device)
            }
            
        } else {
            
            // let name = Tentacle.nameString(advertisement: &device.advertisement)
            //Log.info("Tentacle didReceiveAdvertisment : \(name) : \(identifier)")
            uuids.insert(identifier)
            
            if let d = self.delegate {
                d.tentacleDidUpdateAvailableDevices(self)
            }
            
            if identifier == AppSettings.shared.tentaclePeripheralUUID {
            
                self.activeDevice = device
                
                if let d = self.delegate {
                    d.tentacle(self, didSetActiveDevice: device)
                }
            }
        }
    }
    
    func didUpdate(to state: TentacleBluetoothState) {
        if state == .poweredOn {
            self.bluetooth.startScanning()
        }
    }
    
    func didStartScanning() {
        Log.info("Tentacle : didStartScanning()")
    }

}
