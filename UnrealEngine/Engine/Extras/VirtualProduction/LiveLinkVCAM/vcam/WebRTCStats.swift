//
//  WebRTCStats.swift
//  vcam
//
//  Created by TensorWorks on 24/7/2023.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation
import WebRTC


class WebRTCStats
{
    public var lastBitrate : Int64?
    public var lastFPS : Int64?
    private var lastFramesDecoded : Int64?
    private var lastBytesReceived : Int64?
    private var lastBytesReceivedTimestamp : CFTimeInterval?
    private var lastInterframeDelay : Double?
    private var lastProcessingDelay : Double?
    private var lastARKitEventTimestamp : CFTimeInterval?
    
    private var statsView : WebRTCStatsView
    private var fpsGraph : LineGraphView?
    private var framesReceivedGraph : LineGraphView?
    private var bitrateGraph : LineGraphView?
    private var widthGraph : LineGraphView?
    private var heightGraph : LineGraphView?
    private var jitterGraph : LineGraphView?
    private var jitterBufferDelayGraph : LineGraphView?
    private var multipacketFramesGraph : LineGraphView?
    private var freezeCountGraph : LineGraphView?
    private var pauseCountGraph : LineGraphView?
    private var nackCountGraph : LineGraphView?
    private var pliCountGraph : LineGraphView?
    private var packetLossGraph : LineGraphView?
    private var keyframesGraph : LineGraphView?
    private var framesDecodedGraph : LineGraphView?
    private var framesDroppedGraph : LineGraphView?
    private var interframeDelayGraph : LineGraphView?
    private var processingTimeGraph : LineGraphView?
    private var arkitEventsGraph : LineGraphView?
    private var arkitEventsProcessedGraph : LineGraphView?
    
    init(statsView: WebRTCStatsView) {
        
        self.statsView = statsView
        
        self.bitrateGraph = self.statsView.addGraph(graphName: "Bitrate (megabit/s)")
        self.bitrateGraph?.maxY = 20
        
        self.fpsGraph = self.statsView.addGraph(graphName: "FPS")
        
        self.framesReceivedGraph = self.statsView.addGraph(graphName: "Frames received")
        self.framesReceivedGraph?.maxY = 100
        
        self.multipacketFramesGraph = self.statsView.addGraph(graphName: "Multi-packet Frames")
        self.multipacketFramesGraph?.maxY = 10
        		
        self.jitterGraph = self.statsView.addGraph(graphName: "Jitter")
        self.jitterGraph?.maxY = 0.05
        self.jitterGraph?.yTickLabelFormatter = { return String(format: "%.2f", $0) }
        
        self.jitterBufferDelayGraph = self.statsView.addGraph(graphName: "Jitter Buffer Delay(ms)")

        self.packetLossGraph = self.statsView.addGraph(graphName: "Packets lost")
        self.packetLossGraph?.maxY = 10
        
        self.nackCountGraph = self.statsView.addGraph(graphName: "Nack Count")
        self.nackCountGraph?.maxY = 10

        self.freezeCountGraph = self.statsView.addGraph(graphName: "Freeze Count")
        self.freezeCountGraph?.maxY = 10
        
        self.pauseCountGraph = self.statsView.addGraph(graphName: "Pause Count")
        self.pauseCountGraph?.maxY = 10
        
        self.pliCountGraph = self.statsView.addGraph(graphName: "Picture Loss Count")
        self.pliCountGraph?.maxY = 10
        
        self.keyframesGraph = self.statsView.addGraph(graphName: "Keyframes Decoded")
        self.keyframesGraph?.maxY = 10
        
        self.framesDecodedGraph = self.statsView.addGraph(graphName: "Decode FPS")
        self.framesDecodedGraph?.maxY = 10
        
        self.framesDroppedGraph = self.statsView.addGraph(graphName: "Frames Dropped")
        self.framesDroppedGraph?.maxY = 10
        
        self.interframeDelayGraph = self.statsView.addGraph(graphName: "Interframe delay (ms)")
        self.interframeDelayGraph?.maxY = 10

        self.processingTimeGraph = self.statsView.addGraph(graphName: "Processing delay (ms)")
        self.processingTimeGraph?.maxY = 10
        
        self.arkitEventsGraph = self.statsView.addGraph(graphName: "ARKit Transform Delta (ms)")
        self.arkitEventsGraph?.maxY = 100
        
        self.arkitEventsProcessedGraph = self.statsView.addGraph(graphName: "ARKit Recv in UE / s")
        self.arkitEventsProcessedGraph?.maxY = 100
    }
    
