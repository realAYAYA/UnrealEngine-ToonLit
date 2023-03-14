//
//  TimecodeViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 1/21/20.
//  Copyright © 2020 Epic Games, Inc. All rights reserved.
//

import UIKit
import Tentacle
import Kronos

class TimecodeSourceCell : UITableViewCell {

    var displayTimecodeLive = false {
        didSet {
            if displayTimecodeLive {
                
                if displayLink == nil {
                    displayLink = CADisplayLink(
                      target: self, selector: #selector(displayLinkDidFire)
                    )
                    displayLink!.add(to: .main, forMode: .common)
                }
                
            } else {
                displayLink?.invalidate()
                displayLink = nil

            }
        }
    }
    
    private var displayLink: CADisplayLink?

    deinit {
        displayLink?.invalidate()
        displayLink = nil
    }
    
    @objc func displayLinkDidFire(_ displayLink: CADisplayLink) {

        // refreshthe timecode
        let timecode = Timecode.create()
        
        let timecodeString = timecode.toString(includeFractional: true)
        detailTextLabel?.text = timecodeString
        detailTextLabel?.textColor = timecode.current ? UIColor.secondaryLabel : UIColor.systemRed
    }

}

class TentaclePeripheralTableViewCell : UITableViewCell {
    
    @IBOutlet weak var nameLabel : UILabel!
    @IBOutlet weak var checkImageView : UIImageView!
    @IBOutlet weak var batteryImageView : UIImageView!
    @IBOutlet weak var signalImageView : UIImageView!

}

class TimecodeViewController : UITableViewController {
    
    private let appSettings = AppSettings.shared
    
    private var tentacleDevices = [TentacleDevice]()
    
    private var peripheralsTimer: Timer!
    private var timecode : Timecode?
    
    private var icons : [TimecodeSource:UIImage?] = [:]
    
    
    required init?(coder: NSCoder) {
        super.init(coder:coder)
        
        // create all the icons at the right size (for alignment)
        for source in [ TimecodeSource.systemTime, TimecodeSource.ntp, TimecodeSource.tentacleSync ] {
            icons[source] = UIImage(timecodeSource: source)?.resizeBackgroundCanvas(newSize: CGSize(width: 20, height: 20))?.withRenderingMode(.alwaysTemplate)
        }
        
        if let tentacle = Tentacle.shared {
            self.tentacleDevices = tentacle.availableDevices
            tentacle.delegate = self
        }
        
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        if UIDevice.current.userInterfaceIdiom == .phone {
            self.navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(done))
        }
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // set up a display link to the one row that displays the current timecode from the correct source
        
