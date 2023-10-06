//
//  TentacleDetailViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/23/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit
import Tentacle

func rssiToString(_ value : Int) -> String {
    
    if value >= -60 {
        return NSLocalizedString("signal-excellent", value: "Excellent", comment: "Signifies a excellent connection quality. This is the highest quality.")
    } else if value >= -70 {
        return NSLocalizedString("signal-good", value: "Good", comment: "Signifies a good connection quality -- This is lower than excellent, but higher than fair.")
    } else if value >= -90 {
        return NSLocalizedString("signal-fair", value: "Fair", comment: "Signifies a fair connection quality -- This is lower than good, but higher than poor.")
    } else {
        return NSLocalizedString("signal-poor", value: "Poor", comment: "Signifies a poor connection quality -- This is lowest quality.")
    }
}

class TentacleDetailViewController : UIViewController {
    
    var device : TentacleDevice!

    var isActive = false
    
    @IBOutlet weak var iconImageView : UIImageView!
    @IBOutlet weak var uuidLabel : UILabel!
    @IBOutlet weak var tableView : UITableView!

    private var tableViewTimer: Timer!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        if UIDevice.current.userInterfaceIdiom == .phone {
            self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
        }

        isActive = Tentacle.devicesIdentical(self.device, Tentacle.shared?.activeDevice)
        
        self.navigationItem.title = Tentacle.nameString(advertisement: &self.device.advertisement)
        iconImageView.image = TentacleIcon.deviceIcon(with: UInt(self.device.advertisement.icon), productId: self.device.advertisement.productId)
        uuidLabel.text = Tentacle.advertisementIdString(advertisement: self.device.advertisement)
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // 5-second timer updates the various other details from the device
        self.tableViewTimer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true, block: { _ in
            
            var paths = [IndexPath]()
            for i in 0..<self.tableView(self.tableView, numberOfRowsInSection: 2) {
                paths.append(IndexPath(row:i, section:2))
            }
            
            self.tableView.reloadRows(at: paths, with: .none)
        })
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        tableViewTimer.invalidate()
    }
    
    @objc func displayLinkDidFire(_ displayLink: CADisplayLink) {

        self.tableView.reloadRows(at: [ IndexPath(row: 0, section: 1)], with: .none)
    }
    
    @objc func done(sender:Any?) {
        self.navigationController?.dismiss(animated: true, completion: nil)
    }
}

extension TentacleDetailViewController : UITableViewDataSource {
    
    func numberOfSections(in tableView: UITableView) -> Int {
        
        return 3
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        
        switch section {
        case 0:
            return 1
        case 1:
            return 1
        case 2:
            return 5
        default:
            return 0
        }
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        
        if indexPath.section == 0 {

            let cell = tableView.dequeueReusableCell(withIdentifier: "forget")!

            cell.textLabel?.text = isActive ? NSLocalizedString("tentacle-disconnect", value: "Disconnect", comment: "A button to disconnect from a tentacle sync device") : NSLocalizedString("tentacle-connect", value: "Connect", comment: "A button to connect to a tentacle sync device")
            cell.textLabel?.tintColor = UIColor.systemBlue
            cell.detailTextLabel?.text = nil

            return cell

        } else {

            if indexPath.section == 1 {
                
                let cell = tableView.dequeueReusableCell(withIdentifier: "timecode")!
                cell.detailTextLabel?.text = " "
                
                return cell

            } else {

                let cell = tableView.dequeueReusableCell(withIdentifier: "detail")!

                let lastSeen = TentacleBluetoothController.timestamp() - device.advertisement.timecode.receivedTimestamp

                switch indexPath.row {
                case 0:
                    cell.textLabel?.text = NSLocalizedString("tentacle-framerate", value: "Framerate", comment: "The framerate for the tentacle device.")
                    cell.detailTextLabel?.text = "\(TentacleAdvertisementGetFrameRate(&device.advertisement))"
                    
                case 1:
                    cell.textLabel?.text = NSLocalizedString("tentacle-mode", value: "Mode", comment: "The operating mode for the tentacle device.")
                    cell.detailTextLabel?.text = device.advertisement.greenMode ?
                        NSLocalizedString("tentacle-mode-green", value: "Green (Generate)", comment: "The operating mode for the tentacle device."):
                        (device.advertisement.linkMode ? NSLocalizedString("tentacle-mode-red", value: "Red (Link)", comment: "The operating mode for the tentacle device.") : Localized.unknown())
                    
                case 2:
                    cell.textLabel?.text = NSLocalizedString("tentacle-lastseen", value: "Last Seen", comment: "When the tentacle device last sent a message.")
                    cell.detailTextLabel?.text = (lastSeen < 1.0) ? NSLocalizedString("tentacle-lastseen-justnow", value: "Just Now", comment: "The device was seen just a moment ago.") : RelativeDateTimeFormatter().localizedString(fromTimeInterval: -lastSeen)
                case 3:
                    cell.textLabel?.text = NSLocalizedString("tentacle-battery", value: "Battery", comment: "Battery level of the tentacle device.")
                    cell.detailTextLabel?.text = (lastSeen < 10.0) ? String(format: "%d%%", device.advertisement.battery) : Localized.unknown()
                    
                case 4:
                    cell.textLabel?.text = NSLocalizedString("tentacle-signal-strength", value: "Signal Strength", comment: "Signal strength indicator of the tentacle device.")
                    let rssi = TentacleDeviceGetRssi(&device)
                    cell.detailTextLabel?.text = (lastSeen < 10.0) ? rssiToString(Int(rssi)) + String(format: " (%d dBm)", rssi) : Localized.unknown()
                    
                default:
                    break
                }
                
                return cell
            }
        }
    }
    
    
}

extension TentacleDetailViewController : UITableViewDelegate {
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {

            if isActive {
                Tentacle.shared?.forgetActiveDevice()
                isActive = false
            } else {
                Tentacle.shared?.activeDevice = self.device
                isActive = true
            }
            
            tableView.reloadRows(at: [ IndexPath(row: 0, section: 0)], with: .automatic)
        }
    }

    func tableView(_ tableView: UITableView, willDisplay cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        if let tsc = cell as? TimecodeSourceCell {
            tsc.displayTimecodeLive = indexPath.section == 1
        }
    }
    
    func tableView(_ tableView: UITableView, didEndDisplaying cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        
        if let tsc = cell as? TimecodeSourceCell {
            tsc.displayTimecodeLive = false
        }

    }
}

