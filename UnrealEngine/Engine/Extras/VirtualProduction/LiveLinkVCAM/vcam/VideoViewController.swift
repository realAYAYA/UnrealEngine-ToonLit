//
//  ViewController.swift
//  vcam
//
//  Created by Brian Smith on 8/8/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit
import MetalKit
import ARKit
import Easing
import GameController

class VideoViewController : BaseViewController {

    var demoMode : Bool {
        liveLink == nil
    }

    private var displayLink: CADisplayLink?

    private var arSession : ARSession?
    
    @IBOutlet weak var renderView : UIView!
    
    @IBOutlet weak var headerView : HeaderView!
    @IBOutlet weak var headerViewTopConstraint : NSLayoutConstraint!
    var headerViewY : CGFloat = 0
    var headerViewHeight : CGFloat = 0
    var headerViewTopConstraintStartValue : CGFloat = 0
    var headerPanGestureRecognizer : UIPanGestureRecognizer!
    var headerPullDownGestureRecognizer : UIScreenEdgePanGestureRecognizer!
    
    var streamingConnection : StreamingConnection?
    var pickerData: [String] = [String]()
    var selectedStreamer: String = ""

    weak var liveLink : LiveLinkProvider?
    var dismissOnDisconnect = false
    var forceDisconnectTimer : Timer?
    
    var statsTimer : Timer?
    var showStats : Bool = false
    
    var gameControllerSnapshot : GCController?
    weak var gameController : GCController? {
        didSet {
            if let gc = gameController {
                gc.extendedGamepad?.dpad.up.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.dpad.down.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.dpad.left.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.dpad.right.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonA.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonB.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonX.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonY.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.leftShoulder.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.rightShoulder.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.leftTrigger.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.rightTrigger.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.leftThumbstickButton?.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.rightThumbstickButton?.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonOptions?.pressedChangedHandler = self.buttonValueChanged
                gc.extendedGamepad?.buttonMenu.pressedChangedHandler = self.buttonValueChanged
            }
        }
    }
    
    private var gamepads: [GCControllerPlayerIndex : Gamepad] = [:];
    
    @IBOutlet weak var arView : ARSCNView!

    @IBOutlet weak var reconnectingBlurView : UIVisualEffectView!

    
    override var prefersHomeIndicatorAutoHidden: Bool {
        return true
    }
    
    override var prefersStatusBarHidden: Bool {
        return true
    }
    
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge {
        return [.top]
    }
    
