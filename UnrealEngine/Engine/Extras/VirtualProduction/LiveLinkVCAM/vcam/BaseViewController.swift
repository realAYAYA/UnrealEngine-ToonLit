//
//  BaseViewController.swift
//  vcam
//
//  Created by Brian Smith on 6/2/21.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

class BaseViewController : UIViewController {
    
    private var connectingAlertView : UIAlertController?
    
    enum ConnectingAlertMode {
        case connecting
        case reconnecting
    }

    func showConnectingAlertView(mode : ConnectingAlertMode,  _ cancelled : @escaping () -> Void) {
        connectingAlertView = UIAlertController(title: mode == .connecting ? NSLocalizedString("Connecting...", comment: "Status message that the app is connecting.") : NSLocalizedString("Reconnecting...", comment: "Status message that the app is reconnecting."), message: nil, preferredStyle: .alert)
        connectingAlertView!.addActivityIndicator()
        connectingAlertView!.addAction(UIAlertAction(title: mode == .connecting ? Localized.buttonCancel() : NSLocalizedString("Disconnect", comment: "Button to disconnect from a UE instance"), style: .destructive, handler: { _ in
            cancelled()
        }))

        present(connectingAlertView!, animated: true, completion: nil)
    }
    
    func hideConnectingAlertView( _ completion : @escaping () -> Void) {
        connectingAlertView?.dismiss(animated: true, completion: completion)
        connectingAlertView = nil
    }

}
