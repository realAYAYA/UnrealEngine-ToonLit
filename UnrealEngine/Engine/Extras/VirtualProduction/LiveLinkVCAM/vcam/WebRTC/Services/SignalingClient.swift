//
//  SignalClient.swift
//  WebRTC
//
//  Created by Stasel on 20/05/2018.
//  Copyright © 2018 Stasel. All rights reserved.
//

import Foundation
import WebRTC

protocol SignalClientDelegate: AnyObject {
    func signalClientDidConnect(_ signalClient: SignalingClient)
    func signalClientDidDisconnect(_ signalClient: SignalingClient, error: Error?)
    func signalClientDidReceiveError(_ signalClient: SignalingClient, error: Error?)
    func signalClient(_ signalClient: SignalingClient, didReceiveRemoteSdp sdp: RTCSessionDescription)
    func signalClient(_ signalClient: SignalingClient, didReceiveCandidate candidate: RTCIceCandidate)
    func signalClient(_ signalClient: SignalingClient, didReceiveConfig config: RTCConfiguration)
}

final class SignalingClient {
    
    private let decoder = JSONDecoder()
    private let encoder = JSONEncoder()
    private let webSocket: WebSocketProvider
    private var shouldReconnect : Bool = true
    weak var delegate: SignalClientDelegate?
    
    init(webSocket: WebSocketProvider) {
        self.webSocket = webSocket
    }
    
    func connect() {
        self.webSocket.delegate = self
        self.webSocket.connect()
    }
    
    func close() {
        self.shouldReconnect = false
        self.webSocket.close()
        self.webSocket.delegate = nil
    }
    
    func send(sdp rtcSdp: RTCSessionDescription) {
        let message = Message.sdp(SessionDescription(from: rtcSdp))
        do {
            let dataMessage = try self.encoder.encode(message)
            
            self.webSocket.send(data: dataMessage)
        }
        catch {
            debugPrint("Warning: Could not encode sdp: \(error)")
        }
    }
    
    func send(candidate rtcIceCandidate: RTCIceCandidate) {
        let message = Message.candidate(IceCandidate(from: rtcIceCandidate))
        do {
            let dataMessage = try self.encoder.encode(message)
            self.webSocket.send(data: dataMessage)
        }
        catch {
            debugPrint("Warning: Could not encode candidate: \(error)")
        }
    }
}


extension SignalingClient: WebSocketProviderDelegate {
    
    func webSocketDidConnect(_ webSocket: WebSocketProvider) {
        self.delegate?.signalClientDidConnect(self)
    }
    
    func webSocketDidDisconnect(_ webSocket: WebSocketProvider, error: Error?) {
        self.delegate?.signalClientDidDisconnect(self, error: error)
        
        // try to reconnect every two seconds
        DispatchQueue.global().asyncAfter(deadline: .now() + 2) {
            if self.shouldReconnect {
                debugPrint("Trying to reconnect to signaling server...")
                self.webSocket.connect()
            }
        }
    }
    
    func webSocketReceiveError(_ webSocket: WebSocketProvider, error: Error?) {
        self.delegate?.signalClientDidReceiveError(self, error: error)
    }
    
    // Handle a message sent over websocket
    func webSocketDidReceiveData(_ webSocket: WebSocketProvider, data: Data) {
        
        // Unescape string
        var dataStr = String(decoding: data, as: UTF8.self)
        dataStr = dataStr.replacingOccurrences(of: "\\\"", with: "\"")
        
        let message: Message
        do {
            message = try self.decoder.decode(Message.self, from: dataStr.data(using: .utf8)!)
        }
        catch {
            let dataStr = String(decoding: data, as: UTF8.self)
            debugPrint("Warning: Could not decode incoming message: \(error) - Message= \(dataStr)")
            return
        }
        
        switch message {
        case .config(let config):
            self.delegate?.signalClient(self, didReceiveConfig: config.rtcConfiguration)
        case .candidate(let iceCandidate):
            self.delegate?.signalClient(self, didReceiveCandidate: iceCandidate.rtcIceCandidate)
        case .sdp(let sessionDescription):
            self.delegate?.signalClient(self, didReceiveRemoteSdp: sessionDescription.rtcSessionDescription)
        case .playerCount(let playerCount):
            print("Got player count=\(playerCount)")
        }
    }
    
}
