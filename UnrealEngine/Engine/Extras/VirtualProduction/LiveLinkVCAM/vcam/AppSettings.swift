//
//  Defaults.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/16/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation
import AVKit

//
// The App Settings are saved via user-defaults, but changes are generally communicated
// throughout the app by key-value observers (NSKeyValueObservation), which is by each
// property has to be a @objc dynamic
//
class AppSettings : NSObject {

    static let shared = AppSettings()
    
    class func defaultLiveLinkSubjectName() -> String {
        UIDevice.current.name.toUnrealCompatibleString()
    }

    private class Keys {
        static let didAcceptEULA = "didAcceptEULA"
        static let lastConnectionAddress = "lastConnectionAddress"
        static let recentConnectionAddresses = "recentConnectionAddresses"

        static let liveLinkSubjectName = "liveLinkSubjectName"

        static let timecodeSource = "timecodeSource"
        static let tentaclePeripheralUUID = "tentaclePeripheralUUID"
        static let tentaclePeripheralName = "tentaclePeripheralName"
        static let ntpPool = "ntpPool"

        static let showStreamingStats = "showStreamingStats"

        static let connectionType = "connectionType"
    }
    
    private class func defaultsDictionary() -> Dictionary<String, Any> {
        
        return [
            Keys.didAcceptEULA : false,
            Keys.lastConnectionAddress : "",
            Keys.recentConnectionAddresses : [],

            Keys.liveLinkSubjectName : defaultLiveLinkSubjectName(),
            
            Keys.timecodeSource : 1,
            Keys.tentaclePeripheralUUID : "",
            Keys.tentaclePeripheralName : "",
            Keys.ntpPool : "",

            Keys.showStreamingStats : false,
            
            Keys.connectionType : StreamingConnectionType.webRTC.rawValue

        ]
    }
    
    class func registerDefaults() {
        
        UserDefaults.standard.register(defaults: AppSettings.defaultsDictionary())
    }

    class func reset() {
        Log.info("Resetting settings.")

        // first, set the settings to the defaults : this will trigger any KVO on the values
        for (key,value) in AppSettings.defaultsDictionary() {
            AppSettings.shared.setValue(value, forKey: key)
        }

        // then delete all values in the UserDefaults so we will fallback to those
        // registered in registerDefaults() at later time
        for (key,_) in UserDefaults.standard.dictionaryRepresentation() {
            UserDefaults.standard.removeObject(forKey: key)
        }
    }
        
    @objc dynamic var didAcceptEULA : Bool = UserDefaults.standard.bool(forKey: Keys.didAcceptEULA) {
        didSet {
            UserDefaults.standard.set(didAcceptEULA, forKey: Keys.didAcceptEULA)
        }
    }

    func addRecentConnectionAddress(_ address : String) {
        if let recents = UserDefaults.standard.array(forKey: Keys.recentConnectionAddresses) as? [String] {
            // remove this connection address if it's already in the recent list and re-insert it at the front (most recent)
            var newRecents = recents.filter { $0 != address }
            if newRecents.count > 4 {
                newRecents = Array(newRecents[...3])
            }
            newRecents.insert(address, at: 0)
            UserDefaults.standard.set(newRecents, forKey: Keys.recentConnectionAddresses)
        } else {
            UserDefaults.standard.set([address], forKey: Keys.recentConnectionAddresses)
        }
    }
    
    var recentConnectionAddresses : [String]? {
        get {
            UserDefaults.standard.array(forKey: Keys.recentConnectionAddresses) as? [String]
        }
    }
    
    @objc dynamic var lastConnectionAddress : String = UserDefaults.standard.string(forKey: Keys.lastConnectionAddress) ?? "" {
        didSet{
            UserDefaults.standard.set(lastConnectionAddress, forKey: Keys.lastConnectionAddress)
        }
    }

    @objc dynamic var liveLinkSubjectName : String = UserDefaults.standard.string(forKey: Keys.liveLinkSubjectName) ?? defaultLiveLinkSubjectName() {
        didSet {
            UserDefaults.standard.set(liveLinkSubjectName, forKey: Keys.liveLinkSubjectName)
        }
    }

    @objc dynamic var timecodeSource : Int = UserDefaults.standard.integer(forKey: Keys.timecodeSource) {
        didSet {
            UserDefaults.standard.set(timecodeSource, forKey: Keys.timecodeSource)
        }
    }
    
    func timecodeSourceEnum() -> TimecodeSource {
        return TimecodeSource(rawValue: self.timecodeSource) ?? .unknown
    }

    func setTimecodeSourceEnum(_ source : TimecodeSource)  {
        self.timecodeSource = source.rawValue
    }

    @objc dynamic var tentaclePeripheralUUID : String = UserDefaults.standard.string(forKey: Keys.tentaclePeripheralUUID) ?? "" {
        didSet {
            UserDefaults.standard.set(tentaclePeripheralUUID, forKey: Keys.tentaclePeripheralUUID)
        }
    }

    @objc dynamic var tentaclePeripheralName : String = UserDefaults.standard.string(forKey: Keys.tentaclePeripheralName) ?? "" {
        didSet {
            UserDefaults.standard.set(tentaclePeripheralName, forKey: Keys.tentaclePeripheralName)
        }
    }

    @objc dynamic var ntpPool : String = UserDefaults.standard.string(forKey: Keys.ntpPool) ?? "" {
        didSet {
            UserDefaults.standard.set(ntpPool, forKey: Keys.ntpPool)
        }
    }
    
    @objc dynamic var showStreamingStats : Bool = UserDefaults.standard.bool(forKey: Keys.showStreamingStats) {
        didSet {
            UserDefaults.standard.set(showStreamingStats, forKey: Keys.showStreamingStats)
        }
    }
    
    @objc dynamic var connectionType : String = UserDefaults.standard.string(forKey: Keys.connectionType) ?? StreamingConnectionType.webRTC.rawValue {
        didSet {
            UserDefaults.standard.set(connectionType, forKey: Keys.connectionType)
        }
    }
    
    func connectionTypeEnum() -> StreamingConnectionType {
        return StreamingConnectionType(rawValue: self.connectionType) ?? .webRTC
    }

    func setConnectionTypeEnum(_ type : StreamingConnectionType)  {
        self.connectionType = type.rawValue
    }
    

}