    @objc func applicationDidBecomeActive(notification: NSNotification) {
        reconnect()
    }
    @objc func applicationDidEnterBackground(notification: NSNotification) {
        disconnect()
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        NotificationCenter.default.addObserver(self, selector: #selector(applicationDidBecomeActive), name: UIApplication.willEnterForegroundNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(applicationDidEnterBackground), name: UIApplication.didEnterBackgroundNotification, object: nil)
        
        self.headerView.start()
        headerViewHeight = self.headerView.frame.height

        headerPanGestureRecognizer = UIPanGestureRecognizer(target: self, action: #selector(handleHeaderPanGesture))
        self.headerView.addGestureRecognizer(headerPanGestureRecognizer)

        headerPullDownGestureRecognizer = UIScreenEdgePanGestureRecognizer(target: self, action: #selector(handleHeaderPanGesture))
        headerPullDownGestureRecognizer.edges = [ .top ]
        self.view.addGestureRecognizer(headerPullDownGestureRecognizer)
        
        headerPanGestureRecognizer.require(toFail: headerPullDownGestureRecognizer)
        
        // The AR video feed & CG objects only exist in demo mode. In normal operation, they are replaced by a live feed of the Unreal Engine rendering, with the virtual camera's position determined by the device & ARKit.
        //self.demoModeBlurView.isHidden = !self.demoMode
        showReconnecting(false, animated: false)
        
        let config = ARWorldTrackingConfiguration()
        config.worldAlignment = .gravity
        
        arSession = ARSession()
        arSession?.delegate = self
        arSession?.run(config)
        
        self.streamingConnection?.renderView = self.renderView
        
        
        let coachingOverlayView = ARCoachingOverlayView()
        
        self.view.insertSubview(coachingOverlayView, belowSubview: self.reconnectingBlurView)
        coachingOverlayView.layout(.left, .right, .top, .bottom, to: self.arView)
        coachingOverlayView.goal = .tracking
        coachingOverlayView.session = self.arSession
        coachingOverlayView.activatesAutomatically = true
        coachingOverlayView.delegate = self

        statsTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true, block: { t in
            self.updateStreamingStats()
        })
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        UIApplication.shared.isIdleTimerDisabled = true
        
        // Check for already connected controllers
        for gc in GCController.controllers() {
            self.controllerConnected(gamepad: gc)
        }

        let notificationCenter = NotificationCenter.default;
        
        // Add notifications for when new controllers connect or disconnect
        notificationCenter.addObserver(forName: .GCControllerDidConnect, object: nil, queue: .main) { (note) in
            guard let gc = note.object as? GCController else { return }
            self.controllerConnected(gamepad: gc)
        }
        
        notificationCenter.addObserver(forName: .GCControllerDidDisconnect, object: nil, queue: .main) { (note) in
            guard let gc = note.object as? GCController else { return }
            self.controllerDisconnected(gamepad: gc)
        }
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        UIApplication.shared.isIdleTimerDisabled = false
        
        let notificationCenter = NotificationCenter.default
        notificationCenter.removeObserver(self, name: .GCControllerDidConnect, object: nil)
        notificationCenter.removeObserver(self, name: .GCControllerDidDisconnect, object: nil)
    }
    
    func showReconnecting(_ visible : Bool, animated: Bool) {

        if (self.reconnectingBlurView.effect != nil && visible) ||
            (self.reconnectingBlurView.effect == nil && !visible) {
            return
        }

        UIView.animate(withDuration: 0.0, animations: {
            self.reconnectingBlurView.effect = visible ? UIBlurEffect(style: UIBlurEffect.Style.dark) : nil
        })

        if visible {
            showConnectingAlertView(mode : .reconnecting) {
                self.exit()
            }
        } else {
            hideConnectingAlertView() {}
        }
    }

    func exit() {
        
        // if still connected, we need to send a disconnect/goodbye, then dismiss the view controller when
        // the goodbye msg is sent.
        // otherwise, we can call disconnect (which tears down some objects) and dismiss this VC immediately.
        if self.streamingConnection?.isConnected ?? false {
            dismissOnDisconnect = true
            disconnect()
        } else {
            self.forceDisconnectAndDismiss()
        }
    }
    
    func reconnect() {
        
        showReconnecting(true, animated: true)
        self.streamingConnection?.reconnect()
    }
    
    func disconnect() {
        self.forceDisconnectTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: false, block: { timer in
            Log.info("Forcing disconnect due to timeout.")
            self.forceDisconnectAndDismiss()
        })
        // Tell UE that the controllers connected to this device are no longer used
        for gc in GCController.controllers() {
            self.controllerDisconnected(gamepad: gc)
        }
        
        self.streamingConnection?.disconnect()
    }
    
    func forceDisconnectAndDismiss() {
        self.forceDisconnectTimer?.invalidate()
        self.forceDisconnectTimer = nil
        self.streamingConnection?.delegate = nil
        self.streamingConnection = nil
        self.presentingViewController?.dismiss(animated: true, completion: nil)
    }
    
    func updateStreamingStats() {

        var str = ""
        
        if let sz = self.streamingConnection?.videoSize {
            str += "\(Int(sz.width))x\(Int(sz.height))"
        }
        
        if let stats = self.streamingConnection?.stats {
                
            if let bps = stats.bytesPerSecond {
                if !str.isEmpty {
                    str += " • "
                }
             
                str += ByteCountFormatter().string(fromByteCount: Int64(bps)) + "/sec"
            }
            
            if let fps = stats.framesPerSecond {
                if !str.isEmpty {
                    str += " • "
                }
             
                str += "\(fps) fps"
            }
            
        }

        self.headerView.stats = str
    }

    @objc func handleHeaderPanGesture(_ gesture : UIGestureRecognizer) {

        guard let panGesture = gesture as? UIPanGestureRecognizer else { return }
        
        switch gesture.state {
        case .began, .changed, .ended:
            updateHeaderConstraint(gesture : panGesture)
        default:
            break
        }
    }
    
    func updateHeaderConstraint(gesture pan : UIPanGestureRecognizer) {

        // if gesture is starting, keep track of the start constraint value
        if pan.state == .began {
            headerViewTopConstraintStartValue = headerViewTopConstraint.constant
        }

        if pan.state == .began || pan.state == .changed {

            // use the correct view for the given gesture's translate
            let translation = pan.translation(in: pan is UIScreenEdgePanGestureRecognizer ? view : headerView)

            // add the translation to the start value
            headerViewTopConstraint.constant = headerViewTopConstraintStartValue + translation.y
            
            // if the constant is now over 1, we will stretch downward. We use a sine easeOut to give
            // a rubber-band effect
            if headerViewTopConstraint.constant > 0 {
                let t = Float(headerViewTopConstraint.constant) / Float(UIScreen.main.bounds.height)
                let t2 = CGFloat(Curve.sine.easeOut(t))
                
                //og.info("\(headerViewTopConstraint.constant) -> \(t) -> \(t2)")
                
                headerViewTopConstraint.constant = t2 * headerView.frame.height * 2.0
            }
        } else if pan.state == .ended {
            
            // gesture is ending, we will animate to either visible or hidden
            let newTopY : CGFloat = headerViewTopConstraint.constant > -(headerView.frame.height * 0.25) ? 0.0 : (-self.headerView.frame.height - 1.0)
            
            // we also need a display link here to properly communicate the new headerY from the presentation layer to
            // the metal renderer.
            self.displayLink = CADisplayLink(
              target: self, selector: #selector(displayLinkDidFire)
            )
            self.displayLink!.add(to: .main, forMode: .common)

            UIView.animate(withDuration: 0.2) {
                // animate to the new position
                self.headerViewTopConstraint.constant = newTopY
                self.view.layoutIfNeeded()
            } completion: { _ in
                // all done, we kill the display link
                self.displayLink?.invalidate()
                self.displayLink = nil
            }
        }
    }
    
    @objc func displayLinkDidFire(_ displayLink: CADisplayLink) {

        // save the actual animating value of the header view's Y position
        self.headerViewY = self.headerView.layer.presentation()?.frame.minY ?? 0
    }
    
    func sendControllerThumbstickUpdate() {
        
        guard let sc = streamingConnection else { return }
        guard let gc = gameController else { return }
        
        let snapshot = gc.capture()
        guard let gp = snapshot.extendedGamepad else { return }

        guard let controller = gamepads[gc.playerIndex] else { return }
        // Fallback to default funcationality if we haven't received an ID from UE. This could happen if using latest app with UE < 5.2
        let controllerIndex = controller.id ?? UInt8(gc.playerIndex.rawValue)
        
        // dict of type -> oldValue, newValue
        let inputs : [StreamingConnectionControllerInputType : ( oldValue: Float?, newValue : Float)]  = [
            .thumbstickLeftX : ( self.gameControllerSnapshot?.extendedGamepad?.leftThumbstick.xAxis.value,  gp.leftThumbstick.xAxis.value),
            .thumbstickLeftY : ( self.gameControllerSnapshot?.extendedGamepad?.leftThumbstick.yAxis.value,  gp.leftThumbstick.yAxis.value),
            .thumbstickRightX : ( self.gameControllerSnapshot?.extendedGamepad?.rightThumbstick.xAxis.value,  gp.rightThumbstick.xAxis.value),
            .thumbstickRightY : ( self.gameControllerSnapshot?.extendedGamepad?.rightThumbstick.yAxis.value,  gp.rightThumbstick.yAxis.value),
        ]
        
        for input in inputs {
            
            if (input.value.newValue != 0.0) || ((input.value.oldValue ?? 0.0) != input.value.newValue) {
                sc.sendControllerAnalog(input.key, controllerIndex: UInt8(controllerIndex), value: input.value.newValue)
            }
        }

        self.gameControllerSnapshot = snapshot
    }

    func buttonValueChanged(button : GCControllerButtonInput, value : Float, pressed : Bool) {

        
        guard let sc = streamingConnection else { return }
        guard let gc = gameController else { return }
        guard let gp = gc.extendedGamepad else { return }

        guard let controller = gamepads[gc.playerIndex] else { return }
        // Fallback to default functionality if we haven't received an ID from UE. This could happen if using latest app with UE < 5.2
        let controllerIndex = controller.id ?? UInt8(gc.playerIndex.rawValue)
        
        let isRepeat = false
        
        var inputType : StreamingConnectionControllerInputType!

        switch button {
        case gp.leftThumbstickButton:
            inputType = .thumbstickLeftButton
        case gp.rightThumbstickButton:
            inputType = .thumbstickRightButton
        case gp.buttonA:
            inputType = .faceButtonBottom
        case gp.buttonB:
            inputType = .faceButtonRight
        case gp.buttonX:
            inputType = .faceButtonLeft
        case gp.buttonY:
            inputType = .faceButtonTop
        case gp.leftShoulder:
            inputType = .shoulderButtonLeft
        case gp.rightShoulder:
            inputType = .shoulderButtonRight
        case gp.leftTrigger:
            inputType = .triggerButtonLeft
        case gp.rightTrigger:
            inputType = .triggerButtonRight
        case gp.dpad.up:
            inputType = .dpadUp
        case gp.dpad.down:
            inputType = .dpadDown
        case gp.dpad.left:
            inputType = .dpadLeft
        case gp.dpad.right:
            inputType = .dpadRight
        case gp.buttonOptions:
            inputType = .specialButtonLeft
        case gp.buttonMenu:
            inputType = .specialButtonRight
        default:
            Log.warning("Couldn't find mapping for input button \(button.description)")
            return
        }
        
        if button.isPressed {
            sc.sendControllerButtonPressed(inputType!, controllerIndex: UInt8(controllerIndex), isRepeat: isRepeat)
        } else {
            sc.sendControllerButtonReleased(inputType!, controllerIndex: UInt8(controllerIndex))
        }
    }
    
    func controllerConnected(gamepad: GCController) {
        guard let sc = streamingConnection else { return }
        sc.sendControllerConnected();
        
        let newGamepad = Gamepad()
        newGamepad.controller = gamepad
        gamepads[gamepad.playerIndex] = newGamepad
    }
    
    func controllerDisconnected(gamepad: GCController) {
        guard let sc = streamingConnection else { return }
        sc.sendControllerDisconnected(controllerIndex: gamepads[gamepad.playerIndex]!.id!);
        
        gamepads.removeValue(forKey: gamepad.playerIndex)
    }
    
    func controllerResponseReceived(controllerIndex: UInt8) {
        for gamepad in gamepads.values {
            if(gamepad.id == nil) {
                gamepad.id = controllerIndex;
                break;
            }
        }
    }
}

extension VideoViewController : HeaderViewDelegate {