        self.peripheralsTimer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true, block: { _ in
            if !self.tentacleDevices.isEmpty {
                
                var paths = [IndexPath]()
                for i in 1...self.tentacleDevices.count {
                    paths.append(IndexPath(row:i, section:0))
                }
                
                self.tableView.reloadRows(at: paths, with: .none)
            }
        })
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        peripheralsTimer.invalidate()
    }
    
    @objc func done(sender:Any?) {
        self.navigationController?.dismiss(animated: true, completion: nil)
    }


    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        
        if indexPath.section == 0 {
            
            // section 0 -- current source :
            // * NTP allows changing the NTP server
            // * Tentacle lists all available devices and allows selection of the active
            //   device
            
            switch appSettings.timecodeSourceEnum() {
            case .ntp:
                if indexPath.row == 1 {
                    self.performSegue(withIdentifier: "ntpPool", sender: nil)
                }
            case .tentacleSync:
                if indexPath.row > 0 {
                    Tentacle.shared?.activeDevice = self.tentacleDevices[indexPath.row - 1]
                    self.tableView.reloadSections([ 0 ], with: .automatic)
                }
            default:
                break
            }
            
        } else if indexPath.section == 1 {

            // section 1 -- available sources :
            // * respond to the selected type by stopping all other sources and setting
            //   the new current source enum
            if let cell = tableView.cellForRow(at: indexPath) {

                if let source = TimecodeSource(rawValue: cell.tag) {

                    appSettings.setTimecodeSourceEnum(source)

                    if source == .tentacleSync {
                        if let tentacle = Tentacle.shared {
                            self.tentacleDevices = tentacle.availableDevices
                            tentacle.delegate = self
                        }
                    } else {
                        self.tentacleDevices.removeAll()
                    }

                    self.tableView.reloadData()
                }
            }
        }
    }
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        return 2
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
            
        if section == 0 {
            if Timecode.sourceIsSystemTime() {
                return 1
            } else if Timecode.sourceIsNTP() {
                return 2
            } else if Timecode.sourceIsTentacle() {
                return 1 +  self.tentacleDevices.count
            } else {
                return 0
            }
        } else if section == 1 {
            return 2
        }
        
        return 0
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        
        var cell : UITableViewCell!

        let timecodeSource = appSettings.timecodeSourceEnum()
        
        if indexPath.section == 0 {
            
            // current source
            
            // first row just displays the current source : the name (system,ntp,tentacle) and
            // the actual timecode that is assocated with it (updated w/ display link, above)
            if indexPath.row == 0 {
            
                cell = tableView.dequeueReusableCell(withIdentifier: "detail")!
                cell.textLabel?.text = Timecode.sourceToString(timecodeSource)
                cell.detailTextLabel?.text = " "
                cell.imageView?.image = icons[timecodeSource] ?? nil

            } else {
                
                //
                // extra rows in section 1 are the options for the current source : a pool
                // for NTP, and the list of available devices for tentacle
                //
                
                if timecodeSource == .ntp {
                    
                    cell = tableView.dequeueReusableCell(withIdentifier: "ntpPool")
                    cell.textLabel?.text = "Pool"
                    cell.detailTextLabel?.text = appSettings.ntpPool.isEmpty ? "time.apple.com" : appSettings.ntpPool
                    
                } else if timecodeSource == .tentacleSync {
                    
                    var device = self.tentacleDevices[indexPath.row - 1]
                    let isSelected = Tentacle.devicesIdentical(Tentacle.shared?.activeDevice, device)
                    let isCurrent = (TentacleBluetoothController.timestamp() - device.advertisement.timecode.receivedTimestamp) < 10.0;

                    let tentacleCell = tableView.dequeueReusableCell(withIdentifier: "tentaclePeripheral") as! TentaclePeripheralTableViewCell
                    tentacleCell.nameLabel.text = Tentacle.nameString(advertisement: &device.advertisement)
                    tentacleCell.checkImageView.isHidden = !isSelected
                    tentacleCell.batteryImageView.image = isCurrent ? TentacleIcon.batteryIcon(forPercent: UInt(device.advertisement.battery)) : nil
                    tentacleCell.signalImageView.image = isCurrent ? TentacleIcon.signalStrengthIcon(forRssiValue: Int(device.advertisement.rssi)) : TentacleIcon.signalStrengthIcon(with: 0)

                    cell = tentacleCell
                }
            }
        } else if indexPath.section == 1 {
            
            // build up the list of which sources we need to show here.
            
            var sources = [TimecodeSource]()

            if timecodeSource != .systemTime {
                sources.append(.systemTime)
            }
            if timecodeSource != .ntp {
                sources.append(.ntp)
            }
            if timecodeSource != .tentacleSync {
                sources.append(.tentacleSync)
            }

            let source = sources[indexPath.row]
            
            cell = tableView.dequeueReusableCell(withIdentifier: "detail")!
            cell.textLabel?.text = Timecode.sourceToString(source)
            cell.imageView?.image = icons[source] ?? nil
            cell.tag = source.rawValue
            
            switch source {
            case .ntp:
                cell.detailTextLabel?.text = appSettings.ntpPool.isEmpty ? "time.apple.com" : appSettings.ntpPool
            case .tentacleSync:
                cell.detailTextLabel?.text = appSettings.tentaclePeripheralName
            default:
                cell.detailTextLabel?.text = ""
            }
            
        }
        
        return cell

    }
    
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0:
            return NSLocalizedString("timecode-section-current", value: "Current Source", comment: "A section title that shows the currently selected source..")
        case 1:
            return NSLocalizedString("timecode-section-other", value: "Other Sources", comment: "A section title that lists other options for timecode source.")
        default:
            return nil
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return NSLocalizedString("timecode-current-footer", value: "Live Link VCAM can use the iPhone's system clock for timecode, connect to an NTP Server, or receive timecode from a Tentacle Sync via Bluetooth.", comment: "A section footer explaining the timecode sources.")
        default:
            return nil
        }
    }
    
    override func tableView(_ tableView: UITableView, willDisplay cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        if let tsc = cell as? TimecodeSourceCell {
            tsc.displayTimecodeLive = (indexPath.section == 0) && (indexPath.row == 0)
        }
    }
    
    override func tableView(_ tableView: UITableView, didEndDisplaying cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        
        if let tsc = cell as? TimecodeSourceCell {
            tsc.displayTimecodeLive = false
        }

    }
    
    func removeAllTentaclePeripherals() {
        if !self.tentacleDevices.isEmpty {
            
            let count = self.tentacleDevices.count
            self.tentacleDevices.removeAll()
            
            var paths = [IndexPath]()
            for i in 1...count {
                paths.append(IndexPath(row:i, section:0))
            }
            
            self.tableView.deleteRows(at: paths, with: .automatic)
        }
    }
    

    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        
        if segue.identifier == "tentacleDetail" {

            if let vc = segue.destination as? TentacleDetailViewController,
                let cell = sender as? UITableViewCell {

                if let rowIndex = self.tableView.indexPath(for: cell)?.row {
                    vc.device = self.tentacleDevices[rowIndex - 1]
                }
            }
        } else if segue.identifier == "ntpPool" {
            
            if let vc = segue.destination as? SingleValueViewController {

                vc.navigationItem.title = NSLocalizedString("timecode-title-ntp-pool", value: "NTP Pool", comment: "A screen to edit the NTP pool -- an address for Network Time Protocol data.")
                vc.mode = .edit
                vc.allowedType = .any
                vc.initialValue = appSettings.ntpPool
                vc.placeholderValue = "time.apple.com"
                vc.finished = { (action, value) in
                    
                    if action == .done {
                        
                        AppSettings.shared.ntpPool = value!.trimmed()
                        self.tableView.reloadRows(at: [ IndexPath(row: 1, section: 0)], with: .automatic)
                        
                        AppSettings.shared.setTimecodeSourceEnum(.none)
                        AppSettings.shared.setTimecodeSourceEnum(.ntp)
                    }
                }
            }
        }
    }
}

