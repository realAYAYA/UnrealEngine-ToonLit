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
            switch appSettings.connectionType {
            case "RemoteSession":
                cell.detailTextLabel?.text = "Remote Session"
            case "WebRTC":
                cell.detailTextLabel?.text = "Pixel Streaming"
            default:
                cell.detailTextLabel?.text = "Unknown"
            }

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
                
                vc.navigationItem.title = "Connection Type"
                vc.items = [ "Remote Session", "Pixel Streaming" ]
                vc.selectedIndex = appSettings.connectionType == "RemoteSession" ? 0 : 1
                vc.footerString = "Description of Connection Types"
                vc.completion = { (index) in
                    self.appSettings.connectionType = index == 0 ? "RemoteSession" : "WebRTC"
                }
            }
        }

    }
    
    
}
