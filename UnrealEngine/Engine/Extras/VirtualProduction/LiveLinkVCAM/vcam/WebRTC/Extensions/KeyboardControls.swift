//
//  KeyboardControls.swift
//  vcam
//
//  Created by William Belcher on 1/2/2023.
//

import Foundation

protocol KeyboardDelegate: AnyObject {
    func submitString(_ entry: String)
}

final class KeyboardControls: KeyboardDelegate {
    let webRTCClient: WebRTCClient
    
    init(_ webRTCClient: WebRTCClient) {
        self.webRTCClient = webRTCClient
    }
    
    func submitString(_ entry: String) {
        var bytes: [UInt8] = []
        bytes.append(PixelStreamingToStreamerMessage.TextboxEntry.rawValue)
        bytes.append(contentsOf: UInt16(entry.count).toBytes())
        
        for char in entry {
            bytes.append(contentsOf: char.utf16[char.utf16.index(char.utf16.startIndex, offsetBy: 0)].toBytes())
        }

        self.webRTCClient.sendData(Data(bytes))
    }
}
