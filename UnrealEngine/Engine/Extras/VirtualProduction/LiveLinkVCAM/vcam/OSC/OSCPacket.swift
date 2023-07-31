//
//  OSCPacket.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 12/16/19.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation

enum OSCPacketType {
    case message
    case bundle
    case unknown
}

enum OSCArgument {
    case bool(Bool)
    case int32(Int32)
    case float32(Float)
    case string(String)
    case blob(Data)
    case unknown
    
    static func from(_ value : Any?) -> OSCArgument? {
        if value is String {
            return OSCArgument.string(value as! String)
        } else if value is Bool {
            return OSCArgument.bool(value as! Bool)
        } else if value is Int {
            return OSCArgument.int32(value as! Int32)
        } else if value is Float {
            return OSCArgument.float32(value as! Float)
        } else if value is Date {
            return OSCArgument.string(ISO8601DateFormatter().string(from: value as! Date))
        } else if value is Data {
            return OSCArgument.blob(value as! Data)
        } else {
            return nil
        }
    }
}

enum OSCError : Error {
    
    case malformedMessage
    case unsupportedType

}

class OSCPacket {
    
    var packetType = OSCPacketType.unknown

    func parseString(_ data : Data) -> String? {
     
        let subseq = data.prefix { (value : UInt8) -> Bool in
            return value != 0
        }
        
        if subseq.count > 0 {
            return String(decoding: subseq, as: UTF8.self)
        }

        return nil
    }

    class func padSize32(_ size : Int) -> Int {
        return (size + 3) & ~3
    }
    
    class func encode(_ str : String) -> Data {

        var data = Data()
        
        if let bytes = str.data(using: .utf8) {
            data.append(bytes)
            let pad = OSCPacket.padSize32(str.count + 1)
            let padlen = pad - str.count
            data.append(Data(count:padlen))
        }
        
        return data
    }

    class func encode(_ i : Int8) -> Data {

        withUnsafeBytes(of: i) { Data($0) }
    }

    class func encode(_ i : Int32, bigEndian : Bool = true) -> Data {

        withUnsafeBytes(of: bigEndian ? i.bigEndian : i) { Data($0) }
    }

    class func encode(_ f : Float, bigEndian : Bool = true) -> Data {

        withUnsafeBytes(of: bigEndian ? f.bitPattern.bigEndian : f.bitPattern) { Data($0) }
    }
    
    func toData(prependSize : Bool = false) throws ->Data {
        
        fatalError("must implement encoded() function in subclass")
        
    }
}

class OSCPacketMessage : OSCPacket {

    var addressPattern : String?
    var arguments : [OSCArgument]?

    init(_ data : Data) throws {
        
        super.init()
        packetType = .message
        
        // parse the address pattern
        if let ap = parseString(data) {

            addressPattern = ap
            
            var offset = OSCPacket.padSize32(ap.count + 1)
            
            if offset > data.count {
                throw OSCError.malformedMessage
            }
            
            // parse the type tag
            if let typeTag = parseString(data.suffix(from:data.startIndex + offset)) {

                //Log.info("\(addressPattern ?? "???") \(typeTag)")
                
                // validate that the typetag is formed correctly
                if typeTag.starts(with: ",") {

                    offset += OSCPacket.padSize32(typeTag.count + 1)
                    
                    arguments = [OSCArgument]()
                    
                    // loop over each entry in the type tag
                    try typeTag.suffix(from:typeTag.index(typeTag.startIndex, offsetBy: 1)).forEach { (c:Character) in
                        
                        let thisTypeData = data.suffix(from:data.startIndex + offset)
                        
                        // int32
                        if c == "i" {
                            let value = thisTypeData.subdata(in: thisTypeData.startIndex..<thisTypeData.startIndex+4).withUnsafeBytes { $0.load(as: Int32.self) }
                            arguments?.append(OSCArgument.int32(value.byteSwapped))
                            offset += 4
                        }

                        // float32
                        else if c == "f" {
                            let value = Float(bitPattern: UInt32(bigEndian: thisTypeData.withUnsafeBytes { $0.load(as: UInt32.self) }))
                            arguments?.append(OSCArgument.float32(value))
                            offset += 4
                        }

                        // string
                        else if c == "s" {
                            if let arg = parseString(thisTypeData) {
                                arguments?.append(OSCArgument.string(arg))
                                offset += OSCPacket.padSize32(arg.count + 1)
                            }
                        }

                        // blob
                        else if c == "b" {
                            // due to a bug in UE, the blob size is little endian (see encoding also, below)
                            let sz = Int(thisTypeData.subdata(in: thisTypeData.startIndex..<thisTypeData.startIndex+4).withUnsafeBytes { $0.load(as: Int32.self) })
                            arguments?.append(OSCArgument.blob(thisTypeData.subdata(in: (thisTypeData.startIndex + 4)..<(thisTypeData.startIndex + 4 + sz))))
                            offset += 4 + OSCPacket.padSize32(sz)
                        }

                        // unknown/unsupported
                        else {
                            throw OSCError.unsupportedType
                        }
                    }
                    
                    
                } else {
                    throw OSCError.malformedMessage
                }
            }
        } else {
            throw OSCError.malformedMessage
        }
    }
    
    init(_ address : String, arguments args : [OSCArgument]?) {
        super.init()
        
        addressPattern = address
        arguments = args
    }

    convenience init(_ address : OSCAddressPattern, arguments args : [OSCArgument]?) {
        
        self.init(address.rawValue, arguments: args)
    }

    override func toData(prependSize : Bool = false) throws -> Data {

        var data = Data(count: prependSize ? MemoryLayout<UInt32>.size : 0)
        
        if let ap = addressPattern {
            data.append(OSCPacket.encode(ap))
        }

        var typeTag = String(",")
        var argumentsData = Data()
        
        if let args = arguments {
            for a in args {
                switch (a) {
                case .bool(let v):
                    typeTag.append(v ? "t" : "f")
                case .int32(let v):
                    typeTag.append("i")
                    argumentsData.append(OSCPacket.encode(v))
                case .float32(let v):
                    typeTag.append("f")
                    argumentsData.append(OSCPacket.encode(v))
                case .string(let v):
                    typeTag.append("s")
                    argumentsData.append(OSCPacket.encode(v))
                case .blob(let v):
                    typeTag.append("b")
                    // due to a bug in UE, the blob size is little endian
                    argumentsData.append(OSCPacket.encode(Int32(v.count), bigEndian: false))
                    argumentsData.append(v)
                case .unknown:
                    throw OSCError.unsupportedType
                }
            }
        }
        
        data.append(OSCPacket.encode(typeTag))
        data.append(argumentsData)
        
        if prependSize {
            let bufferSize = UInt32(data.count - MemoryLayout<UInt32>.size)
            let bufferSizeBytes = withUnsafeBytes(of: bufferSize) { Data($0) }
            data.replaceSubrange(0..<MemoryLayout<UInt32>.size, with: bufferSizeBytes)
        }

        return data
    }
    
    func debugString() -> String {
        
        var s = String(addressPattern ?? "<no addressPattern>")

        if let args = arguments {
            for a in args {
                s.append(" ")
                switch (a) {
                case .bool(let v):
                    s.append("bool(\(v ? "true" : "false"))")
                case .int32(let v):
                    s.append("int32(\(v))")
                case .float32(let v):
                    s.append("float32(\(v))")
                case .string(let v):
                    s.append("string(\(v))")
                case .blob(let v):
                    s.append("blob(\(v.count) bytes)")
                case .unknown:
                    s.append("unknown()")
                }
            }
        }
        
        return s
    }

}
