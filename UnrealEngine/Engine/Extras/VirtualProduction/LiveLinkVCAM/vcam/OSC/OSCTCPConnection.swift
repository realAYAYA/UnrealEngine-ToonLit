//
//  OSCTCPConnection.swift
//  VCAM
//
//  Created by Brian Smith on 12/13/19.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation
import CocoaAsyncSocket

enum OSCTCPConnectionError: Error {
    case connectionFailed
}

protocol OSCTCPConnectionDelegate : AnyObject {
    
    func oscConnectionDidConnect(_ connection : OSCTCPConnection)
    func oscConnectionDidDisconnect(_ connection : OSCTCPConnection, withError err: Error?)
    func oscConnection(_ connection : OSCTCPConnection, didReceivePacket packet: OSCPacket)

}

class OSCTCPConnection : NSObject {

    var isConnected : Bool {
        get {
            return socket.isConnected
        }
    }
    
    var socket : GCDAsyncSocket!
    weak var delegate : OSCTCPConnectionDelegate?
    
    var host = String()
    var port : UInt16 = 2049
    
    static var TagDisconnect : Int = -1

    var incomingData = Data()

    init(host : String, port : UInt16, delegate: OSCTCPConnectionDelegate) throws {

        super.init()
        self.host = host
        self.port = port
        self.delegate = delegate
        
        // create a socket & bind to listen the given port.
        socket = GCDAsyncSocket(delegate: self, delegateQueue: DispatchQueue(label: "GCDAsyncSocket Delegate"))
        try socket.connect(toHost: host, onPort: port, withTimeout: 5)
    }
    
    deinit {
        Log.info("OSCTCPConnection shutting down.")
        disconnect()
    }
    
    func reconnect() {
        if socket.isDisconnected {
            do {
                try socket.connect(toHost: host, onPort: port)
            } catch {
            }
        }
    }
    
    func disconnect() {
        if socket.isConnected {
            send(OSCAddressPattern.rsGoodbye, arguments: [ OSCArgument.string("Client disconnecting")], tag : OSCTCPConnection.TagDisconnect )
        }
    }
    
    func send(_ packet : OSCPacket, tag : Int = 0) {

        do {
            if let s = socket {

                //let pm = packet as! OSCPacketMessage
                //Log.info("sending \(pm.debugString())")
                
                let data = try packet.toData(prependSize : true)
                
                s.write(data, withTimeout: 1000, tag: tag)
            } else {
                Log.error("OSCConnection.send() : no socket.");
            }
        } catch {
            Log.error("OSCConnection.send() : \(error.localizedDescription)");
        }
    }

    func send(_ addressPattern : OSCAddressPattern, tag : Int = 0) {

        send(OSCPacketMessage(addressPattern, arguments : nil), tag : tag)
    }
    
    func send(_ addressPattern : OSCAddressPattern, arguments : [OSCArgument]?, tag : Int = 0) {

        send(OSCPacketMessage(addressPattern, arguments : arguments), tag : tag)
    }
}


extension OSCTCPConnection : GCDAsyncSocketDelegate {
    
    func socket(_ sock: GCDAsyncSocket, didWriteDataWithTag tag: Int) {
        
        if tag == OSCTCPConnection.TagDisconnect {
            sock.disconnect()
        }
        
    }
    
    func socket(_ sock: GCDAsyncSocket, didRead data: Data, withTag tag: Int) {
    
        do {

            //Log.info("did Read data of \(data.count) bytes")

            incomingData.append(data)

            while (incomingData.count >= 4) {

                
                //Log.info("incomingData is \(incomingData.count) bytes")
                
                let lengthData = incomingData.subdata(in: incomingData.startIndex..<(incomingData.startIndex + 4))
                let length = Int(lengthData.withUnsafeBytes { $0.load(as: UInt32.self) })
                //Log.info("message length is \(length) bytes")

                if incomingData.count >= (length + 4) {
                    
                    // notify the delegate
                    if let d = delegate {

                        // get the data for this single message
                        let messageData = incomingData.subdata(in: (incomingData.startIndex + 4)..<(incomingData.startIndex + length + 4))
                        
                        // decode the data as an OSC Packet Message
                        let oscPacketMessage = try OSCPacketMessage(messageData)

                        d.oscConnection(self, didReceivePacket: oscPacketMessage)
                    }
                    
                } else {
                    
                    // we have a partial message, we are done processing this data until more arrives
                    break
                }

                incomingData = incomingData.suffix(from: incomingData.startIndex + 4 + length)
                //Log.info("incomingData now is \(incomingData.count) bytes")

            }
            
           incomingData = Data(incomingData)
           //Log.info("partial incomingData is \(incomingData.count) bytes")
            
        } catch {
            Log.error("OSCTCPConnection : Couldn't parse the incoming data : \(error.localizedDescription)")
        }

        // read some more
        sock.readData(withTimeout: 10, tag: 0)
    }

    func socket(_ sock: GCDAsyncSocket, didConnectToHost host: String, port: UInt16) {

        // start a read
        incomingData.removeAll()
        sock.readData(withTimeout: 10, tag: 0)
        
        delegate?.oscConnectionDidConnect(self)
    }
    
    func socketDidDisconnect(_ sock: GCDAsyncSocket, withError err: Error?) {
        delegate?.oscConnectionDidDisconnect(self, withError : err)
    }
}
