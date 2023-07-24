//
//  WebRTCStreamingConnection.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import WebRTC

class WebRTCStreamingConnection : StreamingConnection {
    var _url : URL?
    
    public var webRTCClient: WebRTCClient?
    private var webRTCClientState : RTCIceConnectionState?
    private var signalClient: SignalingClient?
    private var touchControls: TouchControls?
    private var webRTCView : WebRTCView?
    private var rtcVideoTrack : RTCVideoTrack?
    
    private var signalingConnected = false
    private var hasRemoteSdp = false
    private var hasLocalSdp = false
    private var remoteCandidateCount = 0
    private var localCandidateCount = 0
    private var _statsTimer : Timer?
    private var _lastBytesReceived : Int?
    private var _lastBytesReceivedTimestamp : CFTimeInterval?

    override var name : String {
        get {
            StreamingConnectionType.webRTC.rawValue
        }
    }
    override var destination : String {
        get {
            self._url?.absoluteString ?? ""
        }
        set {
            self._url = URL(string: "ws://\(newValue):80")!
        }
    }
    
    override var isConnected: Bool {
        get {
            return (self.webRTCClientState ?? .disconnected) == .connected
        }
    }
    
    override var relayTouchEvents: Bool {
        didSet {
            self.touchControls?.relayTouchEvents = relayTouchEvents
        }
    }

    override var renderView: UIView? {
        didSet {
            if let rv = renderView {
                let rtcView = WebRTCView(frame: CGRect(x: 0, y: 0, width: rv.frame.size.width, height: rv.frame.size.height))
                rv.addSubview(rtcView)
                rtcView.delegate = self
                rtcView.layoutToSuperview(.top, .bottom, .left, .right)
                self.webRTCView = rtcView
                
                self.attachVideoTrack()
            }
        }
    }
    
