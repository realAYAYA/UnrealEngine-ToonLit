//
//  AboutViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 2/21/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit

class AboutViewController : UIViewController {

    @IBOutlet weak var appname : UILabel!
    @IBOutlet weak var version : UILabel!
    @IBOutlet weak var copyright : UILabel!
    @IBOutlet weak var legal : UILabel!

    override func viewDidLoad() {
        super.viewDidLoad()

        if UIDevice.current.userInterfaceIdiom == .phone {
            self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
        }

        self.appname.text = "Live Link VCAM"
        
        if let infoDict = Bundle.main.infoDictionary {
            self.version.text = "Version \(infoDict["CFBundleShortVersionString"] as! String) (\(infoDict["CFBundleVersion"] as! String))"
            self.copyright.text = infoDict["NSHumanReadableCopyright"] as? String
        }
        
        self.legal.text = ""

        if let eulaPath = Bundle.main.path(forResource: "eula", ofType: "html") {
            do {
                let contents = try String(contentsOfFile: eulaPath)
                
                let attrString = try NSMutableAttributedString(
                    data: contents.data(using: String.Encoding.unicode)!,
                    options: [NSAttributedString.DocumentReadingOptionKey.documentType: NSAttributedString.DocumentType.html],
                    documentAttributes: nil)
                
                attrString.addAttributes(
                    [ NSAttributedString.Key.foregroundColor : self.copyright.textColor!//,
                        //NSAttributedString.Key.font : self.copyright.font!
                    ],
                    range: NSRange(location: 0, length: attrString.length))
                
                self.legal.attributedText = attrString
            } catch {
            }
        }
    }

    @objc func done(sender:Any?) {
        self.navigationController?.dismiss(animated: true, completion: nil)
    }

}
