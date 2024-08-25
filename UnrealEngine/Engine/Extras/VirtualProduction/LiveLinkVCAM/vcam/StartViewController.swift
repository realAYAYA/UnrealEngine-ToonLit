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
    private var gameController : GCController?
    private var _liveLinkTimer : Timer?

    
    @objc dynamic let appSettings = AppSettings.shared
    private var observers = [NSKeyValueObservation]()
    
    var pickerData: [String] = [String]()
    var selectedStreamer: String = "";

    var ipAddressIsDemoMode : Bool {
        self.ipAddress.text == "demo.mode"
    }
    
    override var preferredStatusBarStyle: UIStatusBarStyle {
          return .lightContent
    }
    
    // Constructor (before view is even loaded)
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupObservers()
    }
    
    deinit {
        Log.info("StartViewController destructed")
    }
    
    func setupObservers() {
        // Observers for keyboard show/hide
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillShow), name: UIResponder.keyboardWillShowNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillHide), name: UIResponder.keyboardWillHideNotification, object: nil)
        
        // Observers for game controller connect/disconnect
        NotificationCenter.default.addObserver(self, selector: #selector(gameControllerDidConnectNotification), name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(gameControllerDidDisconnectNotification), name: .GCControllerDidDisconnect, object: nil)
        
        // Observers for app settings
        observers.append(observe(\.appSettings.timecodeSource, options: [.initial,.new,.old], changeHandler: { [weak self] object, change in
            
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
            
            switch self?.appSettings.timecodeSourceEnum() {
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
        observers.append(observe(\.appSettings.liveLinkSubjectName, options: [.old,.new], changeHandler: { [weak self] object, change in
            if let sc = self?.streamingConnection {
                sc.subjectName = self?.appSettings.liveLinkSubjectName
            }
        }))

        // initial & value changes for the connection type instantiates a new StreamingConnection object
        observers.append(observe(\.appSettings.connectionType, options: [.initial, .old,.new], changeHandler: { [weak self] object, change in
            
            if let validSelf = self {
                validSelf.streamingConnection?.shutdown()
                validSelf.streamingConnection = nil

                let connectionType = validSelf.appSettings.connectionType
                if let connectionClass = Bundle.main.classNamed("VCAM.\(connectionType)StreamingConnection") as? StreamingConnection.Type {
                    validSelf.streamingConnection = connectionClass.init(subjectName: validSelf.appSettings.liveLinkSubjectName)
                    validSelf.streamingConnection?.delegate = validSelf
                }
            }
        }))
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
        self.ipAddress.inputAssistantItem.leadingBarButtonGroups.removeAll()
        self.ipAddress.inputAssistantItem.trailingBarButtonGroups.removeAll()
        textFieldChanged(self.ipAddress)
        
        self.rebuildRecentAddressesBarButtons()

        // Add gesture recognizer
        self.tapGesture = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        self.tapGesture.cancelsTouchesInView = false
        self.tapGesture.delegate = self
        self.view.addGestureRecognizer(tapGesture)
        
        // Attempt to get an attached game controller
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
        
        _liveLinkTimer = Timer.scheduledTimer(withTimeInterval: 1.0/10.0, repeats: true, block: { [weak self] timer in
            self?.streamingConnection?.sendTransform(simd_float4x4(), atTime: Timecode.create().toTimeInterval())
        })
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        self.headerView.start()
        self.streamingConnection?.disconnect()
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        self.headerView.stop()
        
        // Remove gesture recognizer
        self.view.removeGestureRecognizer(self.tapGesture)
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {

        // hide the keyboard if it was being shown
        self.view.endEditing(true)

        if segue.identifier == "showVideoView" {

            // connection was successful, we save the last address in our recents list
            AppSettings.shared.addRecentConnectionAddress(AppSettings.shared.lastConnectionAddress)
            self.rebuildRecentAddressesBarButtons()

            if let vc = segue.destination as? VideoViewController {
                
                // stop the timer locally which is sending LL identity xform
                _liveLinkTimer?.invalidate()
                _liveLinkTimer = nil

                // This a little awkward but what happens here is the streamingConnection and gameController gets
                // are passed (weakly) from StartViewController to VideoViewController as it needs them
                // They are not nil'd in this view because this view still exists because it is the ancestor of the segue
                vc.streamingConnection = self.streamingConnection
                vc.streamingConnection?.delegate = vc
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
            self.entryViewYConstraint.constant = -(keyboardFrame.size.height - self.headerView.frame.height) / 2.0
        } else {
            self.entryViewYConstraint.constant = 0
        }

        UIView.animate(withDuration: 0.2) { [weak self] in
            self?.view.layoutIfNeeded()
        }
    }

    @objc func keyboardWillHide(notification: NSNotification) {
        if self.entryViewYConstraint.constant != 0 {
            self.entryViewYConstraint.constant = 0

            UIView.animate(withDuration: 0.2) { [weak self] in
                self?.view.layoutIfNeeded()
            }
        }
    }
    
    func rebuildRecentAddressesBarButtons() {

        var view : UIView?

        if let addresses = AppSettings.shared.recentConnectionAddresses {
            for address in addresses {
                
                if view == nil {
                    view = UIView()
                }

                let item = UIButton(configuration: UIButton.Configuration.gray())
                item.setTitle(address, for: .normal)
                item.setTitleColor(UIColor.white, for: .normal)
                item.addTarget(self, action: #selector(handleRecentAddressSelection), for: .touchUpInside)
                
                view!.addSubview(item)
                item.layoutToSuperview(.centerY)
                
                if view!.subviews.count == 1 {
                    item.layoutToSuperview(.left)
                } else {
                    item.layout(.left, to: .right, of: view!.subviews[view!.subviews.count - 2], offset: 20)
                }
            }

        }
        
        if let v = view {
            v.subviews.last?.layoutToSuperview(.right)
            let inputView = UIInputView(frame: CGRect(x: 0, y: 0, width: 100, height: 50), inputViewStyle: .keyboard)
            inputView.addSubview(v)
            v.layoutToSuperview(.top, offset: 4)
            v.layoutToSuperview(.centerX, .bottom)
            
            self.ipAddress.inputAccessoryView = inputView  // inputAssistantItem.leadingBarButtonGroups.append(group)
        }
        
    }
    
    @objc func handleRecentAddressSelection(_ sender : Any?) {
        if let btn = sender as? UIButton {
            if let addr = btn.title(for: .normal) {
                self.ipAddress.text = addr
                self.ipAddress.resignFirstResponder()
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
            UIView.animate(withDuration: 0.2) { [weak self] in
                self?.connectingView.alpha = 1.0
            }
            
            showConnectingAlertView(mode: .connecting, { [weak self] in
                self?.hideConnectingView() {}
            })

            do {
                self.streamingConnection?.destination = self.ipAddress.text!.trimmed()
                try self.streamingConnection?.connect()
                
            } catch StreamingConnectionError.runtimeError(let errorMessage) {
                
                showConnectionErrorAlert(errorMessage)

            } catch {
                
                showConnectionErrorAlert("\(Localized.messageCouldntConnect()) : \(error.localizedDescription)")
            }
        }
    }
    
    func showConnectionErrorAlert(_ message : String) {
        hideConnectingAlertView() {

            let errorAlert = UIAlertController(title: Localized.titleError(), message: "\(Localized.messageCouldntConnect()) : \(message)", preferredStyle: .alert)
            errorAlert.addAction(UIAlertAction(title: Localized.buttonOK(), style: .default, handler: { [weak self] _ in
                self?.hideConnectingView { }
            }))
            self.present(errorAlert, animated: true)
        }
    }
    
    func hideConnectingView( _ completion : @escaping () -> Void) {
        UIView.animate(withDuration: 0.2, animations: { [weak self] in
            self?.connectingView.alpha = 0.0
        }, completion: { [weak self] b in
            self?.connectingView.isHidden = true
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

extension StartViewController : UIPickerViewDelegate, UIPickerViewDataSource {
    func numberOfComponents(in pickerView: UIPickerView) -> Int {
        // Number of columns
        return 1;
    }
    
    func pickerView(_ pickerView: UIPickerView, numberOfRowsInComponent component: Int) -> Int {
        // Number of rows
        return pickerData.count
    }
    
    func pickerView(_ pickerView: UIPickerView, titleForRow row: Int, forComponent component: Int) -> String? {
        return pickerData[row]
    }
    
    func pickerView(_ pickerView: UIPickerView, didSelectRow row: Int, inComponent component: Int) {
            // This method is triggered whenever the user makes a change to the picker selection.
            // The parameter named row and component represents what was selected.
        selectedStreamer = pickerData[row]
    }
}
