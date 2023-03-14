//
//  RemoteSessionView.swift
//  vcam
//
//  Created by Brian Smith on 6/30/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import QuickLayout

protocol RemoteSessionViewDelegate {
    
    func remoteSessionView(_ view : RemoteSessionView?, touch : StreamingConnectionTouchType, index : Int, at point: CGPoint, force: CGFloat)
    
}

class RemoteSessionView : UIView {

    var delegate : RemoteSessionViewDelegate?
    
    private var _currentTouches = Array<UITouch?>()

    private weak var _aspectRatioConstraint : NSLayoutConstraint?

    var pixelBuffer : CVPixelBuffer? {
        didSet {
            if let pb = pixelBuffer {

                self.layer.contents = pb
                
                let thisSize = CGSize(width: CVPixelBufferGetWidth(pb), height: CVPixelBufferGetHeight(pb))
                if _aspectRatioConstraint == nil || _aspectRatioConstraint?.multiplier != thisSize.width / thisSize.height {
                    if let constraint = _aspectRatioConstraint {
                        self.removeConstraint(constraint)
                    }
                    _aspectRatioConstraint = layout(.width, to: .height, of: self, ratio: (thisSize.width / thisSize.height), priority: .required)
                }
            }
        }
    }
    
    override func didMoveToSuperview() {
        self.removeConstraints(self.constraints)
        
        // explained here :
        // https://stackoverflow.com/questions/25766747/emulating-aspect-fit-behaviour-using-autolayout-constraints-in-xcode-6
        layoutToSuperview(.centerX, .centerY)
        layoutToSuperview(.width, relation: .lessThanOrEqual, priority: .required)
        layoutToSuperview(.width, relation: .equal, priority: .defaultHigh)
        layoutToSuperview(.height, relation: .lessThanOrEqual, priority: .required)
        layoutToSuperview(.height, relation: .equal, priority: .defaultHigh)
    }
    
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {

        //guard relayTouchEvents else { return }
        
        // for each touch, we need to maintain an index to send via OSC, so that UE
        // can correctly map between began/moved. The currentTouches array will maintain
        // the indices for each touch (keeping some nil if needed) and clean up / compress
        // the array when a touch ends.
        
        for touch in touches {

            // insert this new touch into the array, using the first available slot (a nil entry)
            // or append to the end.
            var fingerIndex = 0
            
            for i in 0..._currentTouches.count {
                fingerIndex = i
                if i == _currentTouches.count {
                    _currentTouches.append(touch)
                    break
                } else if _currentTouches[i] == nil {
                    _currentTouches[i] = touch
                    break
                }
            }
            self.delegate?.remoteSessionView(self, touch: .began, index: fingerIndex, at: touch.location(in: self), force: touch.force)
        }
    }
    
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {

        //guard relayTouchEvents else { return }

        for touch in touches {
            if let fingerIndex = _currentTouches.firstIndex(of: touch) {
                self.delegate?.remoteSessionView(self, touch: .moved, index: fingerIndex, at: touch.location(in: self), force: touch.force)
            }
        }

    }
    
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {

        //guard relayTouchEvents else { return }

        for touch in touches {
            
            
            if let fingerIndex = _currentTouches.firstIndex(of: touch) {
                self.delegate?.remoteSessionView(self, touch: .ended, index: fingerIndex, at: touch.location(in: self), force: touch.force)
                
                // this touch is ended, set to nil
                _currentTouches[fingerIndex] = nil
            }
        }
        
        // compress the array : remove all nils from the end
        if let lastNonNilIndex = _currentTouches.lastIndex(where: { $0 != nil }) {
            if lastNonNilIndex < (_currentTouches.count - 1)  {
                _currentTouches.removeSubrange((lastNonNilIndex + 1)..<_currentTouches.count)
            }
        } else {
            _currentTouches.removeAll()
        }
    }
}
