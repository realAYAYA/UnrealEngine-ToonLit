//
//  IntToBytes.swift
//  WebRTC-Demo
//
//  Created by Luke Bermingham on 20/6/2022.
//  Very similar to this answer on StackOverflow: https://stackoverflow.com/a/41368211
//

import Foundation

// Add the `toBytes` method to UInt16, UInt32, and UInt64
protocol UIntToBytesConvertable {
    func toBytes() -> [UInt8]
}

extension UIntToBytesConvertable {
    func toByteArr<T: UnsignedInteger>(endian: T, count: Int) -> [UInt8] {
        var _endian = endian
        let bytePtr = withUnsafePointer(to: &_endian) {
            $0.withMemoryRebound(to: UInt8.self, capacity: count) {
                UnsafeBufferPointer(start: $0, count: count)
            }
        }
        return [UInt8](bytePtr)
    }
}

extension UInt16: UIntToBytesConvertable {
    func toBytes() -> [UInt8] {
        return toByteArr(endian: self.littleEndian, count: MemoryLayout<UInt16>.size)
    }
}

extension UInt32: UIntToBytesConvertable {
    func toBytes() -> [UInt8] {
        return toByteArr(endian: self.littleEndian, count: MemoryLayout<UInt32>.size)
    }
}

extension UInt64: UIntToBytesConvertable {
    func toBytes() -> [UInt8] {
        return toByteArr(endian: self.littleEndian, count: MemoryLayout<UInt64>.size)
    }
}
