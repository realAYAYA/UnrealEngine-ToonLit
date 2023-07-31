//
//  StarscreamProvider.swift
//  WebRTC-Demo
//
//  Created by stasel on 15/07/2019.
//  Copyright © 2019 stasel. All rights reserved.
//

import Foundation
import Starscream

class StarscreamWebSocket: WebSocketProvider {
    
    var delegate: WebSocketProviderDelegate?
    private let socket: WebSocket
    
    init(url: URL) {
        self.socket = WebSocket(url: url)
        self.socket.delegate = self
    }
    
    func connect() {
        self.socket.connect()
    }
    
    func send(data: Data) {
        let stringifiedMsg = String(data: data, encoding: .utf8)
        if let msg = stringifiedMsg {
            self.send(msg: msg)
        }
        else {
            debugPrint("Could not convert data message to UTF8 string.")
        }
    }

    func send(msg: String) {
        self.socket.write(string: msg)
    }
    
    func close() {
        self.socket.disconnect(forceTimeout: 2, closeCode: 1000 /*normal close*/)
    }
}

extension StarscreamWebSocket: Starscream.WebSocketDelegate {
    func websocketDidConnect(socket: WebSocketClient) {
        self.delegate?.webSocketDidConnect(self)
    }
    
    func websocketDidDisconnect(socket: WebSocketClient, error: Error?) {
        self.delegate?.webSocketDidDisconnect(self, error: error)
    }
    
    func websocketDidReceiveMessage(socket: WebSocketClient, text: String) {
        debugPrint("Warning: Expected to receive data format but received a string. Check the websocket server config.")
    }
    
    func websocketDidReceiveData(socket: WebSocketClient, data: Data) {
        self.delegate?.webSocketDidReceiveData(self, data: data)
    }
    
    
}
