//
//  RemoteSessionStreamingConnection.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import CocoaAsyncSocket
import CoreVideo
import QuickLayout

class LiveLinkLogger : LiveLinkLogDelegate {

    static let instance = LiveLinkLogger()
    
    func logMessage(_ message: String!) {
        Log.info(message)
    }

}


class RemoteSessionStreamingConnection : StreamingConnection  {
    
    static var DefaultPort = UInt16(2049)

    static var _initializedLiveLink = false
    private var oscConnection : OSCTCPConnection?
    private var multicastWatchdogSocket  : GCDAsyncUdpSocket?
    private var _liveLink : LiveLinkProvider?
    private var _timer : Timer?
    
    private var _decoder : JPEGVideoDecoder?
    private var _decodedPixelBuffer : CVPixelBuffer?
    weak var remoteSessionView : RemoteSessionView?
    
    private var _recvHistory = [(Date,Int)]()

    var host : String?
    var port : UInt16?
    
    
    override var renderView : UIView? {
        didSet {
            let v = RemoteSessionView()
            v.delegate = self
            self.renderView?.addSubview(v)
            remoteSessionView = v
        }
    }
    
    override var name : String {
        get {
            "RemoteSession"
        }
    }
    
    override var subjectName: String! {
        didSet {
            self._liveLink?.removeCameraSubject(oldValue!)
            self._liveLink?.addCameraSubject(subjectName)
        }
    }
    
    override var destination : String {
        get {
            "\(self.host ?? "<notset>"):\(self.port ?? 0)"
        }
        set {
            (self.host,self.port) = NetUtility.hostAndPortFromAddress(newValue)
        }
    }

    override var isConnected: Bool {
        get {
            return self.oscConnection?.isConnected ?? false
        }
    }
    
    required init(subjectName: String) {
        super.init(subjectName: subjectName)

        // make sure LiveLink is initialized
        if RemoteSessionStreamingConnection._initializedLiveLink == false {
            LiveLink.initialize(LiveLinkLogger.instance)
            RemoteSessionStreamingConnection._initializedLiveLink = true
        }

        
        restartLiveLink()
    }

    override func connect() throws {
        self.oscConnection = nil
        self.oscConnection = try OSCTCPConnection(host:self.host ?? "", port:self.port ?? RemoteSessionStreamingConnection.DefaultPort, delegate: self)
        
        _timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true, block: { (t) in
            
            if let conn = self.oscConnection {
                if conn.isConnected {
                    conn.send(OSCAddressPattern.ping)
                } else {
                    conn.reconnect()
                }
            }
            
            let now = Date()
            var countToRemove = 0
            var totalBytesLastSecond = 0
            for item in self._recvHistory {
                if now.timeIntervalSince(item.0) > 1 {
                    countToRemove += 1
                } else {
                    totalBytesLastSecond += item.1
                }
            }

            self._recvHistory.removeFirst(countToRemove)

            self.stats = StreamingConnectionStats()
            self.stats?.bytesPerSecond = totalBytesLastSecond

            if self._recvHistory.count > 1,
               let first = self._recvHistory.first,
               let last = self._recvHistory.last {
                
                self.stats?.framesPerSecond = Float(self._recvHistory.count - 1) / Float(last.0.timeIntervalSince(first.0))
            }

        })
    }
    
    override func reconnect() {
        
        oscConnection?.reconnect()
    }
    
    override func disconnect() {
        
        _timer?.invalidate()
        _timer = nil
        
        oscConnection?.disconnect()
        _decoder = nil
    }
    
    override func sendTransform(_ transform: simd_float4x4, atTime time: Double) {
        self._liveLink?.updateSubject(AppSettings.shared.liveLinkSubjectName, withTransform: transform, atTime: time)
    }
    
    func restartLiveLink() {

        // stop the provider & restart livelink here
        if self._liveLink != nil {
            Log.info("Restarting LiveLink.")
            LiveLink.restart()
            self._liveLink = nil
        }

        Log.info("Initializing LiveLink Provider.")

        self._liveLink = LiveLink.createProvider("Live Link VCAM")
        self._liveLink?.addCameraSubject(AppSettings.shared.liveLinkSubjectName)

        multicastWatchdogSocket?.close()
        Log.info("Starting multicast watchdog.")
        multicastWatchdogSocket = GCDAsyncUdpSocket(delegate: self, delegateQueue: DispatchQueue.main)
        do {
            try multicastWatchdogSocket?.enableReusePort(true)
            try multicastWatchdogSocket?.bind(toPort: 6665)
            try multicastWatchdogSocket?.joinMulticastGroup("230.0.0.1")
            try multicastWatchdogSocket?.beginReceiving()
        } catch {
            Log.info("Error creating watchdog : \(error.localizedDescription)")
        }
    }
    
    func decode(_ width : Int32, _ height : Int32, _ blob : Data) {

        if _decoder == nil {
            _decoder = JPEGVideoDecoder()
        }

        _recvHistory.append((Date(), blob.count))
        
        self._decoder?.decode(width: width, height: height, data: blob) { (pixelBuffer) in
            
            if let pb = pixelBuffer {
                
                DispatchQueue.main.async {
                    self.remoteSessionView?.pixelBuffer = pb
                    self.videoSize = CGSize(width: CVPixelBufferGetWidth(pb), height: CVPixelBufferGetHeight(pb))
                }
            }
            
        }
    }
}

