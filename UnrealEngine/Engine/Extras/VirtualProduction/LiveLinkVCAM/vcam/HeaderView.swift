//
//  HeaderView.swift
//  vcam
//
//  Created by Brian Smith on 4/17/21.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import QuickLayout

@objc protocol HeaderViewDelegate {
    func headerViewLogButtonTapped(_ headerView : HeaderView)
    func headerViewExitButtonTapped(_ headerView : HeaderView)
}

class HeaderView : UIView {
    
    @objc dynamic let appSettings = AppSettings.shared
    private var observers = [NSKeyValueObservation]()

    private var displayLink: CADisplayLink!
    
    private var subjectLabel : UILabel!
    private var timecodeImageView: UIImageView!
    private var timecodeLabel : UILabel!
    private var streamingStatsLabel: UILabel!
    
    @IBOutlet weak var delegate : HeaderViewDelegate?
    @IBInspectable var showButtons : Bool = true
    
    var subject : String = "" {
        didSet {
            self.subjectLabel.text = " • " + subject
        }
    }
    var stats : String?  {
        didSet {
            if let str = stats {
                self.streamingStatsLabel.text = " • " + str
            } else {
                self.streamingStatsLabel.text = nil
            }
        }
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        privateInit()
    }
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        privateInit()
    }
    
    private func privateInit() {

    }
    
    override func awakeFromNib() {
        super.awakeFromNib()
        
        let labelFont = UIFont(name: "Menlo", size: 13)
        
        let v = UIView()
        addSubview(v)
        v.layoutToSuperview(.centerX, .centerY)
        
        timecodeImageView = UIImageView(frame: CGRect(x: 0, y: 0, width: 16, height: 16))
        v.addSubview(timecodeImageView)

        timecodeImageView.translatesAutoresizingMaskIntoConstraints = false
        timecodeImageView.set(.height, .width, of: 16)
        timecodeImageView.layoutToSuperview(.centerY, offset: -2)
        timecodeImageView.layoutToSuperview(.left)
        timecodeImageView.contentMode = .scaleAspectFit

        timecodeLabel = UILabel()
        v.addSubview(timecodeLabel)
        
        timecodeLabel.font = labelFont
        timecodeLabel.textColor = .white
        timecodeLabel.textAlignment = .center
        
        timecodeLabel.layoutToSuperview(.centerY)
        timecodeLabel.layout(.left, to: .right, of: timecodeImageView, offset: 4)

        subjectLabel = UILabel()
        v.addSubview(subjectLabel)
        
        subjectLabel.font = labelFont
        subjectLabel.textColor = .white
        subjectLabel.textAlignment = .center
        
        subjectLabel.layout(.left, to: .right, of: timecodeLabel)
        subjectLabel.layoutToSuperview(.centerY)

        streamingStatsLabel = UILabel()
        v.addSubview(streamingStatsLabel)
        
        streamingStatsLabel.font = labelFont
        streamingStatsLabel.textColor = .white
        streamingStatsLabel.textAlignment = .center

        streamingStatsLabel.layout(.left, to: .right, of: subjectLabel)
        streamingStatsLabel.layoutToSuperview(.centerY, .right)
        
        if showButtons {
            
            let btn = UIButton(type: .custom)
            btn.translatesAutoresizingMaskIntoConstraints = false
            addSubview(btn)
            
            btn.imageView?.contentMode = .scaleAspectFit
            btn.tintColor = UIColor.white
            btn.setImage(UIImage(named: "Exit"), for: .normal)

            btn.set(.width, .height, of: 30)
            btn.layoutToSuperview(.centerY)
            btn.layoutToSuperview(.right, offset: -50)
            
            btn.addTarget(self, action: #selector(exitTapped), for: .touchUpInside)
        }

        if showButtons {
            
            let btn = UIButton(type: .custom)
            btn.translatesAutoresizingMaskIntoConstraints = false
            addSubview(btn)
            
            btn.imageView?.contentMode = .scaleAspectFit
            btn.tintColor = UIColor.white
            btn.setImage(UIImage(systemName: "book"), for: .normal)

            btn.set(.width, .height, of: 30)
            btn.layoutToSuperview(.centerY)
            btn.layoutToSuperview(.left, offset: 50)
            
            btn.addTarget(self, action: #selector(logTapped), for: .touchUpInside)
        }
        
        if showButtons {
            
            let border = CALayer()
            border.frame = CGRect(x: 0, y: frame.height, width: frame.width, height: 1.0)
            border.backgroundColor = UIColor.white.withAlphaComponent(0.25).cgColor
            
            layer.addSublayer(border)
        }


        observers.append(observe(\.appSettings.liveLinkSubjectName, options: [.initial,.new], changeHandler: { object, change in
            self.subject = self.appSettings.liveLinkSubjectName
        }))
        
        observers.append(observe(\.appSettings.timecodeSource, options: [.initial,.new], changeHandler: { object, change in
            
            switch self.appSettings.timecodeSourceEnum() {
            case .systemTime:
                self.timecodeImageView.image = UIImage(named: "counterDevice")
            case .ntp:
                self.timecodeImageView.image = UIImage(named: "counterCloud")
            case .tentacleSync:
                self.timecodeImageView.image = UIImage(named: "tentacle")
            default:
                break
            }
        }))
        
        observers.append(observe(\.appSettings.showStreamingStats, options: [.initial,.new], changeHandler: { object, change in
            
        }))
    }
    
    func start() {
        // display link updates the timecode value
        displayLink = CADisplayLink(
          target: self, selector: #selector(displayLinkDidFire)
        )
        displayLink.add(to: .main, forMode: .common)
    }
    
    func stop() {
        displayLink.invalidate()
        displayLink = nil
    }
    
    @objc func exitTapped(_ sender : Any) {
        if let d = self.delegate {
            d.headerViewExitButtonTapped(self)
        }
    }

    @objc func logTapped(_ sender : Any) {
        if let d = self.delegate {
            d.headerViewLogButtonTapped(self)
        }
    }


    @objc func displayLinkDidFire(_ displayLink: CADisplayLink) {

        self.timecodeLabel.text = Timecode.create().toString(includeFractional: false)
    }
}
