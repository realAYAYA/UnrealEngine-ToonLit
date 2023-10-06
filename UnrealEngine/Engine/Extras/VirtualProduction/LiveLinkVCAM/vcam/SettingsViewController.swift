//
//  SettingsViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 12/11/19.
//  Copyright Epic Games, Inc. All Rights Reserved.
//


import UIKit

class SettingsViewController : UITableViewController {
    
    let appSettings = AppSettings.shared
    
    class LocalizedConnectionType {
        
        class func title() -> String {
            return NSLocalizedString("connectiontype-title", value: "ConnectionType", comment: "")
        }
        class func footer() -> String {
            return NSLocalizedString("connectiontype-footer", value: "Unreal Engine Pixel Streaming introduced editor support in Unreal Engine 5.1 and is the recommended connection type for Live Link VCAM going forward.  The legacy Remote Session connection type will continue to be supported in Live Link VCAM for older projects.", comment: "")
        }
        class func value(_ type : StreamingConnectionType) -> String {
            switch type {
            case .remoteSession:
                return NSLocalizedString("connectiontype-value-remotesession", value: "Remote Session", comment: "")
            case .webRTC:
                return NSLocalizedString("connectiontype-value-pixelstreaming", value: "Pixel Streaming", comment: "")
            }
        }
    }
                    
    override func viewDidLoad() {
        super.viewDidLoad()
        if UIDevice.current.userInterfaceIdiom == .phone {
            self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
        }
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        self.tableView.reloadData()
    }

    override func tableView(_ tableView: UITableView, willDisplay cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        
        if let detail = cell.detailTextLabel {
            detail.textColor = UIColor.secondaryLabel
        }
        
        switch cell.reuseIdentifier {

        case "subjectName":
            cell.detailTextLabel?.text = appSettings.liveLinkSubjectName

        case "timecode":
            cell.detailTextLabel?.text = Timecode.sourceToString(appSettings.timecodeSourceEnum())
            
        case "connectionType":
            cell.detailTextLabel?.text = LocalizedConnectionType.value(appSettings.connectionTypeEnum())

        default:
            break
        }
    }
    
    @objc func done(sender:Any?) {
        self.navigationController?.dismiss(animated: true, completion: nil)
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {

        if let vc = segue.destination as? SingleValueViewController {
            
            if segue.identifier == "subjectName" {
                
                vc.navigationItem.title = Localized.subjectName()
                vc.mode = .edit
                vc.allowedType = .unreal
                vc.initialValue = AppSettings.shared.liveLinkSubjectName
                vc.placeholderValue = AppSettings.defaultLiveLinkSubjectName()
                vc.finished = { (action, value) in
                    
                    if action == .done {
                        
                        let v = value!.trimmed()
                        
                        AppSettings.shared.liveLinkSubjectName = v.isEmpty ? AppSettings.defaultLiveLinkSubjectName() : value!.toUnrealCompatibleString()
                        self.tableView.reloadData()
                    }
                }
            }
        }
        
        else if let vc = segue.destination as? MultipleChoiceViewController {
            
            if segue.identifier == "connectionType" {
                
                vc.navigationItem.title = LocalizedConnectionType.title()
                vc.items = [ LocalizedConnectionType.value(.remoteSession), LocalizedConnectionType.value(.webRTC) ]
                vc.selectedIndex = appSettings.connectionTypeEnum() == .remoteSession ? 0 : 1
                vc.footerString = LocalizedConnectionType.footer()
                vc.completion = { (index) in
                    self.appSettings.setConnectionTypeEnum(index == 0 ? .remoteSession : .webRTC)
                }
            }
        }

    }
    
    
}