extension RemoteSessionStreamingConnection : RemoteSessionViewDelegate {
    
    func remoteSessionView(_ view: RemoteSessionView?, touch type: StreamingConnectionTouchType, index: Int, at point: CGPoint, force: CGFloat) {
        
        guard let rsv = remoteSessionView else { return }
        
        let normalizedPoint = CGPoint(x: point.x / rsv.frame.size.width, y: point.y / rsv.frame.size.height)
        let data = OSCUtility.ueTouchData(point: normalizedPoint, finger: index, force: force)
        
        //Log.info("\(type.rawValue) @ \(normalizedPoint)")
        
        var pattern : OSCAddressPattern!
        switch type {
        case .began:
            pattern = .touchStarted
        case .moved:
            pattern = .touchMoved
        case .ended:
            pattern = .touchEnded
        }
        
        self.oscConnection?.send(pattern, arguments: [ OSCArgument.blob(data) ])

    }
}


extension RemoteSessionStreamingConnection : GCDAsyncUdpSocketDelegate {

    func udpSocketDidClose(_ sock: GCDAsyncUdpSocket, withError error: Error?) {
        Log.error("Multicast watchdog closed : restarting LiveLink.")
        self.restartLiveLink()
    }
}

extension RemoteSessionStreamingConnection : OSCTCPConnectionDelegate {
    
    func oscConnectionDidConnect(_ connection: OSCTCPConnection) {
        Log.info("RemoteSession connection initiated.")

    }
    
    func oscConnectionDidDisconnect(_ connection: OSCTCPConnection, withError err: Error?) {
        self.delegate?.streamingConnection(self, didDisconnectWithError: err)
    }
    
    func oscConnection(_ connection: OSCTCPConnection, didReceivePacket packet: OSCPacket) {
        
        if let msg = packet as? OSCPacketMessage {
            
            //Log.info(msg.debugString())
            
            switch msg.addressPattern {

            case OSCAddressPattern.rsHello.rawValue:
                if let args = msg.arguments {
                    if args.count == 1,
                        case let OSCArgument.string(version) = args[0] {
                        connection.send(OSCAddressPattern.rsHello, arguments: [ OSCArgument.string(version) ])
                    }
                }

            case OSCAddressPattern.rsGoodbye.rawValue:
                connection.disconnect()
                if let args = msg.arguments {
                    if args.count == 1,
                        case let OSCArgument.string(version) = args[0] {
                        Log.info("RemoteSession closed by server : \(version)")
                    }
                }
                
            case OSCAddressPattern.rsChannelList.rawValue:
                connection.send(OSCAddressPattern.rsChangeChannel, arguments: [ OSCArgument.string("FRemoteSessionImageChannel"), OSCArgument.string("Write"), OSCArgument.int32(1)] )
                connection.send(OSCAddressPattern.rsChangeChannel, arguments: [ OSCArgument.string("FRemoteSessionInputChannel"), OSCArgument.string("Read"), OSCArgument.int32(1)] )
                
                self.delegate?.streamingConnectionDidConnect(self)

            case OSCAddressPattern.screen.rawValue:

                if let args = msg.arguments {
                    if args.count == 4,
                        case let OSCArgument.int32(width) = args[0],
                        case let OSCArgument.int32(height) = args[1],
                       case let OSCArgument.blob(blob) = args[2] {
                        
                        //Log.info("RECV frame \(frame) : \(width)x\(height) \(blob.count) bytes")
                        self.decode(width, height, blob)
                    }
                }
                
            case OSCAddressPattern.ping.rawValue:
                connection.send(OSCAddressPattern.ping)


            default:
                break
            }
        }
    }
    
    
}


