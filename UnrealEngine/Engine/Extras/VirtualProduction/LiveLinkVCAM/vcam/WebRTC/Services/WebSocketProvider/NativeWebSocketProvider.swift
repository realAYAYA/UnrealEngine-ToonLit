//
//  NativeWebSocketProvider.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 7/6/2022.
//  Copyright © 2022 Stas Seldin. All rights reserved.
//

import Foundation

@available(iOS 13.0, *)
class NativeWebSocketProvider : NSObject, WebSocketProvider, URLSessionWebSocketDelegate {
    
    var delegate                            : WebSocketProviderDelegate?
    private var socket                      : URLSessionWebSocketTask!
    private var timeout                     : TimeInterval!
    private var url                         : URL!
    private(set) var isConnected            : Bool                      = false
    
    init(url:URL,timeout:TimeInterval) {
        self.timeout        = timeout
        self.url            = url
        super.init()
    }
    
    // do not move create socket to init method because if you want to reconnect it never connect again
    public func connect() {
        let configuration                        = URLSessionConfiguration.default
        let urlSession                           = URLSession(configuration: configuration, delegate: self, delegateQueue: OperationQueue())
        let urlRequest                           = URLRequest(url: url,timeoutInterval: timeout)
        socket                                   = urlSession.webSocketTask(with: urlRequest)
        socket.resume()
        readMessage()
    }
    
    func close() {
        let error = NSError(domain: "NativeWebSocketProvider", code: 0, userInfo: [NSLocalizedDescriptionKey : "Manually closed websocket."])
        closeConnection(error)
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
        socket.send(.string(msg)) { error in
            self.handleError(error)
        }
    }
    
    private func readMessage() {
        socket.receive { result in
            switch result {
            case .failure(_):
                break
            case .success(let message):
                switch message {
                case .data(let data):
                    self.delegate?.webSocketDidReceiveData(self, data: data)
                case .string(let string):
                    self.delegate?.webSocketDidReceiveData(self, data: string.data(using: .utf8)!)
                @unknown default:
                    print("Unimplemented case found in NativeWebSocketProvider.")
                }
                self.readMessage()
            }
        }
    }
    
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didOpenWithProtocol protocol: String?) {
        isConnected = true
        delegate?.webSocketDidConnect(self)
    }
    
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        
        var closeCodeStr : String
        switch closeCode {
        case URLSessionWebSocketTask.CloseCode.invalid:
            closeCodeStr = "invalid"
        case URLSessionWebSocketTask.CloseCode.normalClosure:
            closeCodeStr = "normalClosure"
        case URLSessionWebSocketTask.CloseCode.goingAway:
            closeCodeStr = "goingAway"
        case URLSessionWebSocketTask.CloseCode.protocolError:
            closeCodeStr = "protocolError"
        case URLSessionWebSocketTask.CloseCode.unsupportedData:
            closeCodeStr = "unsupportedData"
        case URLSessionWebSocketTask.CloseCode.noStatusReceived:
            closeCodeStr = "noStatusReceived"
        case URLSessionWebSocketTask.CloseCode.abnormalClosure:
            closeCodeStr = "abnormalClosure"
        case URLSessionWebSocketTask.CloseCode.invalidFramePayloadData:
            closeCodeStr = "invalidFramePayloadData"
        case URLSessionWebSocketTask.CloseCode.policyViolation:
            closeCodeStr = "policyViolation"
        case URLSessionWebSocketTask.CloseCode.messageTooBig:
            closeCodeStr = "messageTooBig"
        case URLSessionWebSocketTask.CloseCode.mandatoryExtensionMissing:
            closeCodeStr = "mandatoryExtensionMissing"
        case URLSessionWebSocketTask.CloseCode.internalServerError:
            closeCodeStr = "internalServerError"
        case URLSessionWebSocketTask.CloseCode.tlsHandshakeFailure:
            closeCodeStr = "tlsHandshakeFailure"
        @unknown default:
            closeCodeStr = "unknown close code"
        }
            
        
        if let reason = reason {
            let reasonStr = String(decoding: reason, as: UTF8.self)
            print("Websocket closed (\(closeCodeStr)) because: \(reasonStr)")
        }
        else {
            print("Websocket closed without reason message - (\(closeCodeStr))")
        }
        isConnected = false
    }
    
    ///never call delegate?.webSocketDidDisconnect in this method it leads to close next connection
    func urlSession(_ session: URLSession, didReceive challenge: URLAuthenticationChallenge, completionHandler: @escaping (URLSession.AuthChallengeDisposition, URLCredential?) -> Void) {
        completionHandler(.useCredential, URLCredential(trust: challenge.protectionSpace.serverTrust!))
    }
    
    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        closeConnection(error)
    }
    
    func urlSession(_ session: URLSession, didBecomeInvalidWithError error: Error?) {
        closeConnection(error)
    }
    
    func closeConnection(_ error : Error?) {
        socket.cancel(with: .goingAway, reason: nil)
        isConnected = false
        delegate?.webSocketDidDisconnect(self, error: error)
    }
    
    /// we need to check if error code is one of the 57 , 60 , 54 timeout no network and internet offline to notify delegate we disconnected from internet
    private func handleError(_ error: Error?){
        if let error = error as NSError?{
            if error.code == 57  || error.code == 60 || error.code == 54{
                closeConnection(error)
            }else{
                delegate?.webSocketReceiveError(self, error: error)
            }
        }
    }
}