    func processStatsReport(report: RTCStatisticsReport){
        
        for (_,value) in report.statistics {
            if let kind = value.values["kind"] as? String {
                if kind == "video" {
                    
                    //Log.info("\(value.values)")
                    
                    // Jitter buffer delay
                    if let jitterBufferDelay = value.values["jitterBufferDelay"] as? Double, let jitterBufferEmittedCount = value.values["jitterBufferEmittedCount"] as? UInt64 {
                        let jbDelayMs : Double = jitterBufferDelay / Double(jitterBufferEmittedCount) * 1000.0
                        self.jitterBufferDelayGraph?.addDataPoint(value: jbDelayMs)
                    }
                    
                    // Jitter
                    if let jitter = value.values["jitter"] as? Double {
                        self.jitterGraph?.addDataPoint(value: jitter)
                    }

                    // FPS
                    if let fps = value.values["framesPerSecond"] as? Double {
                        self.lastFPS = Int64(fps)
                        self.fpsGraph?.addDataPoint(value: fps)
                    }
                    
                    // Frames received
                    if let framesReceived = value.values["framesReceived"] as? Int32 {
                        self.framesReceivedGraph?.addDataPoint(value: Double(framesReceived))
                    }
                    
                    // framesAssembledFromMultiplePackets
                    if let multipacketFrames = value.values["framesAssembledFromMultiplePackets"] as? Int64 {
                        self.multipacketFramesGraph?.addDataPoint(value: Double(multipacketFrames))
                    }
                    
                    // Freeze Count
                    if let freezeCount = value.values["freezeCount"] as? Int64 {
                        self.freezeCountGraph?.addDataPoint(value: Double(freezeCount))
                    }
                    
                    // Pause Count
                    if let pauseCount = value.values["pauseCount"] as? Int64 {
                        self.pauseCountGraph?.addDataPoint(value: Double(pauseCount))
                    }
                    
                    // Nack Count
                    if let nackCount = value.values["nackCount"] as? Int64 {
                        self.nackCountGraph?.addDataPoint(value: Double(nackCount))
                    }
                    
                    // PLI count
                    if let pliCount = value.values["pliCount"] as? Int64 {
                        self.pliCountGraph?.addDataPoint(value: Double(pliCount))
                    }
                    
                    // Bitrate
                    if let bytesReceived = value.values["bytesReceived"] as? Int64 {
                        
                        if let lastBytes = self.lastBytesReceived {
                            self.lastBitrate = bytesReceived - lastBytes
                            let megabitsDelta : Double = Double(self.lastBitrate!) / 125000.0
                            self.bitrateGraph?.addDataPoint(value: megabitsDelta)
                        }
                        
                        self.lastBytesReceived = bytesReceived
                    }
                    
                    // Packets lost
                    if let packetsLost = value.values["packetsLost"] as? Int32 {
                        self.packetLossGraph?.addDataPoint(value: Double(packetsLost))
                    }
                    
                    // Keyframes decoded
                    if let keyframesDecoded = value.values["keyFramesDecoded"] as? Int64 {
                        self.keyframesGraph?.addDataPoint(value: Double(keyframesDecoded))
                    }
                    
                    // Frames decoded
                    if let framesDecoded = value.values["framesDecoded"] as? Int64 {
                        if let lastFrames = self.lastFramesDecoded {
                            let framesDelta : Int64 = framesDecoded - lastFrames;
                            if framesDelta > 0 {
                                self.framesDecodedGraph?.addDataPoint(value: Double(framesDelta))
                            }
                        }
                        self.lastFramesDecoded = framesDecoded
                    }
                    
                    // Frames dropped
                    if let framesDropped = value.values["framesDropped"] as? Int64 {
                        self.framesDroppedGraph?.addDataPoint(value: Double(framesDropped))
                    }
                    
                    // totalInterFrameDelay
                    if let totalInterFrameDelay = value.values["totalInterFrameDelay"] as? Double {
                        if let lastDelay = self.lastInterframeDelay {
                            let delayDelta : Double = totalInterFrameDelay - lastDelay
                            if delayDelta > 0 {
                                self.interframeDelayGraph?.addDataPoint(value: Double(delayDelta))
                            }
                        }
                        self.lastInterframeDelay = totalInterFrameDelay
                    }
                    
                    // totalProcessingDelay
                    if let totalProcessingDelay = value.values["totalProcessingDelay"] as? Double {
                        if let lastProcessing = self.lastProcessingDelay {
                            let processingDelta : Double = totalProcessingDelay - lastProcessing
                            if processingDelta > 0 {
                                self.processingTimeGraph?.addDataPoint(value: Double(processingDelta))
                            }
                        }
                        self.lastProcessingDelay = totalProcessingDelay
                    }

                    // There is also:
                    // firCount
                    // totalSquaredInterFrameDelay : Double
                    // totalDecodeTime : Double
                    // totalPausesDuration : Double
                    // packetsReceived : Int64
                    // totalFreezesDuration : Int64
                    // These were excluded because we can only fit so many graphs on the screen at once
                    // and these ones were either inferred from other graphs or less likely to be problematic (we hope)
                    
                }
            }
        }
    }
    
    func processARKitResponse(numResponses: UInt16){
        self.arkitEventsProcessedGraph?.addDataPoint(value: Double(numResponses))
    }
    
    func processARKitEvent(){
        let now = CFAbsoluteTimeGetCurrent()
        let prevTime = self.lastARKitEventTimestamp ?? CFAbsoluteTimeGetCurrent()
        let eventDelta = now - prevTime
        self.arkitEventsGraph?.addDataPoint(value: Double(eventDelta * 1000))
        self.lastARKitEventTimestamp = now
    }
    
}
