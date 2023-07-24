//
//  OSCUtility.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/24/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation
import UIKit

class OSCUtility {
    
    class func hostAndPortFromAddress(_ address : String) -> (host: String, port : UInt16?) {

        let components = address.components(separatedBy: ":")

        if components.count == 0 {
            return (address, nil)
        } else if components.count == 1 {
            return (components[0], nil)
        } else {
            return (components[0], UInt16(components[1]))
        }
    }
    
    class func hostAndPortFromAddress(_ address : String, defaultPort : UInt16) -> (host: String, port : UInt16) {
        
        let (host, port) = hostAndPortFromAddress(address)
        if port == nil {
            return (host, defaultPort)
        } else {
            return (host,port!)
        }
    }
    
    // Return IP address of WiFi interface (en0) as a String, or `nil`
    class func getWiFiIPAddress() -> String? {
        var address : String?

        // Get list of all interfaces on the local machine:
        var ifaddr : UnsafeMutablePointer<ifaddrs>?
        guard getifaddrs(&ifaddr) == 0 else { return nil }
        guard let firstAddr = ifaddr else { return nil }

        // For each interface ...
        for ifptr in sequence(first: firstAddr, next: { $0.pointee.ifa_next }) {
            let interface = ifptr.pointee

            // Check for IPv4 or IPv6 interface:
            let addrFamily = interface.ifa_addr.pointee.sa_family
            if addrFamily == UInt8(AF_INET) /* || addrFamily == UInt8(AF_INET6) */ {

                // Check interface name:
                let name = String(cString: interface.ifa_name)
                if  name == "en0" {

                    // Convert interface address to a human readable string:
                    var hostname = [CChar](repeating: 0, count: Int(NI_MAXHOST))
                    getnameinfo(interface.ifa_addr, socklen_t(interface.ifa_addr.pointee.sa_len),
                                &hostname, socklen_t(hostname.count),
                                nil, socklen_t(0), NI_NUMERICHOST)
                    address = String(cString: hostname)
                }
            }
        }
        freeifaddrs(ifaddr)

        return address
    }
    
    class func ueTouchData(point: CGPoint, finger : Int, force : CGFloat ) -> Data {
     
        var data = Data(capacity: 16);
        
        data.append(OSCPacket.encode(Float(point.x), bigEndian: false))
        data.append(OSCPacket.encode(Float(point.y), bigEndian: false))
        data.append(OSCPacket.encode(Int32(finger), bigEndian: false))
        data.append(OSCPacket.encode(Float(force), bigEndian: false))

        return data;
    }
    
    class func ueControllerAnalogData(key : String, controller : Int, value : Float) -> Data {
        
        var data = Data(capacity: 32);
        
        data.append(OSCPacket.encode(Int32(key.count + 1), bigEndian: false))
        data.append(key.data(using: .utf8)!)
        data.append(OSCPacket.encode(Int8(0)))
        data.append(OSCPacket.encode(Int32(controller), bigEndian: false))
        data.append(OSCPacket.encode(Float(value), bigEndian: false))

        return data;
        
    }
    
    class func ueControllerButtonData(key : String, controller : Int, isRepeat : Bool) -> Data {
        
        var data = Data(capacity: 32);
        
        data.append(OSCPacket.encode(Int32(key.count + 1), bigEndian: false))
        data.append(key.data(using: .utf8)!)
        data.append(OSCPacket.encode(Int8(0)))
        data.append(OSCPacket.encode(Int32(controller), bigEndian: false))
        data.append(OSCPacket.encode(Int8(isRepeat ? 1 : 0)))

        return data;
        
    }
}
