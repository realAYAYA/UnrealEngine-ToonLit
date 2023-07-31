//
//  FileUtility.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/20/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import Foundation

class FileUtility {
    
    class func createFileAtURL(_ url: URL, overwrite: Bool) -> FileHandle? {
        
        let fm = FileManager.default
        
        if fm.fileExists(atPath: url.path) {
            
            if overwrite {
                do {
                    try fm.removeItem(at: url)
                } catch {
                    Log.error("Failed to remove file at '\(url.path)'")
                    return nil
                }
            } else {
                return nil
            }
        }

        fm.createFile(atPath: url.path, contents: nil, attributes: nil)
        return FileHandle(forWritingAtPath: url.path)

    }
    
    class func fileSystemSize()  -> Int64 {
        
        var bytes : Int64?
        
        let fileURL = URL(fileURLWithPath: NSHomeDirectory() as String)

        do {
            let values = try fileURL.resourceValues(forKeys: [.volumeTotalCapacityKey])
            bytes = (values.volumeTotalCapacity as NSNumber?)?.int64Value
        } catch {
            Log.error("Couldn't get the file system size.")
        }
        
        return bytes ?? 0
    }
    
    class func fileSystemFreeSize()  -> Int64 {
        
        var bytes : Int64?

        let fileURL = URL(fileURLWithPath: NSHomeDirectory() as String)

        do {
            let values = try fileURL.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
            bytes = (values.volumeAvailableCapacityForImportantUsage as NSNumber?)?.int64Value
        } catch {
            Log.error("Couldn't get the free file system size.")
        }
        
        return bytes ?? 0
    }
    
    class func appSize() -> Int64
    {
        // create list of directories
        var paths = [Bundle.main.bundlePath] // main bundle

        // documents directory
        let docDirDomain = FileManager.SearchPathDirectory.documentDirectory
        let docDirs = NSSearchPathForDirectoriesInDomains(docDirDomain, .userDomainMask, true)
        if let docDir = docDirs.first {
           paths.append(docDir)
        }
        
        // library directory
        let libDirDomain = FileManager.SearchPathDirectory.libraryDirectory
        let libDirs = NSSearchPathForDirectoriesInDomains(libDirDomain, .userDomainMask, true)
        if let libDir = libDirs.first {
           paths.append(libDir)
        }
        
        // temp directory
        paths.append(NSTemporaryDirectory() as String) // temp directory

        // get sizes of all directories
        var totalSize: Int64 = 0
        for path in paths {
            if let size = FileUtility.directorySize(directory: path) {
               totalSize += size
           }
        }
        return totalSize
    }
    
    class func directorySize(directory: String) -> Int64? {
        let fm = FileManager.default
        guard let subdirectories = try? fm.subpathsOfDirectory(atPath: directory) as NSArray else {
            return nil
        }
        let enumerator = subdirectories.objectEnumerator()
        var size: Int64 = 0
        while let fileName = enumerator.nextObject() as? String {
            do {
                let fileDictionary = try fm.attributesOfItem(atPath: directory.appending("/" + fileName)) as NSDictionary
                size += Int64(fileDictionary.fileSize())
            } catch let err {
                Log.error("err getting attributes of file \(fileName): \(err.localizedDescription)")
            }
        }
        return size
    }
}
