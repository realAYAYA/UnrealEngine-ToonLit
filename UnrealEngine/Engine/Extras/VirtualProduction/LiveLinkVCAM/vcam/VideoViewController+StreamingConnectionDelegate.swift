//
//  VideoViewController+StreamingConnectionDelegate.swift
//  vcam
//
//  Created by Brian Smith on 6/28/22.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation
import UIKit

extension VideoViewController : StreamingConnectionDelegate {

    func streamingConnectionDidConnect(_ connection: StreamingConnection) {
        Log.info("StreamingConnection \(connection.name) did connect to \(connection.destination)")
        DispatchQueue.main.async { [weak self] in
            self?.showReconnecting(false, animated: true)
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, didDisconnectWithError err: Error?) {
        if let e = err {
            Log.info("StreamingConnection \(connection.name) disconnected: \(e.localizedDescription)")
        } else {
            Log.info("StreamingConnection \(connection.name) disconnected.")
        }

        // Got connection error, so exit back to streaming screen.
        DispatchQueue.main.async { [weak self] in
            self?.reconnect()
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, exitWithError err: Error?) {
        DispatchQueue.main.async { [weak self] in
            self?.hideConnectingAlertView() {
                let alert = UIAlertController(title: nil, message: err?.localizedDescription, preferredStyle: .alert)
                alert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default) { [weak self] _ in
                    self?.disconnect()
                })
                
                self?.present(alert, animated: true)
            }
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, requestsTextEditWithContents contents: String, handler: @escaping (Bool, String?) -> Void) {
        
        DispatchQueue.main.async { [weak self] in
            let alert = UIAlertController(title: nil, message: contents.count == 0 ?
                                          NSLocalizedString("message-entervalue", value: "Enter the value", comment: "Entering value in a textbox") :
                                          NSLocalizedString("message-updatevalue", value: "Update the value", comment: "Updating value in a textbox"),
                                          preferredStyle: .alert)
            alert.addTextField()
            alert.textFields![0].text = contents
            alert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default) {[weak alert] _ in
                handler(true, alert?.textFields![0].text!)
            })
            alert.addAction(UIAlertAction(title: Localized.buttonCancel(), style: .cancel) {_ in
                handler(false, nil)
            })

            self?.present(alert, animated: true)
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, requestStreamerSelectionWithStreamers streamers: Array<String>, handler: @escaping (String) -> Void) {
        self.pickerData = streamers;
        self.selectedStreamer = streamers[0];
        DispatchQueue.main.async { [weak self] in
            self?.hideConnectingAlertView() { [weak self] in
                let alert = UIAlertController(title: Localized.titleSelectStream(), message: "\n\n\n\n\n\n", preferredStyle: .alert)
                
                let picker = UIPickerView(frame: CGRect(x: 5, y: 20, width: 250, height: 140))
                picker.dataSource = self
                picker.delegate = self
                alert.view.addSubview(picker)

                alert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default) {[weak self] _ in
                    if let selectedStreamer = self?.selectedStreamer {
                        handler(selectedStreamer)
                    }
                })
                
                self?.present(alert, animated:true)
            }
        }
    }
    
    func streamingConnection(_ connection: StreamingConnection, receivedGamepadResponse: UInt8) {
        self.controllerResponseReceived(controllerIndex: receivedGamepadResponse)
    }
}