extension TimecodeViewController : TentacleDelegate {

    func tentacleDidUpdateAvailableDevices(_ tentacle: Tentacle) {

        if appSettings.timecodeSourceEnum() == .tentacleSync {
            self.tentacleDevices = tentacle.availableDevices
            self.tableView.reloadSections([0], with: .automatic);
        }
    }

    func tentacle(_ tentacle: Tentacle, didSetActiveDevice device: TentacleDevice?) {
        
        self.tableView.reloadSections([0], with: .automatic);
    }
    
    func tentacle(_ tentacle: Tentacle, didUpdateDevice device: inout TentacleDevice) {

        if appSettings.timecodeSourceEnum() == .tentacleSync {
            
            for i in 0..<self.tentacleDevices.count {

                var d = self.tentacleDevices[i]

                if Tentacle.devicesIdentical(d, device) {

                    if d.advertisement.battery != device.advertisement.battery ||
                        TentacleIcon.signalStrengthIcon(forRssiValue:Int(d.advertisement.rssi)).hashValue != TentacleIcon.signalStrengthIcon(forRssiValue: Int(device.advertisement.rssi)).hashValue ||
                        Tentacle.nameString(advertisement: &d.advertisement) != Tentacle.nameString(advertisement: &device.advertisement) {
                        self.tentacleDevices[i] = device
                        self.tableView.reloadRows(at: [ IndexPath(row: i + 1, section: 0) ], with: .none)
                    }

                    break
                }
            }
        }
    }
}
