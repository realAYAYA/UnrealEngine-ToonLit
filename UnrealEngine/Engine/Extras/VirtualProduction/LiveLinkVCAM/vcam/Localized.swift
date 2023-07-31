//
//  Localized.swift
//  FaceLink
//
//  Created by Brian Smith on 5/11/20.
//  Copyright Epic Games, Inc. All Rights Reserved.
//

import Foundation


class Localized {

    class func buttonOK() -> String {
        NSLocalizedString("okbutton", value: "OK", comment: "An OK button on an alert")
    }
    class func buttonCancel() -> String {
        NSLocalizedString("button-cancel", value: "Cancel", comment: "A cancel button on an alert")
    }
    class func buttonDone() -> String {
        NSLocalizedString("button-done", value: "Done", comment: "A done button on an edit item screen.")
    }
    class func buttonAdd() -> String {
        NSLocalizedString("button-add", value: "Add", comment: "An Add button on an edit item screen.");
    }
    class func buttonBack() -> String {
        NSLocalizedString("button-back", value: "Back", comment: "A Back button to go back to the previous screen");
    }
    class func buttonSelect() -> String {
        NSLocalizedString("button-select", value: "Select", comment: "A button to select items for sharing or delete.");
    }
    class func buttonSelectAll() -> String {
        NSLocalizedString("button-selectall", value: "Select All", comment: "A button to select all items in a list.");
    }
    class func buttonDeselectAll() -> String {
        NSLocalizedString("button-deselectall", value: "Deselect All", comment: "A button to deselect all items in a list.");
    }
    class func buttonDelete() -> String {
        NSLocalizedString("button-delete", value: "Delete", comment: "A button to delete items.")
    }
    class func titleError() -> String {
        NSLocalizedString("title-error", value: "Error", comment: "An alert title for an error message.")
    }
    class func optionEnabled() -> String {
        NSLocalizedString("option-enabled", value:"Enabled", comment: "An option was enabled.")
    }
    class func optionDisabled() -> String {
        NSLocalizedString("option-disabled", value:"Disabled", comment: "An option was disabled.")
    }
    class func fadeOut() -> String {
        NSLocalizedString("settings-fadeout", value: "Fade Out", comment: "Option for allowing the interface to fade out (to transparent/invisible)")
    }
    class func alwaysOn() -> String {
        NSLocalizedString("settings-alwayson", value: "Always On", comment: "Option for setting the interface to always be visible")
    }
    class func filename() -> String {
        NSLocalizedString("settings-filename", value: "Filename", comment: "Option for displaying a string as a filename.")
    }
    class func unknown() -> String {
        NSLocalizedString("value-unknown", value: "Unknown", comment: "An unknown or unsupported value was found")
    }
    class func default_() -> String {
        NSLocalizedString("value-default", value: "Default", comment: "A label that indicates a default value")
    }
    class func none() -> String {
        NSLocalizedString("value-none", value: "None", comment: "An label that indicates no value is set or added -- an empty set.")
    }
    class func subjectName() -> String {
        NSLocalizedString("livelink-subjectname", value: "Subject Name", comment: "The name of the subject (a person, device, character) of the capture")
    }
}
