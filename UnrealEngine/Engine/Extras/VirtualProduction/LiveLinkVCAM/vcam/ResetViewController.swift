//
//  ResetViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/20/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit

class ResetViewController : UITableViewController {
    
    override func viewDidLoad() {
        super.viewDidLoad()
        if UIDevice.current.userInterfaceIdiom == .phone {
            self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
        }
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        
        if indexPath.section == 0 && indexPath.row == 0 {
            
            let alert = UIAlertController(
                title: NSLocalizedString("reset-title", value: "Reset settings?", comment: "Alert title to confirm reset settings."),
                message: NSLocalizedString("reset-message", value: "Restore all Live Link VCAM settings to their defaults?", comment: "Alert title to confirm reset settings."),
                preferredStyle: .alert)
            
            alert.addAction(UIAlertAction(title: NSLocalizedString("reset-confirm-title", value: "Reset", comment: "Alert title to confirm reset settings."), style: .destructive, handler: { _ in
                AppSettings.reset()
                let alert = UIAlertController(title: NSLocalizedString("reset-success-title", value: "Reset Successful", comment: "Alert title that reset was successful."),
                                              message: NSLocalizedString("reset-success-message", value: "Live Link VCAM settings were reset to their defaults.", comment: "Alert message that reset was successful."),
                                              preferredStyle: .alert)
                alert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default))
                self.present(alert, animated: true)
            }))
            alert.addAction(UIAlertAction(title: Localized.buttonCancel(), style: .cancel))

            self.present(alert, animated: true)
        }
        
        tableView.deselectRow(at: indexPath, animated: true)
        
    }
    
    @objc func done(sender:Any?) {
        self.navigationController?.dismiss(animated: true, completion: nil)
    }
    
}
