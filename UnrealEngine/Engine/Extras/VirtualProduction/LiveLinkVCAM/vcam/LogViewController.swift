//
//  LogViewController.swift
//  VCAM
//
//  Created by Brian Smith on 1/9/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit

class LogItemCell : UITableViewCell {
    
    @IBOutlet weak var typeLabel : UILabel!
    @IBOutlet weak var dateLabel : UILabel!
    @IBOutlet weak var messageLabel : UILabel!

}

class LogViewController : UITableViewController {

    var shareButton : UIBarButtonItem!

    var dateFormatter : DateFormatter!
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)

        self.navigationItem.title = NSLocalizedString("log-title", value: "Log", comment: "Title of the application log screen")
        
        Log.delegate = self
        
        dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "hh:mm:ss.SSS"
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        self.shareButton = UIBarButtonItem(barButtonSystemItem: .action, target: self, action: #selector(share))

        if UIDevice.current.userInterfaceIdiom == .phone {
            let doneButton = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
            self.navigationItem.rightBarButtonItems = [ doneButton, shareButton ]
        } else {
            self.navigationItem.rightBarButtonItem = shareButton
        }
    }
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        return 1
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        
        return Log.items.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        
        let cell = tableView.dequeueReusableCell(withIdentifier: "logItemCell") as! LogItemCell
        
        let item = Log.items[indexPath.row]

        switch item.type {
        case .debug:
            cell.typeLabel.textColor = UIColor.systemGray
        case .info:
            cell.typeLabel.textColor = UIColor.systemGreen
        case .warning:
            cell.typeLabel.textColor = UIColor.systemYellow
        case .error:
            cell.typeLabel.textColor = UIColor.systemRed
        default:
            break
        }
        
        cell.dateLabel.text = dateFormatter.string(from: item.timestamp)
        cell.messageLabel.text = item.message

        return cell
    }
    
    @objc func share(sender:Any?) {
        
        let avc = UIActivityViewController(activityItems: [ LogActivityItemSource(nil) ], applicationActivities: nil)
        avc.popoverPresentationController?.barButtonItem = shareButton
        
        self.present(avc, animated: true, completion: nil)
    }
    
    @objc func done(sender:Any?) {
        
        self.navigationController?.dismiss(animated: true, completion: nil)
    }
}

extension LogViewController : LogDelegate {
    
    func logDidAddRow(_ rowNumber: Int) {
        self.tableView.insertRows(at: [ IndexPath(row:rowNumber, section: 0) ], with: .automatic)
    }
    
}

class LogActivityItemSource : NSObject, UIActivityItemSource {
    
    var url : URL?
    
    init(_ path: String?) {
        super.init()

        // a path was specified (existing log file) use that URL to avoid
        // creating a new file later on
        if let p = path {
            self.url = URL(fileURLWithPath: p)
        }
    }
    
    func activityViewControllerPlaceholderItem(_ activityViewController: UIActivityViewController) -> Any {
        
        if let url = self.url {

            return url

        } else {
            var preview = String()
            let logItems = Log.items;
            
            var count = 0
            
            for item in logItems {
                preview.append(item.toCSV())
                
                count += 1
                if count == 5 {
                    break
                }
            }

            return preview
        }

    }
    
    func activityViewController(_ activityViewController: UIActivityViewController, itemForActivityType activityType: UIActivity.ActivityType?) -> Any? {

        // if we already have a URL, just return it here (either it points to an existing log
        // file OR we have alreadyed created one.
        if let url = self.url {
            return url
        } else {

            let url = FileManager.default.temporaryDirectory.appendingPathComponent("LiveLinkVCAM.log")
            if let fileHandle = FileUtility.createFileAtURL(url, overwrite: true) {
                let logItems = Log.items;
                for item in logItems {

                    if let data = item.toCSV().data(using: .utf8) {
                        fileHandle.write(data)
                    }
                }
                 
                fileHandle.closeFile()

                self.url = url
                return self.url
            }

            return nil
        }
    }
    
    func activityViewController(_ activityViewController: UIActivityViewController, subjectForActivityType activityType: UIActivity.ActivityType?) -> String {
        return "Live Link VCAM Log"
    }
    
    func activityViewController(_ activityViewController: UIActivityViewController, dataTypeIdentifierForActivityType activityType: UIActivity.ActivityType?) -> String {
        return "text/plain"
    }
    
}
