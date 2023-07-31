//
//  ConnectButton.swift
//  vcam
//
//  Created by Brian Smith on 12/4/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

class ConnectButton : UIButton {
  
    override var isEnabled: Bool {
        didSet {
            let c = isEnabled ? self.titleColor(for: .normal) : self.titleColor(for: .disabled)
            self.layer.borderColor = c?.cgColor
        }
    }
    
    required init?(coder: NSCoder) {

        super.init(coder: coder)

        self.setTitleColor(UIColor.lightGray, for: .disabled)

        self.layer.cornerRadius = 5
        self.layer.borderWidth = 1
        self.layer.borderColor = UIColor.white.cgColor
        self.semanticContentAttribute = .forceRightToLeft
        self.titleLabel?.textAlignment = .left
    }
    
    override func titleRect(forContentRect contentRect: CGRect) -> CGRect {
        var titleRect = super.titleRect(forContentRect: contentRect)
        titleRect.origin.x = contentRect.origin.x + 6
        return titleRect
    }

}
