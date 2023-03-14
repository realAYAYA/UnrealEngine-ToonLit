//
//  VideoViewController+StreamingConnectionDelegate.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation

extension VideoViewController : StreamingConnectionDelegate {

    func streamingConnectionDidConnect(_ connection: StreamingConnection) {
        Log.info("StreamingConnection \(connection.name) did connect to \(connection.destination)")
        DispatchQueue.main.async {
            self.showReconnecting(false, animated: true)
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, didDisconnectWithError err: Error?) {
        if let e = err {
            Log.info("StreamingConnection \(connection.name) disconnected: \(e.localizedDescription)")
        } else {
            Log.info("StreamingConnection \(connection.name) disconnected.")
        }

        DispatchQueue.main.async {

            if self.dismissOnDisconnect {
                self.streamingConnection?.delegate = nil
                self.streamingConnection = nil
                self.presentingViewController?.dismiss(animated: true, completion: nil)
            } else {
                self.showReconnecting(true, animated: true)
            }
        }

    }
}

