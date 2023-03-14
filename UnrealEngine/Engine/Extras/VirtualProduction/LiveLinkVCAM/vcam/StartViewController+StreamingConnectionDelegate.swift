//
//  StartViewController+StreamingConnectionDelegate.swift
//  vcam
//
//  Created by Brian Smith on 6/29/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

extension StartViewController : StreamingConnectionDelegate {
    
    func streamingConnectionDidConnect(_ connection: StreamingConnection) {
        Log.info("StreamingConnection \(connection.name) did connect to \(connection.destination)")
        
        DispatchQueue.main.async {
            self.hideConnectingAlertView {
                self.performSegue(withIdentifier: "showVideoView", sender: self)
           }
        }
    }

    func streamingConnection(_ connection: StreamingConnection, didDisconnectWithError err: Error?) {

        DispatchQueue.main.async {

            self.hideConnectingAlertView() {
                if let e = err {
                    Log.info("StreamingConnection \(connection.name) disconnected with error : \(e.localizedDescription)")

                    let errorAlert = UIAlertController(title: "Error", message: "Couldn't connect : \(e.localizedDescription)", preferredStyle: .alert)
                    errorAlert.addAction(UIAlertAction(title: "OK", style: .default, handler: { _ in
                        self.hideConnectingView() {}
                    }))
                    self.present(errorAlert, animated:true)
                    
                } else {
                    Log.info("StreamingConnection \(connection.name) disconnected.")
                }
            }
        }
    }
}
