//
//  File.swift
//  WebRTC-Demo
//
//  Created by stasel on 15/07/2019.
//  Copyright © 2019 stasel. All rights reserved.
//

import Foundation

protocol WebSocketProvider: AnyObject {
    var delegate: WebSocketProviderDelegate? { get set }
    func connect()
    func close()
    func send(data: Data)
    func send(msg: String)
}

protocol WebSocketProviderDelegate: AnyObject {
    func webSocketDidConnect(_ webSocket: WebSocketProvider)
    func webSocketDidDisconnect(_ webSocket: WebSocketProvider, error: Error?)
    func webSocketReceiveError(_ webSocket: WebSocketProvider, error: Error?)
    func webSocketDidReceiveData(_ webSocket: WebSocketProvider, data: Data)
}
