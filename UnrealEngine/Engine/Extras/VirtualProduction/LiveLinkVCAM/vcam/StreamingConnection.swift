//
//  StreamingConnection.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All rights reserved.
//

import Foundation
import UIKit

protocol StreamingConnectionDelegate : AnyObject {
    
    func streamingConnectionDidConnect(_ connection : StreamingConnection)
    func streamingConnection(_ connection : StreamingConnection, didDisconnectWithError err: Error?)
    func streamingConnection(_ connection : StreamingConnection, exitWithError err: Error?)
    func streamingConnection(_ connection : StreamingConnection, requestsTextEditWithContents contents : String, handler : @escaping (Bool, String?) -> Void)
    func streamingConnection(_ connection: StreamingConnection, requestStreamerSelectionWithStreamers streamers: Array<String>, handler: @escaping (String) -> Void)
    func streamingConnection(_ connection: StreamingConnection, receivedGamepadResponse: UInt8)
}

enum StreamingConnectionType : String {
    case remoteSession = "RemoteSession"
    case webRTC = "WebRTC"
}

enum StreamingConnectionTouchType : String {
    case began = "Began"
    case moved = "Moved"
    case ended = "Ended"
}

enum StreamingConnectionError: Error {
    case runtimeError(String)
}



enum StreamingConnectionControllerInputType : String {
    case thumbstickLeftX = "Gamepad_LeftX"
    case thumbstickLeftY = "Gamepad_LeftY"
    case thumbstickRightX = "Gamepad_RightX"
    case thumbstickRightY = "Gamepad_RightY"
    case thumbstickLeftButton = "Gamepad_LeftThumbstick"
    case thumbstickRightButton = "Gamepad_RightThumbstick"
    case faceButtonBottom = "Gamepad_FaceButton_Bottom"
    case faceButtonRight = "Gamepad_FaceButton_Right"
    case faceButtonLeft = "Gamepad_FaceButton_Left"
    case faceButtonTop = "Gamepad_FaceButton_Top"
    case shoulderButtonLeft = "Gamepad_LeftShoulder"
    case shoulderButtonRight = "Gamepad_RightShoulder"
    case triggerButtonLeft = "Gamepad_LeftTrigger"
    case triggerButtonRight = "Gamepad_RightTrigger"
    case dpadUp = "Gamepad_DPad_Up"
    case dpadDown = "Gamepad_DPad_Down"
    case dpadLeft = "Gamepad_DPad_Left"
    case dpadRight = "Gamepad_DPad_Right"
    case specialButtonLeft = "Gamepad_Special_Left"
    case specialButtonRight = "Gamepad_Special_Right"
}


class StreamingConnectionStats {
    var bytesPerSecond : Int?
    var framesPerSecond : Float?
}

class StreamingConnection : NSObject {

    weak var delegate: StreamingConnectionDelegate?
    weak var renderView : UIView?
    
    var subjectName : String!
    
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
    
    var relayTouchEvents = true {
        didSet {
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
    
    func showStats(_ shouldShow : Bool) {
        // Do nothing, derived classes will implement functionality
    }
    
    func shutdown() {
        assertionFailure("not implemented")
    }

    func connect() throws {
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
    
    func sendControllerConnected() {
        assertionFailure("not implemented")
    }
    
    func sendControllerAnalog(_ type :StreamingConnectionControllerInputType, controllerIndex : UInt8, value : Float) {
        assertionFailure("not implemented")
    }
    
    func sendControllerButtonPressed(_ type : StreamingConnectionControllerInputType, controllerIndex : UInt8, isRepeat : Bool) {
        assertionFailure("not implemented")
    }

    func sendControllerButtonReleased(_ type : StreamingConnectionControllerInputType, controllerIndex : UInt8) {
        assertionFailure("not implemented")
    }
    
    func sendControllerDisconnected(controllerIndex: UInt8) {
        assertionFailure("not implemented")
    }

    func setRenderView(_ view : UIView) {
        assertionFailure("not implemented")
    }
    
    func getStats() -> StreamingConnectionStats? {
        return nil
    }

}
