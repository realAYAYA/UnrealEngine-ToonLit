//
//  PeerConnectionConfig.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 10/6/2022.
//  Copyright © 2022 Stas Seldin. All rights reserved.
//

import Foundation
import WebRTC

// Represents peer connection configuration options such as ICE servers being used.
struct PeerConnectionConfig: Codable {
    let username: String?
    let credential: String?
    let urls: [String]?
    
    init(from rtcConfiguration: RTCConfiguration) {
        
        if !rtcConfiguration.iceServers.isEmpty {
            self.urls = rtcConfiguration.iceServers[0].urlStrings
            self.username = rtcConfiguration.iceServers[0].username
            self.credential = rtcConfiguration.iceServers[0].credential
        }
        else {
            self.urls = []
            self.username = ""
            self.credential = ""
        }
    }
    
    var rtcConfiguration: RTCConfiguration {
               
        let rtcConfig : RTCConfiguration = RTCConfiguration()
        
        if let urls = urls, let username = username, let credential = credential {
            rtcConfig.iceServers = [RTCIceServer(urlStrings: urls, username: username, credential: credential)]
        }
        else if let urls = urls {
            rtcConfig.iceServers = [RTCIceServer(urlStrings: urls)]
        }
        
        rtcConfig.sdpSemantics = .unifiedPlan;
        rtcConfig.continualGatheringPolicy = .gatherContinually
        return rtcConfig
    }
}