    required init(subjectName: String) {
        super.init(subjectName: subjectName)
        
        self.webRTCClient = WebRTCClient()
        self.webRTCClient?.speakerOff()
        self.webRTCClient?.delegate = self

        _statsTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true, block: { timer in
            
            if let webRTC = self.webRTCClient {
                
                webRTC.stats({ report in
                    
                    for (key,value) in report.statistics {
                        if key.starts(with: "RTCIceCandidatePair_") {
                            //Log.info("\(value.values)")
                            let timestamp = value.timestamp_us
                            if let bytesReceived = value.values["bytesReceived"] as? Int {
                                
                                if bytesReceived > 0 {

                                    var bytesPerSecond : Int?
                                    if let previousTimestamp = self._lastBytesReceivedTimestamp,
                                       let previousBytesReceived = self._lastBytesReceived {

                                        bytesPerSecond = Int(Double(bytesReceived - previousBytesReceived) / ((timestamp - previousTimestamp) / 1000000.0))
                                    }

                                    self._lastBytesReceivedTimestamp = timestamp
                                    self._lastBytesReceived = bytesReceived
                                    
                                    if let bps = bytesPerSecond {
                                        self.stats = StreamingConnectionStats()
                                        self.stats?.bytesPerSecond = bps
                                    }
                                }
                            }
                            
                            break
                        }
                    }
                })
            } else {
                self._lastBytesReceived = nil
                self._lastBytesReceivedTimestamp = nil
                self.stats = nil
            }
        })
    }
    
    deinit {
        
        Log.info("Destroyed WebRTCStreamingConnection")

        webRTCClient = nil
    }

    override func shutdown() {
        _statsTimer?.invalidate()
        disconnect()
    }
    
    override func connect() throws {
        
        if signalClient == nil {
            
            // We will use 3rd party library for websockets.
            let webSocketProvider: WebSocketProvider
            
            if #available(iOS 13.0, *) {
                webSocketProvider = NativeWebSocketProvider(url: self._url!, timeout: 2.0)
            } else {
                webSocketProvider = StarscreamWebSocket(url: self._url!)
            }
            
            self.signalClient = SignalingClient(webSocket: webSocketProvider)
            self.signalClient?.delegate = self
            self.signalClient?.connect()

        }
    }
    
    override func reconnect() {
        //disconnect()
        //do {
        //    try connect()
        //} catch {
        //    Log.error(error.localizedDescription)
        //}
    }
    
    override func disconnect() {
        signalClient?.delegate = nil
        signalClient?.close()
        signalClient = nil

        signalingConnected = false
        hasLocalSdp = false
        hasRemoteSdp = false
        remoteCandidateCount = 0
        localCandidateCount = 0
    }
    
    override func sendTransform(_ transform: simd_float4x4, atTime time: Double) {
    
        guard let client = webRTCClient else { return }
    
        
        // convert the transform to UE space
        // adapted from AppleARKitConversion.h
        let rawRotation = simd_quaternion(transform)
        let ueRotation = simd_quaternion(-rawRotation.vector.z, rawRotation.vector.x, rawRotation.vector.y, -rawRotation.vector.w)
        var ueTransform = simd_float4x4(ueRotation)
        ueTransform.columns.3 = simd_float4(x: -transform.columns.3.z, y: transform.columns.3.x, z: transform.columns.3.y, w: 1.0) * 100.0 // ue units
        ueTransform.columns.3.w = 1.0 // Force Unit scale
        
        var bytes: [UInt8] = []

        // Write message type using 1 byte
        bytes.append(PixelStreamingToStreamerMessage.Transform.rawValue)

        // Write 4x4 transform each element to get 4 bytes e.g. float -> [UInt8]
        bytes.append(contentsOf: ueTransform.columns.0.x.toBytes())
        bytes.append(contentsOf: ueTransform.columns.0.y.toBytes())
        bytes.append(contentsOf: ueTransform.columns.0.z.toBytes())
        bytes.append(contentsOf: ueTransform.columns.0.w.toBytes())

        bytes.append(contentsOf: ueTransform.columns.1.x.toBytes())
        bytes.append(contentsOf: ueTransform.columns.1.y.toBytes())
        bytes.append(contentsOf: ueTransform.columns.1.z.toBytes())
        bytes.append(contentsOf: ueTransform.columns.1.w.toBytes())

        bytes.append(contentsOf: ueTransform.columns.2.x.toBytes())
        bytes.append(contentsOf: ueTransform.columns.2.y.toBytes())
        bytes.append(contentsOf: ueTransform.columns.2.z.toBytes())
        bytes.append(contentsOf: ueTransform.columns.2.w.toBytes())

        bytes.append(contentsOf: ueTransform.columns.3.x.toBytes())
        bytes.append(contentsOf: ueTransform.columns.3.y.toBytes())
        bytes.append(contentsOf: ueTransform.columns.3.z.toBytes())
        bytes.append(contentsOf: ueTransform.columns.3.w.toBytes())

        // Write timestamp 8 bytes
        bytes.append(contentsOf: time.toBytes())

        // Send the transform + timestamp across
        client.sendData(Data(bytes))
    }
    
    func inputTypeToIndex(_ type : StreamingConnectionControllerInputType) -> UInt8? {
        switch type {
        case .faceButtonBottom : return 0
        case .faceButtonRight : return 1
        case .faceButtonLeft : return 2
        case .faceButtonTop : return 3
            
        case .shoulderButtonLeft : return 4
        case .shoulderButtonRight : return 5

        case .triggerButtonLeft : return 6
        case .triggerButtonRight : return 7

        case .specialButtonLeft : return 8
        case .specialButtonRight : return 9
            
        case .thumbstickLeftButton : return 10
        case .thumbstickRightButton : return 11
            
        case .dpadUp : return 12
        case .dpadDown : return 13
        case .dpadLeft : return 14
        case .dpadRight : return 15
            
        case .thumbstickLeftX : return 1
        case .thumbstickLeftY : return 2
        case .thumbstickRightX : return 3
        case .thumbstickRightY : return 4
        }
    }
    
    override func sendControllerAnalog(_ type : StreamingConnectionControllerInputType, controllerIndex : UInt8, value : Float) {
        
        guard let client = webRTCClient else { return }

        guard let analogIndex = inputTypeToIndex(type) else {
            Log.warning("Couldn't find an index for controller input type \(type.rawValue)")
            return
        }
        
        var bytes: [UInt8] = []

        // Write message type using 1 byte
        bytes.append(PixelStreamingToStreamerMessage.GamepadAnalog.rawValue)

        bytes.append(controllerIndex)
        bytes.append(analogIndex)
        bytes.append(contentsOf: Double(value).toBytes())
        
        client.sendData(Data(bytes))
    }
    
    override func sendControllerButtonPressed(_ type : StreamingConnectionControllerInputType, controllerIndex : UInt8, isRepeat : Bool) {

        guard let client = webRTCClient else { return }

        guard let buttonIndex = inputTypeToIndex(type) else {
            Log.warning("Couldn't find an index for controller input type \(type.rawValue)")
            return
        }

        var bytes: [UInt8] = []

        bytes.append(PixelStreamingToStreamerMessage.GamepadButtonPressed.rawValue)
        bytes.append(controllerIndex)
        bytes.append(buttonIndex)
        bytes.append(UInt8(isRepeat ? 1 : 0))
        
        client.sendData(Data(bytes))
    }

   override func sendControllerButtonReleased(_ type : StreamingConnectionControllerInputType, controllerIndex : UInt8) {

       guard let client = webRTCClient else { return }

       guard let buttonIndex = inputTypeToIndex(type) else {
           Log.warning("Couldn't find an index for controller input type \(type.rawValue)")
           return
       }

       var bytes: [UInt8] = []

       bytes.append(PixelStreamingToStreamerMessage.GamepadButtonReleased.rawValue)
       bytes.append(controllerIndex)
       bytes.append(buttonIndex)
       
       client.sendData(Data(bytes))
    }

    
    func attachVideoTrack() {
        if let webRTC = webRTCClient, let view = self.webRTCView, let track = self.rtcVideoTrack {
            self.touchControls = TouchControls(webRTC, touchView: view)
            view.attachVideoTrack(track: track)
            view.attachTouchDelegate(delegate: self.touchControls!)
        }
    }
}
extension WebRTCStreamingConnection : WebRTCViewDelegate {
    
