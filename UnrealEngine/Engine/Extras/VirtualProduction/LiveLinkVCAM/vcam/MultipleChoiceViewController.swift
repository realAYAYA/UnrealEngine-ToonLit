//
//  MultipleChoiceViewController.swift
//  Live Link Face
//
//  Created by Brian Smith on 1/16/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import UIKit

class MultipleChoiceViewController : UITableViewController {
    
    var items : [String]!
    var selectedIndex : Int!
    
    var footerString : String?

    var completion : ((Int) -> Void)? = nil
    var selectionChanged : ((Int) -> Void)? = nil

    override func viewWillDisappear(_ animated: Bool) {

        if let callback = completion {
            callback(selectedIndex)
        }
        
        super.viewWillDisappear(animated)
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return items.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        
        var cell : UITableViewCell!
        
        cell = tableView.dequeueReusableCell(withIdentifier: "choiceCell")
        if cell == nil {
            cell = UITableViewCell(style: .default, reuseIdentifier: "choiceCell")
        }
        
        cell.textLabel?.text = self.items[indexPath.row]
        cell.accessoryType = (indexPath.row == selectedIndex) ? .checkmark : .none

        return cell
        
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        self.selectedIndex = indexPath.row
        self.tableView.reloadData()
        if let callback = selectionChanged {
            callback(indexPath.row)
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
        return footerString
    }
}
