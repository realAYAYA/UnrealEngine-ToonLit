//
//  StartViewController.swift
//  vcam
//
//  Created by Brian Smith on 11/10/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import CocoaAsyncSocket
import Kronos
import GameController

class StartViewController : BaseViewController {

    @IBOutlet weak var headerView : HeaderView!
    @IBOutlet weak var versionLabel : UILabel!

    @IBOutlet weak var restartView : UIView!

    @IBOutlet weak var entryView : UIView!
    @IBOutlet weak var entryViewYConstraint : NSLayoutConstraint!

    @IBOutlet weak var ipAddress : UITextField!
    @IBOutlet weak var connect : UIButton!

    @IBOutlet weak var connectingView : UIVisualEffectView!

    private var tapGesture : UITapGestureRecognizer!
    
    private var streamingConnection : StreamingConnection?
    private var _liveLinkTimer : Timer?

    
    @objc dynamic let appSettings = AppSettings.shared
    private var observers = [NSKeyValueObservation]()
    
    private var gameController : GCController?

    var ipAddressIsDemoMode : Bool {
        self.ipAddress.text == "demo.mode"
    }
    
    override var preferredStatusBarStyle: UIStatusBarStyle {
          return .lightContent
    }
    
    override func viewDidLoad() {
        
        super.viewDidLoad();
        
        self.restartView.isHidden = true
        
        if let infoDict = Bundle.main.infoDictionary {
            self.versionLabel.text = String(format: "v%@ (%@)", infoDict["CFBundleShortVersionString"] as! String, infoDict["CFBundleVersion"] as! String)
        } else {
            self.versionLabel.text = "";
        }
        
        NetUtility.triggerLocalNetworkPrivacyAlert()
        
        self.ipAddress.text = AppSettings.shared.lastConnectionAddress
        textFieldChanged(self.ipAddress)
        
        self.tapGesture = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        self.tapGesture.cancelsTouchesInView = false
        self.tapGesture.delegate = self
        self.view.addGestureRecognizer(tapGesture)
        
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillShow), name: UIResponder.keyboardWillShowNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillHide), name: UIResponder.keyboardWillHideNotification, object: nil)
        
        observers.append(observe(\.appSettings.timecodeSource, options: [.initial,.new,.old], changeHandler: { object, change in
            
            if let oldValue = TimecodeSource(rawValue:change.oldValue ?? 0) {
                switch oldValue {
                case .ntp:
                    Clock.reset()
                case .tentacleSync:
                    Tentacle.shared = nil
                default:
                    break
                }
            }
            
            switch self.appSettings.timecodeSourceEnum() {
            case .ntp:
                let pool = AppSettings.shared.ntpPool.isEmpty ? "time.apple.com" : AppSettings.shared.ntpPool
                Log.info("Started NTP : \(pool)")
                
                // IMPORTANT
                // We reset the NTP clock first --
                // otherwise we don't know if the NTP address that is used for the pool is valid or not because it
                // will be using a stale last valid time
                Clock.reset()
                Clock.sync(from: pool)
            case .tentacleSync:
                Tentacle.shared = Tentacle()
            default:
                break
            }
            
        }))
        
        // any change to the subject name will remove & re-add the camera subject.
        observers.append(observe(\.appSettings.liveLinkSubjectName, options: [.old,.new], changeHandler: { object, change in
            if let sc = self.streamingConnection {
                sc.subjectName = self.appSettings.liveLinkSubjectName
            }
        }))

        // initial & value changes for the connection type instantiates a new StreamingConnection object
        observers.append(observe(\.appSettings.connectionType, options: [.initial, .old,.new], changeHandler: { object, change in
            
            self.streamingConnection = nil

            let connectionType = self.appSettings.connectionType
            if let connectionClass = Bundle.main.classNamed("VCAM.\(connectionType)StreamingConnection") as? StreamingConnection.Type {
                self.streamingConnection = connectionClass.init(subjectName: self.appSettings.liveLinkSubjectName)
                self.streamingConnection?.delegate = self
            }

        }))

        NotificationCenter.default.addObserver(self, selector: #selector(gameControllerDidConnectNotification), name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(gameControllerDidDisconnectNotification), name: .GCControllerDidDisconnect, object: nil)
        
        self.gameController = GCController.controllers().first
        if let gc = self.gameController {
            if gc.isAttachedToDevice {
                gc.playerIndex = .index1
            }
        }
    }
    

    
    override func viewWillAppear(_ animated : Bool) {
        
        self.connectingView.isHidden = true
        self.headerView.start()

        self.streamingConnection?.delegate = self
        
        _liveLinkTimer = Timer.scheduledTimer(withTimeInterval: 1.0/10.0, repeats: true, block: { timer in
            self.streamingConnection?.sendTransform(simd_float4x4(), atTime: Timecode.create().toTimeInterval())
        })
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        self.headerView.stop()
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {

        // hide the keyboard if it was being shown
        self.view.endEditing(true)

        if segue.identifier == "showVideoView" {

            if let vc = segue.destination as? VideoViewController {
                
                // stop the timer locally which is sending LL identity xform
                _liveLinkTimer?.invalidate()
                _liveLinkTimer = nil

                self.streamingConnection?.delegate = vc
                vc.streamingConnection = self.streamingConnection
                
                vc.gameController = self.gameController
            }
        }
    }
    
    @objc func keyboardWillShow(notification: NSNotification) {
        
        guard let userInfo = notification.userInfo else {return}

        // if the keyboard will overlap the connect button, then we move the view up so that nothing
        // is obscured. In cases where there is a keyboard connected, the toolbar is shown only and
        // nothing will move.
        
        let keyboardFrame = self.view.convert((userInfo[UIResponder.keyboardFrameEndUserInfoKey] as! NSValue).cgRectValue, from: self.view.window)
        let connectButtonFrame = self.view.convert(connect.frame, from: connect.superview)
        
        if keyboardFrame.minY < connectButtonFrame.maxY {
            self.entryViewYConstraint.constant = -(self.view.frame.height - keyboardFrame.size.height) / 2.0
        } else {
            self.entryViewYConstraint.constant = 0
        }

        UIView.animate(withDuration: 0.2) {
            self.view.layoutIfNeeded()
        }
    }

    @objc func keyboardWillHide(notification: NSNotification) {
        if self.entryViewYConstraint.constant != 0 {
            self.entryViewYConstraint.constant = 0

            UIView.animate(withDuration: 0.2) {
                self.view.layoutIfNeeded()
            }
        }
    }
    
    @objc func handleTap(_ recognizer: UITapGestureRecognizer) {
        
        self.view.endEditing(true)
    }
    
    @IBAction func connect(_ sender : Any?) {
        
        AppSettings.shared.lastConnectionAddress = self.ipAddress.text!

        if self.ipAddressIsDemoMode {
            
            self.performSegue(withIdentifier: "showVideoViewDemoMode", sender: self)

        } else {

            // show the connection view
            self.connectingView.isHidden = false
            self.connectingView.alpha = 0.0
            UIView.animate(withDuration: 0.2) {
                self.connectingView.alpha = 1.0
            }
            
            showConnectingAlertView(mode: .connecting) {
                self.hideConnectingView() { }
            }

            do {
                self.streamingConnection?.destination = self.ipAddress.text!
                try self.streamingConnection?.connect()
                
            } catch {
                
                hideConnectingAlertView() {

                    let errorAlert = UIAlertController(title: Localized.titleError(), message: "Couldn't connect : \(error.localizedDescription)", preferredStyle: .alert)
                    errorAlert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default, handler: { _ in
                        self.hideConnectingView { }
                    }))
                    self.present(errorAlert, animated: true)
                }
            }
        }
    }
    
    func hideConnectingView( _ completion : @escaping () -> Void) {
        UIView.animate(withDuration: 0.2, animations: {
            self.connectingView.alpha = 0.0
        }, completion: { b in
            self.connectingView.isHidden = true
        })
        hideConnectingAlertView(completion)
    }
    
    @IBAction func textFieldChanged(_ sender : Any?) {
        self.connect.isEnabled = !self.ipAddress.text!.isEmpty
    }
    
    @objc func gameControllerDidConnectNotification(_ notification: NSNotification) {
        
        self.gameController = notification.object as? GCController
        self.gameController?.playerIndex = .index1
        
        if let videoViewController = self.presentedViewController as? VideoViewController {
            videoViewController.gameController = self.gameController
        }
    }

    @objc func gameControllerDidDisconnectNotification(_ notification: NSNotification) {
        if let gc = notification.object as? GCController {
            Log.info("gameControllerDidDisconnectNotification \(gc.vendorName ?? "Unknown controller")")
        }
    }

}

extension StartViewController : UIGestureRecognizerDelegate {
    
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer, shouldReceive touch: UITouch) -> Bool {
        
        return touch.view != self.connect && touch.view != self.ipAddress
    }
}


extension StartViewController : UITextFieldDelegate {
    
    func textFieldShouldReturn(_ textField: UITextField) -> Bool {
        textField.resignFirstResponder()
        
        connect(textField)
        
        return true
    }
}