    func webRTCView(_ view: WebRTCView, didChangeVideoSize size: CGSize) {
        self.videoSize = size
    }
}

extension WebRTCStreamingConnection: SignalClientDelegate {
    
    func signalClientDidConnect(_ signalClient: SignalingClient) {
        self.signalingConnected = true
        Log.info("Connected to signaling server")
    }
    
    func signalClientDidDisconnect(_ signalClient: SignalingClient, error: Error?) {
        self.signalingConnected = false
        Log.info("Disconnected from signaling server")
        self.delegate?.streamingConnection(self, didDisconnectWithError: error)
    }
    
    func signalClientDidReceiveError(_ signalClient: SignalingClient, error: Error?) {
        if error != nil {
            Log.error("Signalling got error: \(error!)")
        }
    }
    
    func signalClient(_ signalClient: SignalingClient, didReceiveConfig config: RTCConfiguration) {
        Log.info("Received peer connection configuration - ICE servers: \(config.iceServers)")
        self.webRTCClient?.setupPeerConnection(rtcConfiguration: config)
    }
    
    func signalClient(_ signalClient: SignalingClient, didReceiveRemoteSdp sdp: RTCSessionDescription) {
        
        var sdpTypeStr : String = ""
        switch sdp.type {
        case RTCSdpType.answer:
            sdpTypeStr = "answer"
        case RTCSdpType.offer:
            sdpTypeStr = "offer"
        case RTCSdpType.prAnswer:
            sdpTypeStr = "prAnswer"
        case RTCSdpType.rollback:
            sdpTypeStr = "rollback"
        @unknown default:
            sdpTypeStr = "unknown"
        }
        
        Log.info("Received remote sdp. Type=\(sdpTypeStr)")
        Log.info(sdp.sdp)
        
        if self.webRTCClient!.hasPeerConnnection() {
            self.webRTCClient!.handleRemoteSdp(remoteSdp: sdp) { (error) in
                self.hasRemoteSdp = true
                
                // If we get an offer from the streamer we send an answer back
                if sdp.type == RTCSdpType.offer {
                    self.signalClientSendAnswer(signalClient)
                }
                else {
                    Log.debug("We only support replying to offer, but we got \(sdpTypeStr)")
                }
            }
        } else {
            Log.debug("WebRTC peer connection not setup yet - cannot handle remote sdp.")
        }
    }
    
