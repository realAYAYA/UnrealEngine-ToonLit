//
//  JPEGVideoDecoder.swift
//  vcam
//
//  Created by Brian Smith on 8/8/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import VideoToolbox
import MetalKit

class CMBlockBufferHelper {

    static var sharedInstance = {
        return CMBlockBufferHelper()
    }()
    
    var holder: [NSData] = []
    
    func retain(_ data: NSData) {
        holder.append(data)
    }
    
    func release(pointer:UnsafeRawPointer) {
        if let index = holder.firstIndex(where: { $0.bytes == pointer }) {
            holder.remove(at: index)
        }
    }
    
    func blockSource(with bytes: UnsafeMutableRawPointer) -> CMBlockBufferCustomBlockSource {
        return CMBlockBufferCustomBlockSource(
            version: kCMBlockBufferCustomBlockSourceVersion,
            AllocateBlock: { (refCon, size) -> UnsafeMutableRawPointer? in
                //print("AllocateBlock:\(refCon)")
                return refCon
            }, FreeBlock: { (refCon, memoryBlock, size) in
                //print("FreeBlock:\(memoryBlock)")
                CMBlockBufferHelper.sharedInstance.release(pointer:memoryBlock)
            }, refCon: bytes)
    }
    
    func create(with data:NSData) -> CMBlockBuffer? {
        retain(data)
        
        var blockSource = self.blockSource(with: UnsafeMutableRawPointer(mutating: data.bytes))
        
        var bb: CMBlockBuffer? = nil
        let status = CMBlockBufferCreateWithMemoryBlock(allocator: nil, memoryBlock: nil, blockLength: data.length, blockAllocator: nil, customBlockSource: &blockSource, offsetToData: 0, dataLength: data.length, flags: 0, blockBufferOut: &bb)
        if status != noErr {
            //print("CMBlockBufferCreateWithMemoryBlock(\(data) failed:\(status)")
            release(pointer: data.bytes)
        }
        return bb
    }
}

class JPEGVideoDecoder {

    var width : Int32 = 0
    var height : Int32 = 0
    var formatDescription : CMVideoFormatDescription?
    var decompressor : VTDecompressionSession?
    
    let queue: OperationQueue = {
        let queue = OperationQueue()
        queue.maxConcurrentOperationCount = 1
        return queue
    }()
    
    var times = Array<Date>()
    var frame = 0
    
    deinit {
        Log.info("VTDecompressionSession : Tearing down.")
        if let dc = decompressor {
            VTDecompressionSessionInvalidate( dc )
            self.decompressor = nil
        }
    }
    
    private func tick() {
        times.append(Date())
        
        while times.count > 30 {
            times.remove(at: 0)
        }

        if times.count == 30 {
            let elapsed = -(times.first?.timeIntervalSinceNow ?? -1.0)
            Log.info("\(frame) : \(29.0 / elapsed)Hz")
        }
        
        frame += 1
    }
    
    func decode(width : Int32, height : Int32, data: Data, _ completion: @escaping (_ pixelBuffer : CVPixelBuffer?) -> Void) {
        
        queue.operations.filter { $0.isReady && !$0.isFinished && !$0.isExecuting && !$0.isCancelled }
            .forEach {
                //Log.info("cancelling op")
                $0.cancel()
            }

        queue.addOperation {
            
            var status : OSStatus = 0
            
            // create a new decompression session if we don't have a decompressor OR the size has changed.
            if self.decompressor == nil || (CMVideoFormatDescriptionGetDimensions(self.formatDescription!).width != width) || (CMVideoFormatDescriptionGetDimensions(self.formatDescription!).height != height) {

                // tear down the previous decompressor if it exists
                if self.decompressor != nil {
                    Log.info("VTDecompressionSession : Tearing down.")
                    VTDecompressionSessionInvalidate( self.decompressor!)
                    self.decompressor = nil
                }
                
                // create a format description : we are decoding JPEGs
                status = CMVideoFormatDescriptionCreate(allocator: nil, codecType: kCMVideoCodecType_JPEG, width: width, height: height, extensions: nil, formatDescriptionOut: &self.formatDescription)
                if status == 0 {
                    
                    let attrs = [kCVPixelBufferMetalCompatibilityKey : true,
                                 kCVPixelBufferPixelFormatTypeKey : kCVPixelFormatType_32ARGB,
                                 kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey : true
                        ] as [CFString : Any]

                    status = VTDecompressionSessionCreate(allocator: nil, formatDescription: self.formatDescription!, decoderSpecification: nil, imageBufferAttributes: attrs as CFDictionary, outputCallback : nil, decompressionSessionOut: &self.decompressor)
                    
                    if status == 0 {
                        Log.info("VTDecompressionSessionCreate : OK")
                    } else {
                        Log.error("VTDecompressionSessionCreate returned \(status)")
                        completion(nil)
                    }
                } else {
                    Log.error("CMVideoFormatDescriptionCreate returned \(status)")
                    completion(nil)
                }
            }
            
            // create a block buffer and fill it with the encoded JPEG data
            let blockBuffer = CMBlockBufferHelper.sharedInstance.create(with: data as NSData)
            
            CMBlockBufferAssureBlockMemory(blockBuffer!)
            
            var sampleBuffer : CMSampleBuffer?
            
            status = CMSampleBufferCreate(allocator: nil, dataBuffer: blockBuffer, dataReady: true, makeDataReadyCallback: nil, refcon: nil, formatDescription: self.formatDescription!, sampleCount: 1, sampleTimingEntryCount: 0, sampleTimingArray: nil, sampleSizeEntryCount: 0, sampleSizeArray: nil, sampleBufferOut: &sampleBuffer)
            if status != 0 {
                Log.error("CMSampleBufferCreate returned \(status)")
                completion(nil)
            }

            // decompress
            status = VTDecompressionSessionDecodeFrame(self.decompressor!, sampleBuffer: sampleBuffer!, flags: VTDecodeFrameFlags(rawValue: 0), infoFlagsOut: nil, outputHandler: { status, flags, imageBuffer, time0, time1  in
                completion(imageBuffer as CVPixelBuffer?)
            })
            
            if status != 0 {
                Log.error("VTDecompressionSessionDecodeFrame returned \(status)")
                completion(nil)
            }
        }

            
    }
    
}
