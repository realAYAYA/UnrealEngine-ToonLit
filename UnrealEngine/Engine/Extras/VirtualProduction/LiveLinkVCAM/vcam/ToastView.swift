//
//  ToastView.swift
//  vcam
//
//  Created by Brian Smith on 12/4/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import SwiftEntryKit

class ToastView : UIView {
    
    @IBOutlet weak var imageView : UIImageView!
    @IBOutlet weak var label : UILabel!
    
    private enum Level {
        case info
        case warning
        case error
    }
    
    private class func show(_ level : Level, _ message : String) {
        
        // Create a basic toast that appears at the top
        var attributes = EKAttributes.topToast

        // Set its background to white
        attributes.entryBackground = .color(color: .white)

        // Animate in and out using default translation
        attributes.entranceAnimation = .translation
        attributes.exitAnimation = .translation
        attributes.positionConstraints.size.width = .constant(value: 300)
        attributes.positionConstraints.size.height = .constant(value: 50)

        if let view = Bundle.main.loadNibNamed("Toast", owner: nil, options: nil)?.first as? ToastView {

            switch level {
            case .info:
                view.imageView.image = UIImage(systemName: "info.circle.fill")?.withTintColor(UIColor.systemBlue, renderingMode: .alwaysOriginal)
            case .warning:
                view.imageView.image = UIImage(systemName: "exclamationmark.triangle.fill")?.withTintColor(UIColor.systemYellow, renderingMode: .alwaysOriginal)
            case .error:
                view.imageView.image = UIImage(systemName: "exclamationmark.triangle.fill")?.withTintColor(UIColor.systemRed, renderingMode: .alwaysOriginal)
            }
            
            view.label.text = message
            SwiftEntryKit.display(entry: view, using: attributes)
        }
    }
    
    
    class func info(_ message : String) {

        Log.info(message)
        ToastView.show(.info, message)

    }
    class func warning(_ message : String) {

        Log.warning(message)
        ToastView.show(.warning, message)

    }
    class func error(_ message : String) {

        Log.error(message)
        ToastView.show(.error, message)

    }

}