    func headerViewStatsButtonTapped(_ headerView: HeaderView) {
        self.showStats = !self.showStats
        streamingConnection?.showStats(self.showStats)
    }
    
    func headerViewExitButtonTapped(_ headerView : HeaderView) {
     
        let disconnectAlert = UIAlertController(title: nil, message: NSLocalizedString("Disconnect from the remote session?", comment: "Prompt disconnect from a remote session."), preferredStyle: .alert)
        disconnectAlert.addAction(UIAlertAction(title: NSLocalizedString("Disconnect", comment: "Button to disconnect from a UE instance"), style: .destructive, handler: { _ in
            self.exit()
        }))
        disconnectAlert.addAction(UIAlertAction(title: Localized.buttonCancel(), style: .cancel))
        self.present(disconnectAlert, animated:true)
    }

    func headerViewLogButtonTapped(_ headerView : HeaderView) {
     
        performSegue(withIdentifier: "showLog", sender: headerView)
    }
    
}


extension VideoViewController: UIPickerViewDelegate, UIPickerViewDataSource {
    func numberOfComponents(in pickerView: UIPickerView) -> Int {
        // Number of columns
        return 1
    }
    
    func pickerView(_ pickerView: UIPickerView, numberOfRowsInComponent component: Int) -> Int {
        // Number of rows
        return pickerData.count
    }
    
    func pickerView(_ pickerView: UIPickerView, titleForRow row: Int, forComponent component: Int) -> String? {
        return pickerData[row]
    }
    
    func pickerView(_ pickerView: UIPickerView, didSelectRow row: Int, inComponent component: Int) {
        selectedStreamer = pickerData[row]
    }
}
