//
//  VideoViewController+ARSessionDelegate.swift
//  vcam
//
//  Created by Brian Smith on 8/12/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import ARKit

extension VideoViewController : ARSessionDelegate {
    
    func session(_ session: ARSession, didFailWithError error: Error) {
    }
    
    func session(_ session: ARSession, cameraDidChangeTrackingState camera: ARCamera) {
        
        switch camera.trackingState {
        case ARCamera.TrackingState.normal:
            Log.info("Tracking state normal")
        case ARCamera.TrackingState.limited(let reason):
            switch reason {
            case .initializing:
                Log.info("Tracking state limited : initializing")
            case .excessiveMotion:
                Log.info("Tracking state limited : excessiveMotion")
            case .insufficientFeatures:
                Log.info("Tracking state limited : insufficientFeatures")
            case .relocalizing:
                Log.info("Tracking state limited : relocalizing")
            default:
                Log.info("Tracking state limited : unknown")
            }
        case ARCamera.TrackingState.notAvailable:
            Log.info("Tracking state notAvailable")
            
        }
    }
    
    func session(_ session: ARSession, didUpdate frame: ARFrame) {
        self.streamingConnection?.sendTransform(frame.camera.transform, atTime: Timecode.create().toTimeInterval())
        
        // update controller
        self.sendControllerUpdate()
    }
}