    func signalClient(_ signalClient: SignalingClient, didReceiveCandidate candidate: RTCIceCandidate) {
        Log.info("Received remote candidate - \(candidate.sdp)")
        
        if self.webRTCClient!.hasPeerConnnection() {
            self.webRTCClient!.handleRemoteCandidate(remoteCandidate: candidate) { error in
                self.remoteCandidateCount += 1
            }
        } else {
            Log.debug("WebRTC peer connection not setup yet - cannot handle remote candidate")
        }
    }
    
    func signalClientSendAnswer(_ signalClient: SignalingClient){
        if self.webRTCClient!.hasPeerConnnection() {
            Log.info("Sending answer sdp")
            self.webRTCClient!.answer { (localSdp) in
                Log.info(localSdp.sdp)
                self.hasLocalSdp = true
                signalClient.send(sdp: localSdp)
            }
        } else {
            Log.debug("WebRTC peer connection not setup yet - cannot handle sending answer.")
        }
    }
    
}

extension WebRTCStreamingConnection: WebRTCClientDelegate {
    
    func webRTCClient(_ client: WebRTCClient, onStartReceiveVideo video: RTCVideoTrack) {
        self.rtcVideoTrack = video
        self.attachVideoTrack()
        self.delegate?.streamingConnectionDidConnect(self)
    }
    
    func webRTCClient(_ client: WebRTCClient, didDiscoverLocalCandidate candidate: RTCIceCandidate) {
        Log.info("discovered local candidate")
        self.localCandidateCount += 1
        self.signalClient?.send(candidate: candidate)
    }
    
    func webRTCClient(_ client: WebRTCClient, didChangeConnectionState state: RTCIceConnectionState) {
        self.webRTCClientState = state
        switch state {
        case .connected:
            
            // WebRTC connection is "connected" so try to show remote video track if we got one some time during the connection
            // IMPORTANT: Even though we do the same in onStartReceiveVideo above, this one actually makes the video show up due
            // peer connection needing to be connected before video can reasonably be displayed - the other callback is more useful
            // if video is added later on into the call.
            self.rtcVideoTrack = self.webRTCClient?.getRemoteVideoTrack()
            self.attachVideoTrack()
            
        case .disconnected:
            self.delegate?.streamingConnection(self, didDisconnectWithError: nil)
        case .failed:
            self.delegate?.streamingConnection(self, didDisconnectWithError: NSError(domain: "", code: 1, userInfo: [NSLocalizedDescriptionKey : "Failed to connect." ] ))
        default:
            break
        }
        Log.info("WebRTC status: \(state.description.capitalized)")
    }
    
    func webRTCClient(_ client: WebRTCClient, didReceiveData data: Data) {
        
        if(data.count > 0) {
            let payloadTypeInt : UInt8 = data[0]
            if let payloadType = PixelStreamingToClientMessage(rawValue: payloadTypeInt) {
                switch payloadType {
                case .VideoEncoderAvgQP:
                    let qp : String? = String(data: data.dropFirst(), encoding: .utf16LittleEndian)
                    Log.info("Quality = \(qp ?? "N/A")")
                case .QualityControlOwnership:
                    fallthrough
                case .Response:
                    fallthrough
                case .Command:
                    fallthrough
                case .FreezeFrame:
                    fallthrough
                case .UnfreezeFrame:
                    fallthrough
                case .LatencyTest:
                    fallthrough
                case .InitialSettings:
                    fallthrough
                case .FileExtension:
                    fallthrough
                case .FileMimeType:
                    fallthrough
                case .FileContents:
                    print("Skipping payload type \(payloadType) - we have no implementation for it.")
                }
            }
        }
    }
}
