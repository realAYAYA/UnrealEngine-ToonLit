//
//  SingleValueViewController.swift
//  Live Link VCAM
//
//  Created by Brian Smith on 12/31/19.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

//
// The SingleValueViewController allows either the entry of a single value
// OR the editing of a value with an optional Delete button.
//

import UIKit

class SingleValueViewController : UITableViewController {

    enum Mode {
        case create
        case edit
        case editWithDelete
    }

    enum ActionType {
        case done
        case cancel
        case delete
    }

    enum AllowedType {
        case any
        case ipAddress
        case integer
        case unreal // alphanumeric, _, -
    }


    @IBOutlet weak var textField: UITextField!
    var doneButton: UIBarButtonItem?
    var cancelButton: UIBarButtonItem?

    var mode = Mode.create
    var allowedType = AllowedType.any
    var initialValue = String()
    var placeholderValue : String?
    var deleteLabelText : String?
    var finished : ((ActionType, String?) -> Void)? = nil
    
    private var isRoot = false
    public private(set) var finishedAction = ActionType.cancel

    override func viewDidLoad() {
        super.viewDidLoad()

        self.doneButton = UIBarButtonItem(title: mode == .create ? Localized.buttonAdd() : Localized.buttonDone(), style: .done, target: self, action: #selector(action))

        // if this view is the root view of a nagivation controller, then we add a "cancel" and "done" button.
        if self == self.navigationController?.viewControllers.first {
            isRoot = true
            self.cancelButton = UIBarButtonItem(title: Localized.buttonCancel(), style: .plain, target: self, action: #selector(action))
            
            self.navigationItem.leftBarButtonItem = self.cancelButton
            self.navigationItem.rightBarButtonItem = self.doneButton
        } else {
            
            if UIDevice.current.userInterfaceIdiom == .phone {
                self.navigationItem.rightBarButtonItem = self.doneButton
            }

            // else the finishedAction is set to done always : this means that any dismissal (probably via
            // a back gesture) of the VC is essentially a done/confirm
            finishedAction = .done
        }

        self.textField.text = initialValue
        self.textField.placeholder = self.placeholderValue
        self.textField.delegate = self
        
        switch allowedType {
        case .integer:
            self.textField.keyboardType = .numberPad
        case .ipAddress:
            self.textField.keyboardType = .numbersAndPunctuation
        default:
            break
        }

    }
    
    override func viewWillAppear(_ animated: Bool) {
         super.viewWillAppear(animated)
        
         if #available(iOS 13.0, *) {
              navigationController?.navigationBar.setNeedsLayout()
         }
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        
        self.textField.becomeFirstResponder()
    }
     
    override func viewWillDisappear(_ animated: Bool) {

        if let callback = finished {
            callback(finishedAction, self.textField.text)
        }
        
        super.viewWillDisappear(animated)
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        if indexPath.section == 1 {
            self.textField.text = nil
            self.finishedAction = .delete
            
            if isRoot {
                self.dismiss(animated: true, completion: nil)
            } else {
                self.navigationController?.popViewController(animated: true)
            }
        }
    }
    
    override func tableView(_ tableView: UITableView, willDisplay cell: UITableViewCell, forRowAt indexPath: IndexPath) {
        
        // override the "delete" label if there was one specified
        if indexPath.section == 1 {
            if let text = deleteLabelText {
                cell.textLabel?.text = text
            }
        }
    }
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        if mode != .editWithDelete {
            return 1
        } else {
            return super.numberOfSections(in: tableView)
        }
    }
    
    @IBAction func action(sender : UIBarButtonItem) {
        
        if sender == self.doneButton {
            self.finishedAction = .done
        }
        
        // this action can only occur on a modal dialog
        self.dismiss(animated: true, completion: nil)
    }
    
    func handleTextField(_ textField: UITextField, shouldChangeCharactersIn range: NSRange, replacementString string: String) -> Bool {
        
        if textField == self.textField {
            
            switch allowedType {
            case .integer:
                let allowedCharacters = CharacterSet(charactersIn: "0123456789")
                return allowedCharacters.isSuperset(of: CharacterSet(charactersIn: string))
            case .ipAddress:
                let allowedCharacters = CharacterSet(charactersIn: "0123456789.")
                return allowedCharacters.isSuperset(of: CharacterSet(charactersIn: string))
            case .unreal:
                let allowedCharacters = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: "-_"))
                return allowedCharacters.isSuperset(of: CharacterSet(charactersIn: string))
            default:
                return true
            }
        }

        return true
    }

}

extension SingleValueViewController : UITextFieldDelegate {
    
    func textField(_ textField: UITextField, shouldChangeCharactersIn range: NSRange, replacementString string: String) -> Bool {
        return handleTextField(textField, shouldChangeCharactersIn: range, replacementString: string)
    }
}
