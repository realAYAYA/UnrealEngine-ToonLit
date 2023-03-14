//
//  StreamingConnection.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All rights reserved.
//

import Foundation
import UIKit

protocol StreamingConnectionDelegate {
    
    func streamingConnectionDidConnect(_ connection : StreamingConnection)
    func streamingConnection(_ connection : StreamingConnection, didDisconnectWithError err: Error?)
}

enum StreamingConnectionTouchType : String {
    case began = "Began"
    case moved = "Moved"
    case ended = "Ended"
}

class StreamingConnectionStats {
    var bytesPerSecond : Int?
    var framesPerSecond : Float?
}

class StreamingConnection : NSObject {

    var delegate: StreamingConnectionDelegate?
    var subjectName : String!
    
    var renderView : UIView?

    var name: String {
        get {
            assertionFailure("not implemented")
            return ""
        }
    }
    
    var destination: String {
        get {
            assertionFailure("not implemented")
            return ""
        }
        set {
            assertionFailure("not implemented")
        }
        
    }
    
    var isConnected: Bool {
        get {
            assertionFailure("not implemented")
            return false
        }
    }
    
    var stats : StreamingConnectionStats?
    
    private(set) var videoAspectRatio: CGFloat?
    var videoSize: CGSize? {
        didSet {
            if let vs = videoSize {
                self.videoAspectRatio = vs.width / vs.height
            } else {
                self.videoAspectRatio = nil
            }
        }
    }
    
    required init(subjectName: String) {
        self.subjectName = subjectName
    }

    func connect() throws {
        assertionFailure("not implemented")
    }

    func reconnect() {
        assertionFailure("not implemented")
    }

    func disconnect() {
        assertionFailure("not implemented")
    }
    
    func sendTransform(_ transform: simd_float4x4, atTime time: Double) {
        assertionFailure("not implemented")
    }
    
    func setRenderView(_ view : UIView) {
        assertionFailure("not implemented")
    }
    
    func getStats() -> StreamingConnectionStats? {
        return nil
    }

}
