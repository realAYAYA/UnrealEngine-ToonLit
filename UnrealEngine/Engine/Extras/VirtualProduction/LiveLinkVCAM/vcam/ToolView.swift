//
//  ToolView.swift
//  vcam
//
//  Created by Brian Smith on 12/4/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

class ToolView : UIView {
    
    required init?(coder: NSCoder) {

        super.init(coder: coder)

        self.clipsToBounds = true
        self.layer.cornerRadius = 5
        self.layer.borderWidth = 1
        self.layer.borderColor = UIColor.white.cgColor
        self.layer.shadowRadius = 0.5
        self.layer.shadowColor = UIColor.black.cgColor
    }
    
}
