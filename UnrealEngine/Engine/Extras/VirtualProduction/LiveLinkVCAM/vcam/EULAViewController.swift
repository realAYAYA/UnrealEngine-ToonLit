//
//  EULAViewController.swift
//  FaceLink
//
//  Created by Brian Smith on 4/28/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

class EULAViewController : UIViewController {

    @IBOutlet weak var legal : UITextView!

    override var preferredStatusBarStyle: UIStatusBarStyle {
          return .darkContent
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        
    
        self.legal.text = ""

        if let eulaPath = Bundle.main.path(forResource: "eula", ofType: "html") {
            do {
                let contents = try String(contentsOfFile: eulaPath)
                
                let attrString = try NSMutableAttributedString(
                    data: contents.data(using: String.Encoding.unicode)!,
                    options: [NSAttributedString.DocumentReadingOptionKey.documentType: NSAttributedString.DocumentType.html],
                    documentAttributes: nil)
                
                attrString.addAttributes(
                    [ NSAttributedString.Key.foregroundColor : self.legal.textColor!//,
                        //NSAttributedString.Key.font : self.copyright.font!
                    ],
                    range: NSRange(location: 0, length: attrString.length))
                
                self.legal.attributedText = attrString
            } catch {
            }
        }
    }
    
    @IBAction func accept(_ sender : Any?) {
        
        self.performSegue(withIdentifier: "acceptEULA", sender: nil)
        AppSettings.shared.didAcceptEULA = true;

    }
    
}

extension EULAViewController : UITextViewDelegate {
    
    func textView(_ textView: UITextView, shouldInteractWith URL: URL, in characterRange: NSRange, interaction: UITextItemInteraction) -> Bool {
        return true;
    }
    
}
