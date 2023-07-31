//
//  NetUtility.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/24/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation

class NetUtility {
    
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

    // ********************************************************************************
    // ********************************************************************************
    // ********************************************************************************
    // ********************************************************************************
    // from https://developer.apple.com/forums/thread/663768
    // ********************************************************************************
    // ********************************************************************************
    // ********************************************************************************
    // ********************************************************************************

    /// Does a best effort attempt to trigger the local network privacy alert.
    ///
    /// It works by sending a UDP datagram to the discard service (port 9) of every
    /// IP address associated with a broadcast-capable interface. This should
    /// trigger the local network privacy alert, assuming the alert hasn’t already
    /// been displayed for this app.
    ///
    /// This code takes a ‘best effort’. It handles errors by ignoring them. As
    /// such, there’s guarantee that it’ll actually trigger the alert.
    ///
    /// - note: iOS devices don’t actually run the discard service. I’m using it
    /// here because I need a port to send the UDP datagram to and port 9 is
    /// always going to be safe (either the discard service is running, in which
    /// case it will discard the datagram, or it’s not, in which case the TCP/IP
    /// stack will discard it).
    ///
    /// There should be a proper API for this (r. 69157424).
    ///
    /// For more background on this, see [Triggering the Local Network Privacy Alert](https://developer.apple.com/forums/thread/663768).
    class func triggerLocalNetworkPrivacyAlert() {
        let sock4 = socket(AF_INET, SOCK_DGRAM, 0)
        guard sock4 >= 0 else { return }
        defer { close(sock4) }
        let sock6 = socket(AF_INET6, SOCK_DGRAM, 0)
        guard sock6 >= 0 else { return }
        defer { close(sock6) }
        
        let addresses = NetUtility.addressesOfDiscardServiceOnBroadcastCapableInterfaces()
        var message = [UInt8]("!".utf8)
        for address in addresses {
            address.withUnsafeBytes { buf in
                let sa = buf.baseAddress!.assumingMemoryBound(to: sockaddr.self)
                let saLen = socklen_t(buf.count)
                let sock = sa.pointee.sa_family == AF_INET ? sock4 : sock6
                _ = sendto(sock, &message, message.count, MSG_DONTWAIT, sa, saLen)
            }
        }
    }
    /// Returns the addresses of the discard service (port 9) on every
    /// broadcast-capable interface.
    ///
    /// Each array entry is contains either a `sockaddr_in` or `sockaddr_in6`.
    class private func addressesOfDiscardServiceOnBroadcastCapableInterfaces() -> [Data] {
        var addrList: UnsafeMutablePointer<ifaddrs>? = nil
        let err = getifaddrs(&addrList)
        guard err == 0, let start = addrList else { return [] }
        defer { freeifaddrs(start) }
        return sequence(first: start, next: { $0.pointee.ifa_next })
            .compactMap { i -> Data? in
                guard
                    (i.pointee.ifa_flags & UInt32(bitPattern: IFF_BROADCAST)) != 0,
                    let sa = i.pointee.ifa_addr
                else { return nil }
                var result = Data(UnsafeRawBufferPointer(start: sa, count: Int(sa.pointee.sa_len)))
                switch CInt(sa.pointee.sa_family) {
                case AF_INET:
                    result.withUnsafeMutableBytes { buf in
                        let sin = buf.baseAddress!.assumingMemoryBound(to: sockaddr_in.self)
                        sin.pointee.sin_port = UInt16(9).bigEndian
                    }
                case AF_INET6:
                    result.withUnsafeMutableBytes { buf in
                        let sin6 = buf.baseAddress!.assumingMemoryBound(to: sockaddr_in6.self)
                        sin6.pointee.sin6_port = UInt16(9).bigEndian
                    }
                default:
                    return nil
                }
                return result
            }
    }

}
