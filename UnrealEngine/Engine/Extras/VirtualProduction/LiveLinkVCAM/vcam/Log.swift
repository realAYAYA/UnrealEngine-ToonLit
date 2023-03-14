//
//  Log.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/9/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit

enum LogItemType : String {
    case debug
    case info
    case warning
    case error
}

class LogItem {
    
    var type : LogItemType!
    var timestamp : Date!
    var message : String!
    
    init(_ type : LogItemType, _ message : String) {
        self.type = type
        self.timestamp = Date()
        self.message = message
    }
    
    func toCSV() -> String {
        return "\(Log.dateFormatter.string(from: self.timestamp)),\(self.type.rawValue),\(self.message ?? "")\n"
    }
}

protocol LogDelegate {
    func logDidAddRow(_ rowNumber : Int)
}

class Log : NSObject {
    
    static var items = [LogItem]()
    static var dateFormatter : DateFormatter!
    
    static var delegate : LogDelegate?
    
    class func createDateFormatter() -> DateFormatter {
        let df = DateFormatter()
        df.dateFormat = "yyyy-MM-dd hh:mm:ss.SSS"

        return df
    }
    
    class func append(_ type : LogItemType, _ message : String) {

        NSLog(message);

        if dateFormatter == nil {
            dateFormatter = Log.createDateFormatter()
        }
        
        let item = LogItem(type, message)

        DispatchQueue.main.async {
            
            items.append(item)

            if let d = Log.delegate {
                d.logDidAddRow(items.count - 1)
            }
        }
    }
    
    class func debug(_ message : String) {
        append(.debug, message)
    }
    class func info(_ message : String) {
        append(.info, message)
    }
    class func warning(_ message : String) {
        append(.warning, message)
    }
    class func error(_ message : String) {
        append(.error, message)
    }
}
