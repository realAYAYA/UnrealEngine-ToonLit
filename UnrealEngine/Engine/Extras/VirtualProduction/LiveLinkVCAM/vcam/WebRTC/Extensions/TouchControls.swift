//
//  TouchControls.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 20/6/2022.
//

import Foundation
import UIKit

// Protocol for any touch consumer to fulfil.
protocol TouchDelegate: AnyObject {
    func touchesBegan(_ touches: Set<UITouch>);
    func touchesMoved(_ touches: Set<UITouch>);
    func touchesEnded(_ touches: Set<UITouch>);
    func touchesCancelled(_ touches: Set<UITouch>);
    func onVideoChangedSize(size: CGSize)
}

// Represents a coordinate along a viewport axis (0...1) that has been quantized into an uint16 (0..65536).
struct TouchPoint {
    let x : UInt16
    let y : UInt16
    // Value is not in range if the touch was outside the view bounds
    let inRange : Bool
    let force : UInt8
    
    init(x : UInt16, y : UInt16, inRange : Bool, force : UInt8) {
        self.x = x
        self.y = y
        self.inRange = inRange
        self.force = force
    }
}

final class TouchControls : TouchDelegate {
    
    let webRTCClient : WebRTCClient
    let touchView : UIView
    var videoAspectRatio : CGFloat
    
    // We need a way to give each touch a unique finger id that is persistent throughout
    // the life of that touch. So we map each touch to an id [0...10] - iPads can do 11 simulataneous touches!
    var fingers : [Int] = [10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
    var fingerIds = [UITouch : Int]()
    
    init(_ webRTCClient : WebRTCClient, touchView: UIView) {
        self.webRTCClient = webRTCClient
        self.touchView = touchView
        self.videoAspectRatio = CGFloat.zero
    }
    
    func rememberTouch(_ touch : UITouch) {
        let fingerId : Int? = self.fingers.popLast()
        
        if let fingerId = fingerId {
            self.fingerIds[touch] = fingerId
        } else {
            debugPrint("Exhausted all touch identifiers - this shouldn't happen, it indicates a leak in tracking touch events.")
        }
        
    }

    func forgetTouch(_ touch : UITouch) {
        let touchId : Int? = self.fingerIds[touch]
        if let touchId = touchId {
            self.fingers.append(touchId)
            self.fingerIds.removeValue(forKey: touch)
        } else {
            debugPrint("Could not forget this touch because we don't have it stored.")
        }
    }
    
    func touchesBegan(_ touches: Set<UITouch>) {
        // Assign a unique identifier to each touch.
        for touch in touches {
            self.rememberTouch(touch)
            //let coord : CGPoint = touch.location(in: self.touchView)
            //debugPrint("Touch started x=\(coord.x) y=\(coord.y)")
        }
        
        self.sendTouchData(messageType: PixelStreamingToStreamerMessage.TouchStart, touches: touches)
    }
    
    func touchesMoved(_ touches: Set<UITouch>) {
        //debugPrint("Touch moved - count=\(touches.count)")
        self.sendTouchData(messageType: PixelStreamingToStreamerMessage.TouchMove, touches: touches)
    }
    
    func touchesEnded(_ touches: Set<UITouch>) {
        //debugPrint("Touch ended - count=\(touches.count)")
        self.sendTouchData(messageType: PixelStreamingToStreamerMessage.TouchEnd, touches: touches)

        // Re-cycle unique identifiers previously assigned to each touch.
        for touch in touches {
            self.forgetTouch(touch)
        }
    }
    
    func touchesCancelled(_ touches: Set<UITouch>) {
        // same as touches ended unless we are hooking up gestures
        touchesEnded(touches)
    }
    
    func onVideoChangedSize(size: CGSize) {
        self.videoAspectRatio = size.height / size.width
    }
    
    // Unsigned XY positions are normalized to the ratio (0.0..1.0) along a viewport axis and then quantized into an uint16 (0..65536).
    func normalizeAndQuantize(_ touch: UITouch) -> TouchPoint {
        let uiAspectRatio : CGFloat = touchView.bounds.height / touchView.bounds.width
        let uiBigger : Bool = uiAspectRatio > videoAspectRatio
        let ratio : CGFloat = uiAspectRatio / videoAspectRatio;
        let touchLocation : CGPoint = touch.location(in: touchView)
        let viewBounds : CGRect = touchView.bounds
        
        // normalise x,y to the UI element bounds
        let normalizedX : CGFloat = uiBigger ? (touchLocation.x / viewBounds.width) : (ratio * (touchLocation.x / viewBounds.width - 0.5) + 0.5)
        let normalizedY : CGFloat = uiBigger ? (ratio * (touchLocation.y / viewBounds.height - 0.5) + 0.5) : (touchLocation.y / viewBounds.height)
        
        // normalize force value of touch
        let normalizedForce : UInt8 = touch.maximumPossibleForce > 0 ? UInt8(touch.force / touch.maximumPossibleForce * CGFloat(UInt8.max)) : 1
        
        // Detect if touch is out of bounds
        if normalizedX < 0.0 || normalizedX > 1.0 || normalizedY < 0.0 || normalizedY > 1.0 {
            return TouchPoint(x: UInt16.max, y: UInt16.max, inRange: false, force: 0)
        } else {
            return TouchPoint(x: UInt16(normalizedX * CGFloat(UInt16.max)), y: UInt16(normalizedY * CGFloat(UInt16.max)), inRange: true, force: normalizedForce)
        }
    }
    
    func sendTouchData(messageType: PixelStreamingToStreamerMessage, touches: Set<UITouch>) {
        
        var bytes: [UInt8] = []
        
        // Add type of touch using 1 byte
        bytes.append(messageType.rawValue)
        
        // Add number of touches
        bytes.append(UInt8(touches.count))
        
        for touch in touches {
            let touchId : Int? = fingerIds[touch]
            
            if let touchId = touchId {
                let convertedTouch : TouchPoint = normalizeAndQuantize(touch)
                
                // Write X coordinate as two bytes e.g. UInt16 -> [UInt8]
                bytes.append(contentsOf: convertedTouch.x.toBytes())
                
                // Write Y coorindate as two bytes e.g. UInt16 -> [UInt8]
                bytes.append(contentsOf: convertedTouch.y.toBytes())
                
                // Write finger id as one byte
                bytes.append(UInt8(touchId))
                
                // Write normalized force as one byte
                bytes.append(convertedTouch.force)
                
                // Write in range as one byte
                bytes.append(convertedTouch.inRange ? 1 : 0)
            } else {
                debugPrint("Could not send touch because did not have any matching touch id for it.")
            }
        }
        
        let data = Data(bytes)
        self.webRTCClient.sendData(data)
    }
    
}
