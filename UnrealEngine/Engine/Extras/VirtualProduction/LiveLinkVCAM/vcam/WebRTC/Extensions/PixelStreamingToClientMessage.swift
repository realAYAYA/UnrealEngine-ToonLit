//
//  PixelStreamingToClientMessage.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 15/6/2022.
//  Copyright © 2022 Stas Seldin. All rights reserved.
//

import Foundation

// Must stay synced with Pixel Streaming plugin until a better solution is devised.
enum PixelStreamingToClientMessage: UInt8, Codable {
    case QualityControlOwnership = 0
    case Response = 1
    case Command = 2
    case FreezeFrame = 3
    case UnfreezeFrame = 4
    case VideoEncoderAvgQP = 5
    case LatencyTest = 6
    case InitialSettings = 7
    case FileExtension = 8
    case FileMimeType = 9
    case FileContents = 10
}


