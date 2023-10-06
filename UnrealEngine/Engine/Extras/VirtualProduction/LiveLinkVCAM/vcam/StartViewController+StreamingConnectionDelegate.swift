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
            if self.connectingView.isHidden == true {
                self.performSegue(withIdentifier: "showVideoView", sender: self)
            } else {
                self.hideConnectingView {
                    self.performSegue(withIdentifier: "showVideoView", sender: self)
                }
            }
            
        }
    }

    func streamingConnection(_ connection: StreamingConnection, didDisconnectWithError err: Error?) {

        DispatchQueue.main.async {
            connection.disconnect()
            self.hideConnectingAlertView {
                if let e = err {
                    Log.info("StreamingConnection \(connection.name) disconnected with error : \(e.localizedDescription)")

                    let errorAlert = UIAlertController(title: Localized.titleError(), message: "\(Localized.messageCouldntConnect()) : \(e.localizedDescription)", preferredStyle: .alert)
                    errorAlert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default, handler: { _ in
                        self.hideConnectingView() {}
                    }))
                    self.present(errorAlert, animated:true)
                    
                } else {
                    Log.info("StreamingConnection \(connection.name) disconnected.")
                }
            }
        }
    }
    func streamingConnection(_ connection: StreamingConnection, exitWithError err: Error?) {

        DispatchQueue.main.async {
            connection.disconnect()
            self.hideConnectingAlertView {
                if let e = err {
                    Log.info("StreamingConnection \(connection.name) disconnected with error : \(e.localizedDescription)")

                    let errorAlert = UIAlertController(title: Localized.titleError(), message: "\(Localized.messageCouldntConnect()) : \(e.localizedDescription)", preferredStyle: .alert)
                    errorAlert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default, handler: { _ in
                        self.hideConnectingView() {}
                    }))
                    self.present(errorAlert, animated:true)
                    
                } else {
                    Log.info("StreamingConnection \(connection.name) disconnected.")
                }
            }
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, requestsTextEditWithContents contents: String, handler: @escaping (Bool, String?) -> Void) {
    }
    
    func streamingConnection(_ connection: StreamingConnection, requestStreamerSelectionWithStreamers streamers: Array<String>, handler: @escaping (String) -> Void) {
        self.pickerData = streamers;
        self.selectedStreamer = streamers[0];
        DispatchQueue.main.async {
            self.hideConnectingView() {
                let alert = UIAlertController(title: Localized.titleSelectStream(), message: "\n\n\n\n\n\n", preferredStyle: .alert)
                
                let picker = UIPickerView(frame: CGRect(x: 5, y: 20, width: 250, height: 140))
                picker.dataSource = self;
                picker.delegate = self;
                alert.view.addSubview(picker)

                alert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default) {_ in
                    handler(self.selectedStreamer)
                })
                
                self.present(alert, animated:true)
            }
        }
    }
    func streamingConnection(_ connection: StreamingConnection, receivedGamepadResponse: UInt8) {
    }
}
