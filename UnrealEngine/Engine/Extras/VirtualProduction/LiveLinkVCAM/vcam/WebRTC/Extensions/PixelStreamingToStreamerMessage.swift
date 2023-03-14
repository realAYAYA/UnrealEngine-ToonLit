//
//  PixelStreamingToStreamerMessage.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 20/6/2022.
//

import Foundation

enum PixelStreamingToStreamerMessage : UInt8, Codable {
    case IFrameRequest = 0
    case RequestQualityControl = 1
    case FpsRequest = 2
    case AverageBitrateRequest = 3
    case StartStreaming = 4
    case StopStreaming = 5
    case LatencyTest = 6
    case RequestInitialSettings = 7
    case TestEcho = 8
    
    case UIInteraction = 50
    case Command = 51

    case KeyDown = 60
    case KeyUp = 61
    case KeyPress = 62

    case MouseEnter = 70
    case MouseLeave = 71
    case MouseDown = 72
    case MouseUp = 73
    case MouseMove = 74
    case MouseWheel = 75

    case TouchStart = 80
    case TouchEnd = 81
    case TouchMove = 82

    case GamepadButtonPressed = 90
    case GamepadButtonReleased = 91
    case GamepadAnalog = 92
    
    case Transform = 100
}
