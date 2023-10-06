//
//  VideoDecoder.swift
//  vcam
//
//  Created by Brian Smith on 8/8/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation
import CoreVideo

protocol VideoDecoder : AnyObject {
    
    func decode(width : Int32, height : Int32, data: Data, _ completion: @escaping (_ pixelBuffer : CVPixelBuffer?) -> Void)

}
