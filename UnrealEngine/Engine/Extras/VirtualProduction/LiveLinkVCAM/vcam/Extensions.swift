//
//  Extensions.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/24/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation
import UIKit
import SceneKit
import VideoToolbox

extension String {
    
    func replaceCharactersFromSet(characterSet: CharacterSet, replacementString: String ) -> String {
      return self.components(separatedBy: characterSet).joined(separator: replacementString)
    }
    
    func toUnrealCompatibleString() -> String {
        return self.trimmed().replaceCharactersFromSet(characterSet: CharacterSet.whitespaces, replacementString: "_").replaceCharactersFromSet(characterSet: CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_")).inverted, replacementString: "")
    }
    
    func trimmed() -> String {
        return self.trimmingCharacters(in: CharacterSet.whitespaces)
    }
    
    func stringByAppendingPathComponent(path: String) -> String {
        let nsstr = self as NSString
        return nsstr.appendingPathComponent(path)
    }
    
    func capitalizingFirstLetter() -> String {
        return prefix(1).capitalized + dropFirst()
    }
    
    func containsCaseInsensitive(_ other : String) -> Bool {
        return range(of: other, options: .caseInsensitive) != nil
    }
}

extension UIImage {
    func scaleImage(_ scale : CGFloat) -> UIImage? {
        
        let newSize = CGSize(width: self.size.width * scale, height: self.size.height * scale)
        UIGraphicsBeginImageContext(newSize)
        defer { UIGraphicsEndImageContext() }
        self.draw(in: CGRect(x: 0, y: 0, width: newSize.width, height: newSize.height))
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    func resizeBackgroundCanvas(newSize: CGSize) -> UIImage? {
        UIGraphicsBeginImageContextWithOptions(newSize, false, 0)
        defer { UIGraphicsEndImageContext() }
        self.draw(in: CGRect(origin: CGPoint(x: (newSize.width - size.width) / 2.0, y: (newSize.height - size.height) / 2.0), size: size))
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    

    public convenience init?(pixelBuffer: CVPixelBuffer) {
        var cgImage: CGImage?
        VTCreateCGImageFromCVPixelBuffer(pixelBuffer, options: nil, imageOut: &cgImage)

        guard let cgImage2 = cgImage else {
            return nil
        }

        self.init(cgImage: cgImage2)
    }
    
    convenience init?(timecodeSource : TimecodeSource) {
        
        switch timecodeSource {
        case .systemTime:
            self.init(named: "counterDevice")
        case .tentacleSync:
            self.init(named: "tentacle")
        case .ntp:
            self.init(named: "counterCloud")
        default:
            return nil
        }

    }
}

extension UIAlertController {

    private struct ActivityIndicatorData {
        static var activityIndicator = UIActivityIndicatorView(frame: CGRect(x: 0, y: 0, width: 40, height: 40))
    }

    func addActivityIndicator() {
        let vc = UIViewController()
        vc.preferredContentSize = CGSize(width: 40,height: 50)
        ActivityIndicatorData.activityIndicator.style = .large
        ActivityIndicatorData.activityIndicator.startAnimating()
        vc.view.addSubview(ActivityIndicatorData.activityIndicator)
        self.setValue(vc, forKey: "contentViewController")
    }

    func dismissActivityIndicator() {
        ActivityIndicatorData.activityIndicator.stopAnimating()
        self.dismiss(animated: false)
    }
}

extension simd_quatf {
    
    var eulerAngles : simd_float3 {

        var out = simd_float3()

        let q = self.vector
        let sinr_cosp = 2.0 * (q.w * q.x + q.y * q.z)
        let cosr_cosp = 1.0 - 2.0 * (q.x * q.x + q.y * q.y)
        out.x = atan2(sinr_cosp, cosr_cosp)

        let sinp = 2.0 * (q.w * q.y - q.z * q.x)
        if abs(sinp) >= 1.0 {
            out.y = (sinp < 0.0) ? (-Float.pi / 2.0) : (Float.pi / 2.0)
        } else {
            out.y = asin(sinp)
        }

        let siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
        let cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        out.z = atan2(siny_cosp, cosy_cosp)

        return out
    }
    
}

extension Float {
     func toBytes() -> [UInt8] {
         withUnsafeBytes(of: self, Array.init)
     }
 }

 extension Double {
     func toBytes() -> [UInt8] {
         withUnsafeBytes(of: self, Array.init)
     }
 }
