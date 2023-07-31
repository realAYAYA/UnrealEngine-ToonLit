
//
//  OSCAddressPattern.swift
//  vcam
//
//  Created by Brian Smith on 8/8/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation

enum OSCAddressPattern : String {
    
    case rsHello = "/RS.Hello"
    case rsGoodbye = "/RS.Goodbye"
    case rsChannelList = "/RS.ChannelList"
    case rsChangeChannel = "/RS.ChangeChannel"
    case screen = "/Screen"
    case ping = "/Ping"
    case touchStarted = "/MessageHandler/OnTouchStarted"
    case touchMoved = "/MessageHandler/OnTouchMoved"
    case touchEnded = "/MessageHandler/OnTouchEnded"
    case controllerAnalog = "/MessageHandler/OnControllerAnalog"
    case controllerButtonPressed = "/MessageHandler/OnControllerButtonPressed"
    case controllerButtonReleased = "/MessageHandler/OnControllerButtonReleased"
}

