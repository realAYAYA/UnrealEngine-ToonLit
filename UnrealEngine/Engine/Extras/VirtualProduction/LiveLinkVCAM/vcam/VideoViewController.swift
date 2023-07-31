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

    weak var liveLink : LiveLinkProvider?
    var dismissOnDisconnect = false
    
    var statsTimer : Timer?
    
    var gameControllerSnapshot : GCController?
    weak var gameController : GCController? {
        didSet {
            if let gc = gameController {
                //gc.extendedGamepad?.leftThumbstick.valueChangedHandler = self.directionalPadValueChanged
                //gc.extendedGamepad?.rightThumbstick.valueChangedHandler = self.directionalPadValueChanged
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
    
    var relayTouchEvents = true {
        didSet {

            // if we are stopping touch events sent to OSC, then we need to send an "ended" event
            // for all of the current touches so the UI doesn't think it's still occurring
            /*
            if relayTouchEvents == false {

                for i in 0..<self.currentTouches.count {
                    if let t = self.currentTouches[i] {
                        self.sendTouch(.ended, point: t.location(in: self.renderView), finger: i, force: 0);
                    }
                }
                self.currentTouches.removeAll()

            }
             */
            
        }
    }

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
        
        /*
        
        arView.session.run(config, options: [ .resetTracking ] )
        arView.session.delegate = self
        arView.delegate = self
        
        if self.demoMode {
            arView.automaticallyUpdatesLighting = true
            arView.debugOptions = [ .showWorldOrigin ]

            let floor = SCNNode(geometry: SCNPlane(width: 14, height: 14))
            floor.rotation = SCNVector4(1, 0, 0, GLKMathDegreesToRadians(-90))
            floor.position = SCNVector3(0, -1, 0)
            floor.geometry?.firstMaterial?.diffuse.contents = UIImage(named: "checkerboard")
            floor.geometry?.firstMaterial?.diffuse.contentsTransform = SCNMatrix4MakeScale(50, 50, 0)
            floor.geometry?.firstMaterial?.diffuse.wrapS = .repeat
            floor.geometry?.firstMaterial?.diffuse.wrapT = .repeat
            arView.scene.rootNode.addChildNode(floor)

            for x in 0...1 {
                for z in 0...1 {
                    let box = SCNNode(geometry: SCNBox(width: 1, height: 1, length: 1, chamferRadius: 0))
                    box.position = SCNVector3(-5.0 + Double(x) * 10.0, -0.5, -5.0 + Double(z) * 10.0)
                    box.rotation = SCNVector4(0,1,0, Float.random(in: 0...3.14))
                    for i in 0...5 {
                        let mat = SCNMaterial()
                        mat.diffuse.contents = UIImage(named: "UnrealLogo")
                        if i == 0 {
                            box.geometry?.firstMaterial = mat
                        } else {
                            box.geometry?.materials.append(mat)
                        }
                    }
                    arView.scene.rootNode.addChildNode(box)
                }
            }

        } else {
            arView.scene.background.contents = UIColor.black
        }
        
         */
        
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
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        UIApplication.shared.isIdleTimerDisabled = false
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
            disconnect()
            self.presentingViewController?.dismiss(animated: true, completion: nil)
        }
    }
    
    func reconnect() {
        self.streamingConnection?.reconnect()
    }
    
    func disconnect() {
        self.streamingConnection?.disconnect()
        
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
    
    func sendThumbstickUpdate(key : String, controller : Int, oldValue : Float?, newValue : Float) {

        assertionFailure()
        /*
        guard let osc = oscConnection else { return }

        if (newValue != 0.0) || ((oldValue ?? 0.0) != newValue) {
            osc.send(.controllerAnalog, arguments: [ OSCArgument.blob(OSCUtility.ueControllerAnalogData(key: key, controller: controller, value: newValue)) ])
        }
         */
    }
    
    func sendControllerUpdate() {
        
        guard let gc = gameController else { return }
        assertionFailure()
        
        /*
        guard let _ = oscConnection else { return }

        let snapshot = gc.capture()
        guard let gp = snapshot.extendedGamepad else { return }

        let controllerIndex = gc.playerIndex.rawValue
        
        sendThumbstickUpdate(key: "Gamepad_LeftX", controller: controllerIndex, oldValue : self.gameControllerSnapshot?.extendedGamepad?.leftThumbstick.xAxis.value, newValue :gp.leftThumbstick.xAxis.value)
        sendThumbstickUpdate(key: "Gamepad_LeftY", controller: controllerIndex, oldValue : self.gameControllerSnapshot?.extendedGamepad?.leftThumbstick.yAxis.value, newValue :gp.leftThumbstick.yAxis.value)
        sendThumbstickUpdate(key: "Gamepad_RightX", controller: controllerIndex, oldValue : self.gameControllerSnapshot?.extendedGamepad?.rightThumbstick.xAxis.value, newValue :gp.rightThumbstick.xAxis.value)
        sendThumbstickUpdate(key: "Gamepad_RightY", controller: controllerIndex, oldValue : self.gameControllerSnapshot?.extendedGamepad?.rightThumbstick.yAxis.value, newValue :gp.rightThumbstick.yAxis.value)
        
        self.gameControllerSnapshot = snapshot
         */
    }

    func directionalPadValueChanged(directionalPad : GCControllerDirectionPad, xValue : Float, yValue : Float) {
        assertionFailure()
        /*
        guard let osc = oscConnection else { return }
        guard let gc = gameController else { return }
        guard let gp = gc.extendedGamepad else { return }

        let controllerIndex = gc.playerIndex.rawValue
        
        switch directionalPad {
        case gp.leftThumbstick:
            osc.send(.controllerAnalog, arguments: [ OSCArgument.blob(OSCUtility.ueControllerAnalogData(key: "Gamepad_LeftX", controller: controllerIndex, value: xValue)) ])
            osc.send(.controllerAnalog, arguments: [ OSCArgument.blob(OSCUtility.ueControllerAnalogData(key: "Gamepad_LeftY", controller: controllerIndex, value: yValue)) ])
        case gp.rightThumbstick:
            osc.send(.controllerAnalog, arguments: [ OSCArgument.blob(OSCUtility.ueControllerAnalogData(key: "Gamepad_RightX", controller: controllerIndex, value: xValue)) ])
            osc.send(.controllerAnalog, arguments: [ OSCArgument.blob(OSCUtility.ueControllerAnalogData(key: "Gamepad_RightY", controller: controllerIndex, value: yValue)) ])
        default:
            Log.warning("directionalPadValueChanged() encounted unsupported '\(directionalPad.localizedName ?? "UNK")'")
            break
        }
         */
    }
    
    func buttonValueChanged(button : GCControllerButtonInput, value : Float, pressed : Bool) {

        assertionFailure()
        /*
        guard let osc = oscConnection else { return }
        guard let gc = gameController else { return }
        guard let gp = gc.extendedGamepad else { return }

        let address = button.isPressed ? OSCAddressPattern.controllerButtonPressed : OSCAddressPattern.controllerButtonReleased
        let controllerIndex = gc.playerIndex.rawValue
        let isRepeat = false

        switch button {
        case gp.leftThumbstickButton:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_LeftThumbstick", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.rightThumbstickButton:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_RightThumbstick", controller: controllerIndex, isRepeat: isRepeat)) ])

        case gp.buttonA:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_FaceButton_Bottom", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.buttonB:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_FaceButton_Right", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.buttonX:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_FaceButton_Left", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.buttonY:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_FaceButton_Top", controller: controllerIndex, isRepeat: isRepeat)) ])

        case gp.leftShoulder:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_LeftShoulder", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.rightShoulder:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_RightShoulder", controller: controllerIndex, isRepeat: isRepeat)) ])

        case gp.leftTrigger:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_LeftTrigger", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.rightTrigger:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_RightTrigger", controller: controllerIndex, isRepeat: isRepeat)) ])

        case gp.dpad.up:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_DPad_Up", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.dpad.down:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_DPad_Down", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.dpad.left:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_DPad_Left", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.dpad.right:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_DPad_Right", controller: controllerIndex, isRepeat: isRepeat)) ])
            
        case gp.buttonOptions:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_Special_Left", controller: controllerIndex, isRepeat: isRepeat)) ])
        case gp.buttonMenu:
            osc.send(address, arguments: [ OSCArgument.blob(OSCUtility.ueControllerButtonData(key: "Gamepad_Special_Right", controller: controllerIndex, isRepeat: isRepeat)) ])

        default:
            Log.warning("buttonValueChanged() encounted unsupported '\(button.localizedName ?? "UNK")'")
        }
         */
    }
}

extension VideoViewController : HeaderViewDelegate {
    
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


