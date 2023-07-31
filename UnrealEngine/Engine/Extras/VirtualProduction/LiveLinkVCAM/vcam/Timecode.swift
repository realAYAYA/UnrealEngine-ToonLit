//
//  Timecode.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 12/26/19.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

// FYI much of this is lifted from the Live Link Face app

import Foundation
import CoreMedia
import Tentacle
import Kronos

enum TimecodeSource : Int{
    case none = 0
    case systemTime = 1
    case ntp = 2
    case tentacleSync = 3
    case unknown = 9999
}

func clockFrequency() -> Double {
    
    var info = mach_timebase_info()
    guard mach_timebase_info(&info) == KERN_SUCCESS else { return 0 }

    return (Double(info.denom) / Double(info.numer)) * 1000000000.0
}

@objc class Timecode : NSObject {

    static let secondsToMach = clockFrequency()
    
    static var targetFrameRate = UInt8(60)
    var tentacleTimecode : TentacleTimecode?

    var source : TimecodeSource!
    var current = true
    var valid = false

    @objc public var hours : Int32 = 0
    @objc public var minutes : Int32 = 0
    @objc public var seconds : Int32 = 0
    @objc public var frames : Int32 = 0
    @objc public var fraction : Float = 0
    @objc public var dropFrame : Bool = false
    
    class func sourceToString(_ source : TimecodeSource) -> String {
        
        switch source {
        case .systemTime:
            return NSLocalizedString("timecode-source-system", value: "System Timer", comment: "The internal device timer will be used as the timecode source")
        case .ntp:
            return NSLocalizedString("timecode-source-ntp", value: "NTP", comment: "An NTP (network time protocol) pool will be used as the timecode source")
        case .tentacleSync:
            return NSLocalizedString("timecode-source-tentacle", value: "Tentacle Sync", comment: "An Tentacle Sync will be used as the timecode source")
        default:
            return Localized.unknown()
        }
    }
    
    class func sourceIsSystemTime() -> Bool {
        return AppSettings.shared.timecodeSourceEnum() == .systemTime;
    }

    class func sourceIsTentacle() -> Bool {
        return AppSettings.shared.timecodeSourceEnum() == .tentacleSync;
    }

    class func sourceIsNTP() -> Bool {
        return AppSettings.shared.timecodeSourceEnum() == .ntp;
    }


    class func create() -> Timecode {
        
        switch AppSettings.shared.timecodeSourceEnum() {

        case .tentacleSync:
            return Timecode(tentacleDevice: Tentacle.shared?.activeDevice, atTimeInterval: CACurrentMediaTime())
        case .ntp:
            return Timecode(annotatedTime: Clock.annotatedNow)
            
        default:
            return Timecode(timeInterval: CACurrentMediaTime())
        }
    }

    init(fromString str: String) {
        
        source = .unknown
        
        let components = str.components(separatedBy: ":")
        if components.count == 4 {
            
            if let h = Int32(components[0]),
                let m = Int32(components[1]),
                let s = Int32(components[2]) {

                if let f = Int32(components[3]) {
                    frames = f
                    valid = true
                } else {
                    let frameAndFraction = components[3].components(separatedBy: ".")
                    if frameAndFraction.count == 2 {
                        if let f = Int32(frameAndFraction[0]),
                            let frac = Float(frameAndFraction[1]) {
                            frames = f
                            fraction = frac / 1000
                            valid = true
                        }
                        
                    }

                }
                
                if valid {
                    hours = h
                    minutes = m
                    seconds = s
                }
            }
        }
    }

    public init(annotatedTime: AnnotatedTime?) {
        
        source = .ntp
        
        if let an = annotatedTime {
            valid = true

            let components = Calendar.current.dateComponents([.hour, .minute, .second, .nanosecond], from: an.date)
            
            hours = Int32(components.hour ?? 0)
            minutes = Int32(components.minute ?? 0)
            seconds = Int32(components.second ?? 0)
            
            if let usec = components.nanosecond {
                let frameWithFraction = Double(usec)/1000000000.0 * Double(Timecode.targetFrameRate)
                frames = Int32(frameWithFraction)
                fraction = Float(frameWithFraction.truncatingRemainder(dividingBy: 1))
            }
        } else {
            current = false
        }
    }

    public init(timeInterval : CFTimeInterval, source timecodeSource : TimecodeSource) {

        source = timecodeSource
        valid = true

        hours = Int32(timeInterval / 3600.0) % 24
        minutes = Int32(UInt64(timeInterval / 60.0) % 60)
        seconds = Int32(UInt64(timeInterval) % 60)
        
        let frameWithFraction = Float(timeInterval - floor(timeInterval)) * Float(Timecode.targetFrameRate)
        frames = Int32(frameWithFraction)
        fraction = frameWithFraction.truncatingRemainder(dividingBy: 1)
    }

    convenience init(timeInterval: CFTimeInterval) {
        
        self.init(timeInterval:timeInterval, source : .systemTime)
    }

    convenience init(tentacleDevice device: TentacleDevice?, atTimeInterval timeInterval: Double) {
        
        if device != nil {
            
            var timecode = device!.timecode
            var advertisement = device!.advertisement
            let seconds = TentacleTimecodeSecondsAtTimestamp(&timecode, TentacleAdvertisementGetFrameRate(&advertisement), device!.advertisement.dropFrame, timeInterval)
            self.init(timeInterval:seconds)
            tentacleTimecode = device!.timecode
            dropFrame = device!.advertisement.dropFrame
            current = true// !TentacleDeviceIsDisappeared(&device!, timeInterval)

        } else {
            self.init(timeInterval: 0)
            valid = false
            current = false
        }
        
        source = .tentacleSync
    }
    
    func toString(includeFractional : Bool) -> String {
        
        if valid {
            if includeFractional {
                return String(format:"%02d:%02d:%02d:%02d.%03d", hours, minutes, seconds, frames, Int(fraction * 1000))
            } else {
                return String(format:"%02d:%02d:%02d:%02d", hours, minutes, seconds, frames)
            }
        } else {
            
            switch source {
            case .ntp:
                return NSLocalizedString("timecode-error-ntp", value: "NO DATA", comment: "An error string for NTP input")
            case .tentacleSync:
                return NSLocalizedString("timecode-error-tentacle", value: "NO DEVICE", comment: "An error string for tentacle sync input")
            default :
                return NSLocalizedString("timecode-error-system", value: "SYSTEM ERROR", comment: "An error string for system timer input")
            }
        }
    }
    
    func toTimeInterval() -> CFTimeInterval {
        
        if valid {
            return Double(hours * 3600) + Double(minutes * 60) + Double(seconds) + (Double(frames) + Double(fraction)) / Double(Timecode.targetFrameRate)
        } else {
            return 0
        }
    }
    
    func offsetBy(_ offset : CFTimeInterval) -> Timecode {
        
        return Timecode(timeInterval: toTimeInterval() + offset, source: source)
    }
}
