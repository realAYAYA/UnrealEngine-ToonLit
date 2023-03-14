# Copyright Epic Games, Inc. All Rights Reserved.

import collections
import fnmatch
import json
import os
import pathlib
import shutil
import socket
import sys
import typing
from typing import Type

from PySide2 import QtCore
from PySide2 import QtGui
from PySide2 import QtWidgets

from switchboard import switchboard_widgets as sb_widgets
from switchboard.switchboard_logging import LOGGER
from switchboard.switchboard_widgets import (DropDownMenuComboBox,
    NonScrollableComboBox)

ROOT_CONFIGS_PATH = pathlib.Path(__file__).parent.with_name('configs')
CONFIG_SUFFIX = '.json'

USER_SETTINGS_FILE_NAME = 'user_settings.json'
USER_SETTINGS_FILE_PATH = ROOT_CONFIGS_PATH.joinpath(USER_SETTINGS_FILE_NAME)
USER_SETTINGS_BACKUP_FILE_NAME = 'corrupted_user_settings_backup.json'
USER_SETTINGS_BACKUP_FILE_PATH = ROOT_CONFIGS_PATH.joinpath(USER_SETTINGS_BACKUP_FILE_NAME)

DEFAULT_MAP_TEXT = '-- Default Map --'

def migrate_comma_separated_string_to_list(value) -> typing.List[str]:
    if isinstance(value, str):
        return value.split(",")
    # Technically we should check whether every element is a string but we skip it here
    if isinstance(value, list):
        return value
    raise NotImplementedError("Migration not handled")

class Setting(QtCore.QObject):
    '''
    A type-agnostic value container for a configuration setting.

    This base class can be used directly for Settings that will
    never appear in the UI. Otherwise, Settings that can be modified
    in the UI must be represented by a derived class that creates
    the appropriate widget(s) for modifying the Setting's value.
    '''
    signal_setting_changed = QtCore.Signal(object, object)
    signal_setting_overridden = QtCore.Signal(str, object, object)

    def _filter_value(self, value):
        '''
        Filter function to modify the incoming value before updating or
        overriding the setting.

        The base class implementation does not apply any filtering to values.
        '''
        return value

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value,
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None
    ):
        '''
        Create a new Setting object.

        Args:
            attr_name: Internal name.
            nice_name: Display name.
            value    : The initial value of this Setting.
            tool_tip : Tooltip to show in the UI for this Setting.
            show_ui  : Whether to show this Setting in the Settings UI.
        '''
        super().__init__()

        self.attr_name = attr_name
        self.nice_name = nice_name

        value = self._filter_value(value)
        self._original_value = self._value = value

        # todo-dara: overrides are identified by device name right now. This
        # should be changed to the hash instead. That way we could avoid
        # having to patch the overrides and settings in CONFIG when a device
        # is renamed.
        self._overrides = {}

        # These members store the UI widgets for the "base" Setting as well as
        # any overrides of the setting, similar to the way we store the base
        # value and overrides of the value. They identify the widget in the UI
        # that should be highlighted when the Setting is overridden. Derived
        # classes should call set_widget() with an override device name if
        # appropriate in their implementations of _create_widgets() if they
        # want override highlighting.
        self._base_widget = None
        self._override_widgets = {}
        self._on_setting_changed_lambdas = {}
        
        # Appears when override value is different from _value
        self._allow_reset = allow_reset
        self._base_reset_widget = None
        self._reset_override_widgets = {}

        self._migrate_data = migrate_data

        self.tool_tip = tool_tip
        self.show_ui = show_ui

    def is_overridden(self, device_name: str) -> bool:
        try:
            return self._overrides[device_name] != self._value
        except KeyError:
            return False

    def remove_override(self, device_name: str):
        self._overrides.pop(device_name, None)
        self._override_widgets.pop(device_name, None)

    def update_value(self, new_value):
        new_value = self._filter_value(new_value)

        if self._value == new_value:
            return

        old_value = self._value
        self._value = new_value

        self.signal_setting_changed.emit(old_value, self._value)
        self._refresh_reset_base_widget()
    
    def override_value(self, device_name: str, override):
        override = self._filter_value(override)

        if (device_name in self._overrides and
                self._overrides[device_name] == override):
            return

        self._overrides[device_name] = override
        self.signal_setting_overridden.emit(device_name, self._value, override)

        self._refresh_reset_override_widget(device_name)

    def get_value(self, device_name: typing.Optional[str] = None):
        def get_value(self, device_name: typing.Optional[str] = None):
            try:
                return self._overrides[device_name]
            except KeyError:
                return self._value
            
        value = get_value(self, device_name)
        return self._migrate_data(value) if self._migrate_data is not None else value
        

    def on_device_name_changed(self, old_name: str, new_name: str):
        if old_name in self._overrides.keys():
            self._overrides[new_name] = self._overrides.pop(old_name)
            self._override_widgets[new_name] = (
                self._override_widgets.pop(old_name))

    def reset(self):
        self._value = self._original_value
        self._overrides = {}
        self._override_widgets = {}

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> typing.Union[QtWidgets.QWidget, QtWidgets.QLayout]:
        '''
        Create the widgets necessary to manipulate this Setting in the UI.

        Settings that can appear in the UI must provide their own
        implementation of this function. If override highlighting is desired,
        the implementation should also set the override widget member variable.

        This function should return the "top-level" widget or layout. In
        some cases such as the BoolSetting this will just be a QCheckBox,
        whereas in others like the FilePathSetting, this will be a QHBoxLayout
        that contains line edit and button widgets.
        '''
        raise NotImplementedError(
            f'No UI for Setting "{self.nice_name}". '
            'Settings that are intended to display in the UI must '
            'derive from the Setting class and override _create_widgets().')

    def _on_widget_value_changed(
            self, new_value,
            override_device_name: typing.Optional[str] = None):
        '''
        Update this Setting in response to a change in value caused by UI
        manipulation.

        The value is applied as an override if appropriate, in which case if
        an override widget has been identified, it will be highlighted.

        It should not be necessary to override this function in derived
        classes.
        '''
        if override_device_name is None:
            self.update_value(new_value)
            self._refresh_reset_override_widgets()
            return

        old_value = self.get_value(override_device_name)
        if new_value != old_value:
            self.override_value(override_device_name, new_value)

        widget = self.get_widget(override_device_name=override_device_name)
        if self.is_overridden(override_device_name):
            if widget:
                sb_widgets.set_qt_property(widget, "override", True)
        else:
            if widget:
                sb_widgets.set_qt_property(widget, "override", False)

    def _on_setting_changed(
            self, new_value,
            override_device_name: typing.Optional[str] = None):
        '''
        Callback invoked when the value of this Setting changes.

        This can be implemented in derived classes to update the appropriate
        UI elements in response to value changes *not* initiated by the
        Setting's UI.

        The default implementation does nothing.
        '''
        pass

    def create_ui(
            self, override_device_name: typing.Optional[str] = None,
            form_layout: typing.Optional[QtWidgets.QFormLayout] = None) \
            -> typing.Union[QtWidgets.QWidget, QtWidgets.QLayout, None]:
        '''
        Create the UI for this Setting.

        A label will be created for the Setting and a row for it will be
        added to the provided form layout along with the widget/layout
        returned by _create_widgets().

        Ideally, derived classes will not need to override this function
        and can instead just implement _create_widgets().
        '''
        if not self.show_ui:
            # It's ok to use the Setting base class directly for
            # settings that do not display in the UI.
            return None

        top_level_widget = self._create_widgets(
            override_device_name=override_device_name)

        widget = self.get_widget(override_device_name=override_device_name)
        if widget and self.is_overridden(override_device_name):
            sb_widgets.set_qt_property(widget, "override", True)

        # Give the Setting's UI an opportunity to update itself when the
        # underlying value changes.
        self._register_on_setting_changed(top_level_widget, override_device_name)

        if top_level_widget and form_layout:
            setting_label = QtWidgets.QLabel()
            setting_label.setText(self.nice_name)
            if self.tool_tip:
                setting_label.setToolTip(self.tool_tip)

            form_layout.addRow(
                setting_label,                
                self._decorate_with_reset_widget(override_device_name, top_level_widget)
            )

        return top_level_widget
    
    def _register_on_setting_changed(self, top_level_widget: QtWidgets.QWidget, override_device_name: str):
        on_setting_changed_lambda = lambda old_value, new_value, override_device_name=override_device_name: \
            self._on_setting_changed(new_value, override_device_name=override_device_name)
        self.signal_setting_changed.connect(
            on_setting_changed_lambda
        )
        
        # Clear the widget when it is destroyed to avoid dangling references
        top_level_widget.destroyed.connect(lambda destroyed_object=None:
            self._on_widget_destroyed(on_setting_changed_lambda, override_device_name)
        )
        
    def _on_widget_destroyed(self, on_setting_changed_lambda, override_device_name: str):
        self.signal_setting_changed.disconnect(on_setting_changed_lambda)
        self.set_widget(widget=None, override_device_name=override_device_name)
    
    def _decorate_with_reset_widget(self, override_device_name: str, setting_editor_widget: QtWidgets.QWidget):
        # Reset will still be shown on overrides
        if not self._allow_reset and override_device_name is None:
            return setting_editor_widget
        
        horizontal_box = QtWidgets.QWidget()
        horizontal_layout = QtWidgets.QHBoxLayout(horizontal_box)
        horizontal_layout.setContentsMargins(0, 0, 0, 0)
        horizontal_layout.setSpacing(3)

        if isinstance(setting_editor_widget, QtWidgets.QWidget):
            horizontal_layout.addWidget(setting_editor_widget)
        elif isinstance(setting_editor_widget, QtWidgets.QLayout):
            horizontal_layout.addLayout(setting_editor_widget)

        button = QtWidgets.QPushButton()
        pixmap = QtGui.QPixmap(":icons/images/reset_to_default.png")
        button.setIcon(QtGui.QIcon(pixmap))
        button.setFlat(True)
        button.setSizePolicy(QtWidgets.QSizePolicy.Maximum, QtWidgets.QSizePolicy.Maximum)
        button.setMaximumWidth(12)
        button.setMaximumHeight(12)
        button.setToolTip("Reset to default")
        button.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        button.pressed.connect(
            lambda override_device_name=override_device_name:
                self._on_press_reset_override(override_device_name)
        )
        
        horizontal_layout.addWidget(button)
        is_base_reset_widget = override_device_name is None
        if is_base_reset_widget:
            self._base_reset_widget = button
            self._refresh_reset_base_widget()
        else:
            self._reset_override_widgets[override_device_name] = button
            self._refresh_reset_override_widget(override_device_name)
        return horizontal_box
    
    def _on_press_reset_override(self, override_device_name: str):
        if override_device_name is None:
            self.update_value(self._original_value)
            self._base_reset_widget.setVisible(False)
            self._refresh_reset_override_widgets()
            return
        
        if self.is_overridden(override_device_name):
            self.override_value(override_device_name, self._value)
            # Update UI
            self._on_widget_value_changed(self._value, override_device_name)
            self._on_setting_changed(self._value, override_device_name=override_device_name)
            self._reset_override_widgets[override_device_name].setVisible(False)
            
    def _refresh_reset_base_widget(self):
        if self._base_reset_widget:
            self._base_reset_widget.setVisible(self._value != self._original_value)

    def _refresh_reset_override_widgets(self):
        for device_name in self._overrides.keys():
            self._on_press_reset_override(device_name)

    def _refresh_reset_override_widget(self, device_name: str):
        if device_name in self._reset_override_widgets:
            self._reset_override_widgets[device_name].setVisible(self.is_overridden(device_name))

    def set_widget(
            self, widget: typing.Optional[QtWidgets.QWidget] = None,
            override_device_name: typing.Optional[str] = None):
        '''
        Set the widget to be used to manipulate this Setting, or
        this particular device's override of the Setting.

        A value of None can be provided for the widget to clear any
        stored widgets.
        '''
        if widget is None:
            # Clear the widget for this setting.
            if override_device_name is None:
                self._base_widget = None
            else:
                del self._override_widgets[override_device_name]
        else:
            if override_device_name is None:
                self._base_widget = widget
            else:
                self._override_widgets[override_device_name] = widget

    def get_widget(
            self, override_device_name: typing.Optional[str] = None) \
            -> typing.Optional[QtWidgets.QWidget]:
        '''
        Get the widget to be used to manipulate this Setting, or
        this particular device's override of the Setting.

        If no such widget was ever specified, None is returned.
        '''
        return self._override_widgets.get(
            override_device_name, self._base_widget)
    

class BoolSetting(Setting):
    '''
    A UI-displayable Setting for storing and modifying a boolean value.
    '''

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> QtWidgets.QCheckBox:
        check_box = QtWidgets.QCheckBox()
        check_box.setChecked(self.get_value(override_device_name))
        if self.tool_tip:
            check_box.setToolTip(self.tool_tip)

        self.set_widget(
            widget=check_box, override_device_name=override_device_name)

        check_box.stateChanged.connect(
            lambda state, override_device_name=override_device_name:
                self._on_widget_value_changed(
                    bool(state), override_device_name=override_device_name))

        return check_box

    def _on_setting_changed(
            self, new_value: bool,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if widget:
            widget.setChecked(new_value)

        if override_device_name is not None:
            # Reset the checkbox override state. As the checkbox has only
            # two states there is no override anymore when the base value
            # changes.
            if widget:
                sb_widgets.set_qt_property(widget, "override", False)
            self.remove_override(override_device_name)


class IntSetting(Setting):
    '''
    A UI-displayable Setting for storing and modifying an integer value.
    '''
    
    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: str,
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None,
        is_read_only: bool = False
    ):
        '''
        Create a new IntSetting object.

        Args:
            attr_name       : Internal name.
            nice_name       : Display name.
            value           : The initial value of this Setting.
            tool_tip        : Tooltip to show in the UI for this Setting.
            show_ui         : Whether to show this Setting in the Settings UI.
            is_read_only    : Whether to make entry field editable or not.
        '''
        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)

        self.is_read_only = is_read_only

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> QtWidgets.QLineEdit:
        line_edit = QtWidgets.QLineEdit()
        if self.tool_tip:
            line_edit.setToolTip(self.tool_tip)
        line_edit.setValidator(QtGui.QIntValidator())

        value = str(self.get_value(override_device_name))
        line_edit.setText(value)
        line_edit.setCursorPosition(0)
        line_edit.setReadOnly(self.is_read_only)

        self.set_widget(
            widget=line_edit, override_device_name=override_device_name)

        line_edit.editingFinished.connect(
            lambda line_edit=line_edit,
            override_device_name=override_device_name:
                self._on_widget_value_changed(
                    int(line_edit.text()),
                    override_device_name=override_device_name))

        return line_edit

    def _on_setting_changed(
            self, new_value: int,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if not widget:
            return

        old_value = int(widget.text())
        if new_value != old_value:
            widget.setText(str(new_value))
            widget.setCursorPosition(0)


class StringSetting(Setting):
    '''
    A UI-displayable Setting for storing and modifying a string value.
    '''

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: str,
        placeholder_text: str = '',
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None,
        is_read_only: bool = False
    ):
        '''
        Create a new StringSetting object.

        Args:
            attr_name       : Internal name.
            nice_name       : Display name.
            value           : The initial value of this Setting.
            placeholder_text: Placeholder for this Setting's value in the UI.
            tool_tip        : Tooltip to show in the UI for this Setting.
            show_ui         : Whether to show this Setting in the Settings UI.
            is_read_only    : Whether to make entry field editable or not.
        '''
        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)

        self.placeholder_text = placeholder_text
        self.is_read_only = is_read_only

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> QtWidgets.QLineEdit:
        line_edit = QtWidgets.QLineEdit()
        if self.tool_tip:
            line_edit.setToolTip(self.tool_tip)

        value = str(self.get_value(override_device_name))
        line_edit.setText(value)
        line_edit.setPlaceholderText(self.placeholder_text)
        line_edit.setCursorPosition(0)
        line_edit.setReadOnly(self.is_read_only)

        self.set_widget(
            widget=line_edit, override_device_name=override_device_name)

        line_edit.editingFinished.connect(
            lambda line_edit=line_edit,
            override_device_name=override_device_name:
                self._on_widget_value_changed(
                    line_edit.text().strip(),
                    override_device_name=override_device_name))

        return line_edit

    def _on_setting_changed(
            self, new_value: str,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if not widget:
            return

        old_value = widget.text().strip()
        if new_value != old_value:
            widget.setText(new_value)
            widget.setCursorPosition(0)


class FileSystemPathSetting(StringSetting):
    '''
    An abstract UI Setting for storing and modifying a string value that
    represents a file system path.

    This class provides the foundation for the DirectoryPathSetting and
    FilePathSetting classes. It should not be used directly.
    '''

    def _getFileSystemPath(
            self, parent: QtWidgets.QWidget = None,
            start_path: str = '') -> str:
        raise NotImplementedError(
            f'Setting "{self.nice_name}" uses the FileSystemPathSetting '
            'class directly. A derived class (e.g. DirectoryPathSetting or '
            'FilePathSetting) must be used instead.')

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> QtWidgets.QHBoxLayout:
        line_edit = super()._create_widgets(
            override_device_name=override_device_name)

        edit_layout = QtWidgets.QHBoxLayout()
        edit_layout.addWidget(line_edit)

        browse_btn = QtWidgets.QPushButton('Browse')
        browse_btn.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        edit_layout.addWidget(browse_btn)

        def on_browse_clicked():
            start_path = str(pathlib.Path.home())
            if (SETTINGS.LAST_BROWSED_PATH and
                    os.path.exists(SETTINGS.LAST_BROWSED_PATH)):
                start_path = SETTINGS.LAST_BROWSED_PATH

            fs_path = self._getFileSystemPath(
                parent=browse_btn, start_path=start_path)
            if len(fs_path) > 0 and os.path.exists(fs_path):
                fs_path = os.path.normpath(fs_path)

                self._on_widget_value_changed(
                    fs_path,
                    override_device_name=override_device_name)

                SETTINGS.LAST_BROWSED_PATH = os.path.dirname(fs_path)
                SETTINGS.save()

        browse_btn.clicked.connect(on_browse_clicked)

        return edit_layout


class DirectoryPathSetting(FileSystemPathSetting):
    '''
    A UI-displayable Setting for storing and modifying a string value that
    represents the path to a directory on the file system.
    '''

    def _getFileSystemPath(
            self, parent: QtWidgets.QWidget = None,
            start_path: str = '') -> str:
        return QtWidgets.QFileDialog.getExistingDirectory(
            parent=parent, dir=start_path)


class FilePathSetting(FileSystemPathSetting):
    '''
    A UI-displayable Setting for storing and modifying a string value that
    represents the path to a file on the file system.
    '''

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: str,
        placeholder_text: str = '',
        file_path_filter: str = '',
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True
    ):
        '''
        Create a new FilePathSetting object.

        Args:
            attr_name       : Internal name.
            nice_name       : Display name.
            value           : The initial value of this Setting.
            placeholder_text: Placeholder for this Setting's value in the UI.
            file_path_filter: Filter to use in the file browser.
            tool_tip        : Tooltip to show in the UI for this Setting.
            show_ui         : Whether to show this Setting in the Settings UI.
        '''
        super().__init__(
            attr_name, nice_name, value, placeholder_text=placeholder_text,
            tool_tip=tool_tip, show_ui=show_ui)

        self.file_path_filter = file_path_filter

    def _getFileSystemPath(
            self, parent: QtWidgets.QWidget = None,
            start_path: str = '') -> str:
        file_path, _ = QtWidgets.QFileDialog.getOpenFileName(
            parent=parent, dir=start_path, filter=self.file_path_filter)
        return file_path


class PerforcePathSetting(StringSetting):
    '''
    A UI-displayable Setting for storing and modifying a string value that
    represents a Perforce depot path.
    '''

    def _filter_value(self, value: typing.Optional[str]) -> str:
        '''
        Clean the p4 path value by removing whitespace and trailing '/'.
        '''
        if not value:
            return ''

        return value.strip().rstrip('/')


class OptionSetting(Setting):
    '''
    A UI-displayable Setting for storing and modifying a value that is
    one of a fixed set of options.
    '''

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value,
        possible_values: typing.List = None,
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None
    ):
        '''
        Create a new OptionSetting object.

        Args:
            attr_name      : Internal name.
            nice_name      : Display name.
            value          : The initial value of this Setting.
            possible_values: Possible values for this Setting.
            tool_tip       : Tooltip to show in the UI for this Setting.
            show_ui        : Whether to show this Setting in the Settings UI.
        '''
        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)

        self.possible_values = possible_values or []

    def _create_widgets(
        self, override_device_name: typing.Optional[str] = None, *,
        widget_class: Type[NonScrollableComboBox] = NonScrollableComboBox
    ) -> NonScrollableComboBox:
        combo = widget_class()
        if self.tool_tip:
            combo.setToolTip(self.tool_tip)

        for value in self.possible_values:
            combo.addItem(str(value), value)

        combo.setCurrentIndex(
            combo.findData(self.get_value(override_device_name)))

        self.set_widget(widget=combo,
                        override_device_name=override_device_name)

        combo.currentIndexChanged.connect(
            lambda index, override_device_name=override_device_name, combo=combo:
                self._on_widget_value_changed(
                    combo.itemData(index), override_device_name=override_device_name))

        return combo

    def _on_setting_changed(
            self, new_value,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if not widget:
            return

        old_value = widget.currentText()
        new_str_value = str(new_value)
        if new_str_value != old_value:
            index = widget.findText(new_str_value)
            if index != -1:
                widget.setCurrentIndex(index)


class MultiOptionSetting(OptionSetting):
    '''
    A UI-displayable Setting for storing and modifying a set of values,
    which may optionally be a subset of a fixed set of options.
    '''

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> sb_widgets.MultiSelectionComboBox:
        combo = sb_widgets.MultiSelectionComboBox()
        if self.tool_tip:
            combo.setToolTip(self.tool_tip)

        selected_values = self.get_value(override_device_name)
        possible_values = (
            self.possible_values
            if len(self.possible_values) > 0 else selected_values)
        combo.add_items(selected_values, possible_values)

        self.set_widget(
            widget=combo, override_device_name=override_device_name)

        combo.signal_selection_changed.connect(
            lambda entries, override_device_name=override_device_name:
                self._on_widget_value_changed(
                    entries, override_device_name=override_device_name))

        return combo

    def _on_setting_changed(
            self, new_value,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if not widget:
            return

        widget_model = widget.model()
        widget_items = [widget_model.item(i, 0) for i in range(widget.count())]
        widget_items = [item for item in widget_items if item.isEnabled()]

        old_value = [
            item.text()
            for item in widget_items
            if item.checkState() == QtCore.Qt.Checked]
        if new_value != old_value:
            selected_items = []
            for item in widget_items:
                if item.text() in new_value:
                    item.setCheckState(QtCore.Qt.Checked)
                    selected_items.append(item.text())
                else:
                    item.setCheckState(QtCore.Qt.Unchecked)

            widget.setEditText(widget.separator.join(selected_items))


class ListRow(QtWidgets.QWidget):
    INSERT_TEXT = "Insert"
    DUPLICATE_TEXT = "Duplicate"
    DELETE_TEXT = "Delete"
    
    def __init__(
        self,
        array_index: int,
        editor_widget: QtWidgets.QWidget,
        insert_item_callback: typing.Callable[[], None] = None, 
        duplicate_item_callback: typing.Callable[[], None] = None, 
        delete_item_callback: typing.Callable[[], None] = None,
        parent=None
    ):
        super().__init__(parent=parent)
        self._editor_widget = editor_widget
        self._insert_item_callback = insert_item_callback
        self._duplicate_item_callback = duplicate_item_callback
        self._delete_item_callback = delete_item_callback
    
        layout = QtWidgets.QHBoxLayout(self)
        layout.setSpacing(1)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Shift the elements to the right
        layout.addItem(
            QtWidgets.QSpacerItem(10, 0, QtWidgets.QSizePolicy.Maximum, QtWidgets.QSizePolicy.Maximum)
        )
        
        self._index_label = QtWidgets.QLabel()
        self._index_label.setFixedWidth(20)
        self.update_index(array_index)
        layout.addWidget(self._index_label)

        layout.addWidget(editor_widget)

        element_actions = DropDownMenuComboBox()
        element_actions.on_select_option.connect(self._on_view_option_selected)
        element_actions.addItem(self.INSERT_TEXT)
        element_actions.addItem(self.DUPLICATE_TEXT)
        element_actions.addItem(self.DELETE_TEXT)
        layout.addWidget(element_actions)

    def _on_view_option_selected(self, selected_item):
        if selected_item == self.INSERT_TEXT:
            self._insert_item_callback()
        elif selected_item == self.DUPLICATE_TEXT:
            self._duplicate_item_callback()
        elif selected_item == self.DELETE_TEXT:
            self._delete_item_callback()
    
    @property
    def editor_widget(self):
        return self._editor_widget
    
    def update_index(self, index: int):
        self._index_label.setText(str(index))


class ListSetting(Setting):
    '''
    A setting which has add and clear buttons. Each item will be displayed in a new line.
    Functions like array properties in Unreal Editor.
    
    Subclasses are responsible for generating widgets for the array contents, see e.g. ArrayStringSetting.
    '''

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: typing.List = [],
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None
    ):
        '''
        Create a new ListSetting object.

        Args:
            attr_name      : Internal name.
            nice_name      : Display name.
            value          : The initial value of this Setting.
            tool_tip       : Tooltip to show in the UI for this Setting.
            show_ui        : Whether to show this Setting in the Settings UI.
        '''
        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)
        
        self.array_count_labels = {}
        self.element_layouts = {}
    
    def create_element(self, override_device_name: str, index: int) -> typing.Tuple[QtWidgets.QWidget, object]:
        '''
        Called when a new array element is supposed to be created.
        @returns The widget to use for editing and the default value for the new array element
        '''
        raise NotImplementedError("Subclasses must override this")
    
    def update_element_value(self, editor_widget: QtWidgets.QWidget, list_value: typing.List, index: int):
        '''
        Called to update the editor_widget with the the value from list_value[index].
        '''
        raise NotImplementedError("Subclasses must override this")

    def _create_widgets(self, override_device_name: typing.Optional[str] = None):
        root = QtWidgets.QWidget()
        root_layout = QtWidgets.QVBoxLayout(root)
        root_layout.setSpacing(1)
        root_layout.setContentsMargins(1, 1, 1, 1)
        
        root_layout.addWidget(
            self._create_header(override_device_name)
        )
        
        elements_root = QtWidgets.QWidget()
        elements_layout = QtWidgets.QVBoxLayout(elements_root)
        self.element_layouts[override_device_name] = elements_layout
        elements_layout.setSpacing(1)
        elements_layout.setContentsMargins(8, 5, 2, 2)
        root_layout.addWidget(
            elements_root
        )

        self.set_widget(
            widget=root, override_device_name=override_device_name)
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)
        return root

    def _create_header(self, override_device_name) -> QtWidgets.QWidget:
        header = QtWidgets.QWidget()
        header_layout = QtWidgets.QHBoxLayout(header)
        header_layout.setSpacing(5)
        header_layout.setContentsMargins(0, 0, 0, 0)
        
        array_count_label = QtWidgets.QLabel()
        self.array_count_labels[override_device_name] = array_count_label
        array_count_label.setFixedWidth(100)
        self._update_array_count_label(override_device_name)

        add_button = QtWidgets.QPushButton()
        pixmap = QtGui.QPixmap(":icons/images/PlusSymbol_12x.png")
        add_button.setIcon(QtGui.QIcon(pixmap))
        add_button.setFlat(True)
        add_button.setSizePolicy(QtWidgets.QSizePolicy.Maximum, QtWidgets.QSizePolicy.Maximum)
        add_button.setMaximumWidth(12)
        add_button.setMaximumHeight(12)
        add_button.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        add_button.pressed.connect(
            lambda override_device_name=override_device_name:
                self._on_press_add(override_device_name)
        )

        clear_button = QtWidgets.QPushButton()
        pixmap = QtGui.QPixmap(":icons/images/empty_set_12x.png")
        clear_button.setIcon(QtGui.QIcon(pixmap))
        clear_button.setFlat(True)
        clear_button.setSizePolicy(QtWidgets.QSizePolicy.Maximum, QtWidgets.QSizePolicy.Maximum)
        clear_button.setMaximumWidth(12)
        clear_button.setMaximumHeight(12)
        clear_button.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        clear_button.pressed.connect(
            lambda override_device_name=override_device_name:
            self._on_press_clear(override_device_name)
        )

        header_layout.addWidget(array_count_label)
        header_layout.addWidget(add_button)
        header_layout.addWidget(clear_button)
        header_layout.addItem(QtWidgets.QSpacerItem(0, 0, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum))

        return header
        
    def _update_array_count_label(self, override_device_name: str):
        current_value = self.get_value(override_device_name)
        self.array_count_labels[override_device_name].setText(f"{len(current_value)} Elements")
        
    def _on_press_add(self, override_device_name: str):
        current_value = self.get_value(override_device_name)
        row_widget, default_value = self._create_element_row(override_device_name, len(current_value))
        self.element_layouts[override_device_name].addWidget(
            row_widget
        )

        self._on_widget_value_changed(current_value + [default_value], override_device_name)
        # Force update in case _on_widget_value_changed implicitly changed array settings without telling us
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)
        
    def _on_press_clear(self, override_device_name: str):
        self._on_widget_value_changed([], override_device_name)
        
        container_layout = self.element_layouts[override_device_name]
        for i in reversed(range(container_layout.count())):
            container_layout.itemAt(i).widget().setParent(None)
        
        # Force update in case _on_widget_value_changed implicitly changed array settings without telling us
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)
    
    def _insert_at(self, override_device_name: str, index: int):
        current_value = self.get_value(override_device_name)
        row_widget, default_value = self._create_element_row(override_device_name, len(current_value))
        self.element_layouts[override_device_name].addWidget(
            row_widget
        )

        copied_value = current_value.copy()
        copied_value.insert(index, default_value)
        
        self._on_widget_value_changed(copied_value, override_device_name)
        # Force update in case _on_widget_value_changed implicitly changed array settings without telling us
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)
        
    def _duplicate_at(self, override_device_name: str, index: int):
        current_value = self.get_value(override_device_name)
        row_widget, _ = self._create_element_row(override_device_name, len(current_value))
        self.element_layouts[override_device_name].addWidget(
            row_widget
        )

        copied_value = current_value.copy()
        copied_value.insert(index, current_value[index])

        self._on_widget_value_changed(copied_value, override_device_name)
        # Force update in case _on_widget_value_changed implicitly changed array settings without telling us
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)
        
    def _remove_at(self, override_device_name: str, index: int):
        current_value = self.get_value(override_device_name)
        copied_value = current_value.copy()
        del copied_value[index]

        self._on_widget_value_changed(copied_value, override_device_name)
        # Force update in case _on_widget_value_changed implicitly changed array settings without telling us
        self._on_setting_changed(self.get_value(override_device_name), override_device_name)

    def _create_element_row(self, override_device_name: str, index: int) -> typing.Tuple[ListRow, object]:
        editing_widget, default_value = self.create_element(override_device_name, index)
        row = ListRow(
            index,
            editing_widget, 
            lambda override_device_name=override_device_name, index=index:
                self._insert_at(override_device_name, index),
            lambda override_device_name=override_device_name, index=index:
                self._duplicate_at(override_device_name, index),
            lambda override_device_name=override_device_name, index=index:
                self._remove_at(override_device_name, index)
        )
        return row, default_value

    def _on_setting_changed(self, new_value: typing.List, override_device_name: typing.Optional[str] = None):
        container_layout = self.element_layouts[override_device_name]
        
        container_len = container_layout.count()
        new_len = len(new_value)
        new_list_is_bigger = container_layout.count() < new_len
        new_list_is_smaller = container_layout.count() > new_len
        if new_list_is_bigger:
            for missing_index in range(container_layout.count(), len(new_value), 1):
                new_array_row, _ = self._create_element_row(override_device_name, missing_index)
                container_layout.addWidget(new_array_row)
        
        if new_list_is_smaller:
            for added_index in reversed(range(new_len, container_layout.count(), 1)):
                container_layout.itemAt(added_index).widget().deleteLater()
                
        for index in range(new_len):
            array_row: ListRow = container_layout.itemAt(index).widget()
            self.update_element_value(array_row.editor_widget, new_value, index)

        self._update_array_count_label(override_device_name)
    
    
class StringListSetting(ListSetting):
    '''
    An array setting where the elements are strings
    '''
    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: typing.List[str] = [],
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None
    ):
        '''
        Create a new ArraySetting object.

        Args:
            attr_name      : Internal name.
            nice_name      : Display name.
            value          : The initial value of this Setting.
            tool_tip       : Tooltip to show in the UI for this Setting.
            show_ui        : Whether to show this Setting in the Settings UI.
        '''
        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)
        pass

    def create_element(self, override_device_name: str, index: int) -> typing.Tuple[QtWidgets.QWidget, object]:
        line_edit = QtWidgets.QLineEdit()
        line_edit.editingFinished.connect(
            lambda line_edit=line_edit, override_device_name=override_device_name, index=index:
                self._on_editor_widget_changed(line_edit, override_device_name, index)
        )
        return line_edit, ""
    
    def _on_editor_widget_changed(self, line_edit, override_device_name, index):
        current_list = self.get_value(override_device_name)
        # List needs to a different object otherwise _on_widget_value_changed will think the value has not changed
        copied_value = current_list.copy()
        copied_value[index] = line_edit.text()
        self._on_widget_value_changed(copied_value, override_device_name)

    def update_element_value(self, editor_widget: QtWidgets.QLineEdit, list_value: typing.List, index: int):
        editor_widget.setText(list_value[index])


class LoggingModel(QtGui.QStandardItemModel):
    '''
    A data model for storing a logging configuration that maps logging
    category names to the desired log verbosity for messages of that
    category.

    A value of None can be mapped to a category indicating that messages
    for that category should be emitted with the default verbosity level.

    The model allows the list of categories to be extended beyond the list
    given at initialization. Categories added in this way can be dynamically
    added or removed by the user, while categories in the initial list are
    fixed and cannot be removed.
    '''

    CATEGORY_COLUMN = 0
    VERBOSITY_COLUMN = 1
    NUM_COLUMNS = 2

    def __init__(
            self,
            categories: typing.List[str],
            verbosity_levels: typing.List[str],
            parent: typing.Optional[QtCore.QObject] = None):
        '''
        Create a new LoggingModel object.

        Args:
            categories      : List of logging category names.
            verbosity_levels: List of possible verbosity level settings for
                              a category.
            parent          : The QObject parent of this object.
        '''
        super().__init__(parent=parent)

        self._categories = categories or []
        self._user_categories = []
        self._verbosity_levels = verbosity_levels or []

        self.setColumnCount(LoggingModel.NUM_COLUMNS)
        self.setHorizontalHeaderLabels(['Category', 'Verbosity Level'])
        self.horizontalHeaderItem(
            LoggingModel.CATEGORY_COLUMN).setToolTip(
                'The name of the category of logging messages')
        self.horizontalHeaderItem(
            LoggingModel.VERBOSITY_COLUMN).setToolTip(
                'The level of verbosity at which to emit messages for the '
                'category')

    @property
    def categories(self) -> typing.List[str]:
        return self._categories

    @property
    def user_categories(self) -> typing.List[str]:
        return self._user_categories

    @property
    def verbosity_levels(self) -> typing.List[str]:
        return self._verbosity_levels

    def is_user_category(self, category: str) -> bool:
        '''
        Returns True if the given category was added after the
        model was initialized.
        '''
        return (
            category in self.user_categories and
            category not in self.categories)

    def add_user_category(self, category: str) -> bool:
        '''
        Adds a row for the category to the model.

        Returns True if the category was added, or False otherwise.
        '''
        if category in self.categories or category in self.user_categories:
            return False

        self._user_categories.append(category)

        root_item = self.invisibleRootItem()

        root_item.appendRow(
            [QtGui.QStandardItem(category), QtGui.QStandardItem(None)])

        return True

    def remove_user_category(self, category: str) -> bool:
        '''
        Removes the category from the model.

        Returns True if the category was removed, or False otherwise.
        '''
        if not self.is_user_category(category):
            return False

        self._user_categories.remove(category)

        category_items = self.findItems(
            category, column=LoggingModel.CATEGORY_COLUMN)
        if not category_items:
            return False

        root_item = self.invisibleRootItem()

        row = category_items[0].index().row()

        root_item.removeRow(row)

        return True

    def category_at(self, index: QtCore.QModelIndex):
        return self.invisibleRootItem().child(index.row(), LoggingModel.CATEGORY_COLUMN).text()

    @property
    def category_verbosities(self) -> collections.OrderedDict:
        '''
        Returns the data currently stored in the model as a dictionary
        mapping each category name to a verbosity level (or None).
        '''
        value = collections.OrderedDict()

        root_item = self.invisibleRootItem()

        for row in range(root_item.rowCount()):
            category_item = root_item.child(
                row, LoggingModel.CATEGORY_COLUMN)
            verbosity_level_item = root_item.child(
                row, LoggingModel.VERBOSITY_COLUMN)

            value[category_item.text()] = verbosity_level_item.text() or None

        return value

    @category_verbosities.setter
    def category_verbosities(
            self,
            value: typing.Optional[typing.Dict[str, typing.Optional[str]]]):
        '''
        Sets the data in the model using the given dictionary of category
        names to verbosity levels (or None).
        '''
        value = value or collections.OrderedDict()

        # sort them by name to facilitate finding them
        value = collections.OrderedDict(sorted(value.items(), key=lambda item: str(item[0]).lower()))

        self.beginResetModel()
        count_before = self.rowCount()
        
        root_item = self.invisibleRootItem()
        for category, verbosity_level in value.items():
            if (category not in self._categories and
                    category not in self._user_categories):
                self._user_categories.append(category)

            root_item.appendRow(
                [QtGui.QStandardItem(category),
                    QtGui.QStandardItem(verbosity_level)])
            
        # This triggers the rowsRemoved event. 
        # Remove rows after so external observers get the right result from category_verbosities.  
        self.removeRows(0, count_before)
        self.endResetModel()

    def flags(self, index: QtCore.QModelIndex) -> QtCore.Qt.ItemFlags:
        '''
        Returns the item flags for the item at the given index.
        '''
        if not index.isValid():
            return QtCore.Qt.ItemIsEnabled

        item_flags = (QtCore.Qt.ItemIsEnabled | QtCore.Qt.ItemIsSelectable)

        if index.column() == LoggingModel.VERBOSITY_COLUMN:
            item_flags |= QtCore.Qt.ItemIsEditable

        return item_flags


class LoggingVerbosityItemDelegate(QtWidgets.QStyledItemDelegate):
    '''
    A delegate for items in the verbosity column of the logging view.

    This delegate manages creating a combo box with the available
    verbosity levels.
    '''

    def __init__(
            self,
            verbosity_levels: typing.List[str],
            parent: QtWidgets.QTreeView):
        super().__init__(parent=parent)
        self._verbosity_levels = verbosity_levels or []
        self._parent = parent
        self._selected_categories = []

    def createEditor(
            self, parent: QtWidgets.QWidget,
            option: QtWidgets.QStyleOptionViewItem,
            index: QtCore.QModelIndex) -> sb_widgets.NonScrollableComboBox:
        editor = sb_widgets.NonScrollableComboBox(parent)
        editor.addItems(self._verbosity_levels)

        # Pre-select the level currently specified in the model, if any.
        current_value = index.model().data(index)
        if current_value in self._verbosity_levels:
            current_index = self._verbosity_levels.index(current_value)
            editor.setCurrentIndex(current_index)

        edited_category = self._parent.model().category_at(index)
        editor.onHoverScrollBox.connect(
            lambda edited_category=edited_category:
                self.on_hover_combo_box(edited_category)
        )
        # For multi-selection we want to also be modified even when index has not changed
        editor.activated.connect(
            lambda combo_index, 
                editor=editor:
                self.on_current_index_changed(combo_index, editor)
        )
        
        return editor

    def setEditorData(
            self, editor: QtWidgets.QWidget, index: QtCore.QModelIndex):
        editor.blockSignals(True)
        editor.setCurrentIndex(editor.currentIndex())
        editor.blockSignals(False)

    def setModelData(
            self, editor: QtWidgets.QWidget, model: QtCore.QAbstractItemModel,
            index: QtCore.QModelIndex):
        model.setData(index, editor.currentText() or None)

    def on_hover_combo_box(self, edited_category: str):
        selection = self._parent.selected_categories()
        # Discard the selection if the user clicked a combo box outside of the selection
        self._selected_categories = selection if edited_category in selection else [edited_category]

    def on_current_index_changed(self, combo_index, editor):
        self._parent.update_category_verbosities(self._selected_categories, editor.itemText(combo_index))


class LoggingSettingVerbosityView(QtWidgets.QTreeView):
    '''
    A tree view that presents the logging configuration represented by the
    given LoggingModel.
    '''

    def __init__(
            self,
            logging_model: LoggingModel,
            parent: typing.Optional[QtWidgets.QWidget] = None):
        super().__init__(parent=parent)

        self.setModel(logging_model)
        self.header().setSectionResizeMode(
            LoggingModel.CATEGORY_COLUMN,
            QtWidgets.QHeaderView.Stretch)
        self.resizeColumnToContents(LoggingModel.VERBOSITY_COLUMN)
        self.header().setSectionResizeMode(
            LoggingModel.VERBOSITY_COLUMN,
            QtWidgets.QHeaderView.Fixed)
        self.header().setStretchLastSection(False)

        self.setItemDelegateForColumn(
            LoggingModel.VERBOSITY_COLUMN,
            LoggingVerbosityItemDelegate(logging_model.verbosity_levels, self))
        
        self._open_persistent_editors()
        logging_model.modelAboutToBeReset.connect(self._pre_change_model)
        logging_model.modelReset.connect(self._post_change_model)

        self.setSelectionBehavior(QtWidgets.QTreeView.SelectRows)
        self.setSelectionMode(QtWidgets.QTreeView.ExtendedSelection)
        
    def _pre_change_model(self):
        self.scroll_height = self.verticalScrollBar().value()
        
    def _post_change_model(self):
        self.verticalScrollBar().setSliderPosition(self.scroll_height)
        self._open_persistent_editors()
        
    def _open_persistent_editors(self):
        for row in range(self.model().rowCount()):
            verbosity_level_index = self.model().index(row, LoggingModel.VERBOSITY_COLUMN)
            self.openPersistentEditor(verbosity_level_index)
        
    def selected_categories(self) -> typing.List[str]:
        model = self.model()
        selected_categories = []
        for selection_range in self.selectionModel().selection():
            rows = range(selection_range.top(), selection_range.bottom() + 1)
            for row in rows:
                category_index = model.index(row, 0)
                category = model.data(category_index, QtCore.Qt.DisplayRole)
                selected_categories.append(category)
                
        return selected_categories
        
    def update_category_verbosities(self, categories: typing.List[str], new_verbosity: str):
        model = self.model()
        category_verbosities = model.category_verbosities
        for category in categories:
            category_verbosities[category] = new_verbosity

        model.category_verbosities = category_verbosities


class LoggingSetting(Setting):
    '''
    A UI-displayable Setting for storing and modifying a set of logging
    categories and the verbosity level of each category.

    An initial set of categories can be provided when creating the Setting,
    but adding additional user-defined categories is supported as well.
    '''

    # Extracted from ParseLogVerbosityFromString() in
    # Engine\Source\Runtime\Core\Private\Logging\LogVerbosity.cpp
    # None indicates the "default" verbosity level should be used with
    # no override applied.
    DEFAULT_VERBOSITY_LEVELS = [
        None, 'VeryVerbose', 'Verbose', 'Log', 'Display',
        'Warning', 'Error', 'Fatal', 'NoLogging']

    def _filter_value(
            self,
            value: typing.Optional[typing.Dict[str, typing.Optional[str]]]) \
            -> collections.OrderedDict:
        '''
        Filter function to modify the incoming value before updating or
        overriding the setting.

        This ensures that LoggingSetting values are always provided using
        a dictionary (regular Python dict or OrderedDict) or None. An exception
        is raised otherwise.

        The resulting dictionary will include a key/value pair for each
        category in the LoggingSetting. Category names not present in the
        input dictionary will have a value of None in the output dictionary.
        '''
        if value is None:
            value = collections.OrderedDict()
        else:
            try:
                value = collections.OrderedDict(value)
            except Exception as e:
                raise ValueError(
                    'Invalid LoggingSetting value. Values must be '
                    f'either dictionary-typed or None: {e}')

        for category in self._categories:
            if category not in value:
                value[category] = None

        return value

    def __init__(
        self,
        attr_name: str,
        nice_name: str,
        value: typing.Dict[str, typing.Optional[str]],
        categories: typing.List[str] = None,
        verbosity_levels: typing.List[str] = None,
        tool_tip: typing.Optional[str] = None,
        show_ui: bool = True,
        allow_reset: bool = True,
        migrate_data: typing.Callable[[any], None] = None
    ):
        '''
        Create a new LoggingSetting object.

        Args:
            attr_name       : Internal name.
            nice_name       : Display name.
            value           : The initial value of this Setting.
            categories      : The initial list of logging categories.
            verbosity_levels: The possible settings for verbosity level of
                              each category.
            tool_tip        : Tooltip to show in the UI for this Setting.
            show_ui         : Whether to show this Setting in the Settings UI.
        '''

        # Set the categories before calling the base class init since they
        # will be used when filtering the value.
        self._categories = categories or []

        super().__init__(
            attr_name, nice_name, value,
            tool_tip=tool_tip, show_ui=show_ui, allow_reset=allow_reset, migrate_data=migrate_data)

        self._verbosity_levels = (
            verbosity_levels or self.DEFAULT_VERBOSITY_LEVELS)

    def get_command_line_arg(
            self, override_device_name: typing.Optional[str] = None) \
            -> str:
        '''
        Generate the command line argument for specifying the logging
        configuration based on the value currently stored in the Setting.

        Only categories that have a verbosity level specified are included in
        the result. If no categories have a verbosity level specified, an
        empty string is returned.
        '''
        value = self.get_value(override_device_name)

        logging_strings = [
            f'{category} {level}' for category, level in value.items()
            if level]
        if not logging_strings:
            return ''

        return f'-LogCmds=\"{", ".join(logging_strings)}\"'

    def _create_widgets(
            self, override_device_name: typing.Optional[str] = None) \
            -> QtWidgets.QVBoxLayout:
        model = LoggingModel(
            categories=self._categories,
            verbosity_levels=self._verbosity_levels)
        model.category_verbosities = self._value
        view = LoggingSettingVerbosityView(logging_model=model)
        view.setMinimumHeight(150)

        self.set_widget(
            widget=view, override_device_name=override_device_name)

        edit_layout = QtWidgets.QVBoxLayout()
        edit_layout.addWidget(view)

        add_category_button = QtWidgets.QPushButton('Add Category')
        add_category_button.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        edit_layout.addWidget(add_category_button)

        def on_add_category_button_clicked():
            category, ok = QtWidgets.QInputDialog().getText(
                add_category_button, "Add Category",
                "Category name:", QtWidgets.QLineEdit.Normal)
            if not ok:
                return

            category = category.strip()
            if not category:
                return

            if model.add_user_category(category):
                verbosity_level_index = model.index(
                    model.rowCount() - 1, LoggingModel.VERBOSITY_COLUMN)
                view.openPersistentEditor(verbosity_level_index)

        add_category_button.clicked.connect(on_add_category_button_clicked)

        remove_category_button = QtWidgets.QPushButton('Remove Category')
        remove_category_button.setFocusPolicy(QtCore.Qt.FocusPolicy.NoFocus)
        edit_layout.addWidget(remove_category_button)

        def on_remove_category_button_clicked():
            category_indices = view.selectionModel().selectedRows(
                LoggingModel.CATEGORY_COLUMN)
            if not category_indices:
                return
            
            category_list_str = ''
            category_list = []
            for category_index in category_indices:
                category = model.itemFromIndex(category_index).text()
                category_list.append(category)
                category_list_str += f'{category}\n'
            reply = QtWidgets.QMessageBox.question(
                remove_category_button, 'Confirm Remove Category',
                ('Are you sure you would like to remove the following categories: '
                 f'{category_list_str}'),
                QtWidgets.QMessageBox.Yes, QtWidgets.QMessageBox.No)

            if reply == QtWidgets.QMessageBox.Yes:
                view.selectionModel().clear()
                for category in category_list:
                    model.remove_user_category(category)

        remove_category_button.clicked.connect(
            on_remove_category_button_clicked)

        # The remove button is disabled initially until a user category is
        # selected.
        REMOVE_BUTTON_DISABLED_TOOLTIP = (
            'Default categories for the device cannot be removed')
        remove_category_button.setEnabled(False)
        remove_category_button.setToolTip(REMOVE_BUTTON_DISABLED_TOOLTIP)

        # Enable the remove button when a user category is selected, and
        # disable it otherwise.
        def on_view_selectionChanged(selected, deselected):
            remove_category_button.setEnabled(False)
            remove_category_button.setToolTip(REMOVE_BUTTON_DISABLED_TOOLTIP)

            category_indices = view.selectionModel().selectedRows(
                LoggingModel.CATEGORY_COLUMN)
            if not category_indices:
                return

            all_user_category = True
            for category_index in category_indices:
                category = model.itemFromIndex(category_index).text()
                all_user_category &= model.is_user_category(category)
            if all_user_category:
                remove_category_button.setEnabled(True)
                remove_category_button.setToolTip('')

        view.selectionModel().selectionChanged.connect(
            on_view_selectionChanged)
        
        def on_logging_model_modified(override_device_name=None):
            category_verbosities = model.category_verbosities
            self._on_widget_value_changed(
                category_verbosities,
                override_device_name=override_device_name)

        model.dataChanged.connect(
            lambda top_left_index, bottom_right_index, roles,
            override_device_name=override_device_name:
                on_logging_model_modified(
                    override_device_name=override_device_name))

        model.rowsInserted.connect(
            lambda parent, first, last,
            override_device_name=override_device_name:
                on_logging_model_modified(
                    override_device_name=override_device_name))

        model.rowsRemoved.connect(
            lambda parent, first, last,
            override_device_name=override_device_name:
                on_logging_model_modified(
                    override_device_name=override_device_name))
        
        
        return edit_layout

    def _on_setting_changed(
            self, new_value,
            override_device_name: typing.Optional[str] = None):
        widget = self.get_widget(override_device_name=override_device_name)
        if not widget:
            return

        old_value = widget.model().category_verbosities
        if new_value != old_value:
            widget.model().category_verbosities = new_value


class AddressSetting(OptionSetting):
    def __init__(
        self,
        attr_name,
        nice_name,
        value,
        tool_tip=None,
        show_ui=True,
        allow_reset=True,
        migrate_data=None
    ):
        super().__init__(
            attr_name=attr_name,
            nice_name=nice_name,
            value=value,
            possible_values=list(self.generate_possible_addresses()),
            tool_tip=tool_tip,
            show_ui=show_ui,
            allow_reset=allow_reset,
            migrate_data=migrate_data)

    def _create_widgets(
        self, override_device_name: typing.Optional[str] = None
    ) -> NonScrollableComboBox:
        combo: NonScrollableComboBox = super()._create_widgets(
            override_device_name, widget_class=sb_widgets.AddressComboBox)

        combo.setInsertPolicy(QtWidgets.QComboBox.InsertPolicy.NoInsert)

        cur_value = self.get_value(override_device_name)
        if combo.findText(cur_value, QtCore.Qt.MatchFlag.MatchExactly) == -1:
            combo.addItem(str(cur_value), cur_value)
            combo.setCurrentIndex(combo.findText(cur_value))

        combo.lineEdit().editingFinished.connect(
            lambda: self._validate_and_commit_address(combo,
                                                      override_device_name)
        )

        return combo

    def _validate_and_commit_address(
            self, combo: NonScrollableComboBox, override_device_name: str):
        address_str = combo.lineEdit().text()
        self._on_widget_value_changed(address_str, override_device_name=override_device_name)

    def generate_possible_addresses(self):
        return set[str]()


class LocalAddressSetting(AddressSetting):
    def generate_possible_addresses(self):
        addresses = set[str]()
        for address in socket.getaddrinfo(socket.gethostname(), None,
                                          socket.AF_INET):
            addresses.add(str(address[4][0]))
        addresses.add('127.0.0.1')
        addresses.add('localhost')
        addresses.add(socket.gethostname())
        return addresses


class ConfigPathError(Exception):
    '''
    Base exception type for config path related errors.
    '''
    pass


class ConfigPathEmptyError(ConfigPathError):
    '''
    Exception type raised when an empty or all whitespace string is used as a
    config path.
    '''
    pass


class ConfigPathLocationError(ConfigPathError):
    '''
    Exception type raised when a config path is located outside of the root
    configs directory.
    '''
    pass


class ConfigPathIsUserSettingsError(ConfigPathError):
    '''
    Exception type raised when the user settings file path is used as a config
    path.
    '''
    pass


def get_absolute_config_path(
        config_path: typing.Union[str, pathlib.Path]) -> pathlib.Path:
    '''
    Returns the given string or path object as an absolute config path.

    The string/path is validated to ensure that:
      - It is not empty, or all whitespace
      - It ends with the config path suffix
      - It is not the same path as the user settings file path
    '''
    if isinstance(config_path, str):
        config_path = config_path.strip()
        if not config_path:
            raise ConfigPathEmptyError('Config path cannot be empty')

        config_path = pathlib.Path(config_path)

    # Manually add the suffix instead of using pathlib.Path.with_suffix().
    # For strings like "foo.bar", with_suffix() will first remove ".bar"
    # before adding the suffix, which we don't want it to do.
    if not config_path.name.endswith(CONFIG_SUFFIX):
        config_path = config_path.with_name(
            f'{config_path.name}{CONFIG_SUFFIX}')

    if not config_path.is_absolute():
        # Relative paths can simply be made absolute.
        config_path = ROOT_CONFIGS_PATH.joinpath(config_path)

    if config_path.resolve() == USER_SETTINGS_FILE_PATH:
        raise ConfigPathIsUserSettingsError(
            'Config path cannot be the same as the user settings file '
            f'path "{USER_SETTINGS_FILE_PATH}"')

    return config_path


def get_relative_config_path(
        config_path: typing.Union[str, pathlib.Path]) -> pathlib.Path:
    '''
    Returns the given string or path object as a config path relative to the
    root configs path.

    An absolute path is generated first to perform all of the same validation
    as get_absolute_config_path() before the relative path is computed and
    returned.
    '''
    config_path = get_absolute_config_path(config_path)
    return config_path.relative_to(ROOT_CONFIGS_PATH)


class ConfigPathValidator(QtGui.QValidator):
    '''
    Validator to determine whether the input is an acceptable config file
    path.

    If the input is not acceptable, the state is returned as Intermediate
    rather than Invalid so as not to interfere with the user typing in the
    text field.
    '''

    def validate(self, input, pos):
        try:
            get_absolute_config_path(input)
        except Exception:
            return QtGui.QValidator.Intermediate

        return QtGui.QValidator.Acceptable


class Config(object):

    DEFAULT_CONFIG_PATH = ROOT_CONFIGS_PATH.joinpath(f'Default{CONFIG_SUFFIX}')

    saving_allowed = True
    saving_allowed_fifo = []

    def push_saving_allowed(self, value):
        ''' Sets a new state of saving allowed, but pushes current to the stack
        '''
        self.saving_allowed_fifo.append(self.saving_allowed)
        self.saving_allowed = value

    def pop_saving_allowed(self):
        ''' Restores saving_allowed flag from the stack
        '''
        self.saving_allowed = self.saving_allowed_fifo.pop()
        
    def init(self, file_path: typing.Union[str, pathlib.Path]):
        self.init_with_file_path(file_path)

    def init_with_file_path(self, file_path: typing.Union[str, pathlib.Path]):
        if file_path:
            try:
                self.file_path = get_absolute_config_path(file_path)

                # Read the json config file
                with open(self.file_path, 'r') as f:
                    LOGGER.debug(f'Loading Config {self.file_path}')
                    data = json.load(f)                    
                        
            except (ConfigPathError, FileNotFoundError) as e:
                LOGGER.error(f'Config: {e}')
                self.file_path = None
                data = {}
            except ValueError:
                # The original file will be overwritten
                self._backup_corrupted_config(self.file_path.__str__())
                data = {}
        else:
            self.file_path = None
            data = {}

        self.init_switchboard_settings()
        self.init_project_settings(data)
        self.init_unreal_insights(data)
        self.init_muserver(data)

        # Automatically save whenever a project setting is changed or
        # overridden by a device.
        # TODO: switchboard_settings
        all_settings = [setting for _, setting in self.basic_project_settings.items()] \
            + [setting for _, setting in self.osc_settings.items()] \
            + [setting for _, setting in self.source_control_settings.items()] \
            + [setting for _, setting in self.unreal_insight_settings.items()] \
            + [setting for _, setting in self.mu_settings.items()]
            # TODO: multiuser
        for setting in all_settings:
            setting.signal_setting_changed.connect(lambda: self.save())
            setting.signal_setting_overridden.connect(
                self.on_device_override_changed)

        # Directory Paths
        self.SWITCHBOARD_DIR = os.path.abspath(
            os.path.join(os.path.dirname(os.path.abspath(__file__)), '../'))

        # MISC SETTINGS
        self.CURRENT_LEVEL = data.get('current_level', DEFAULT_MAP_TEXT)

        # Devices
        self._device_data_from_config = {}
        self._plugin_data_from_config = {}
        self._device_settings = {}
        self._plugin_settings = {}

        # Convert devices data from dict to list so they can be directly fed
        # into the kwargs.
        for device_type, devices in data.get('devices', {}).items():
            for device_name, data in devices.items():
                if device_name == 'settings':
                    self._plugin_data_from_config[device_type] = data
                else:
                    # Migrate ip_address -> address
                    if 'ip_address' in data:
                        address = data['ip_address']
                        del data['ip_address']
                    else:
                        address = data['address']

                    device_data = {
                        'name': device_name,
                        'address': address
                    }
                    device_data['kwargs'] = {
                        k: v for (k, v) in data.items() if k != 'address'}
                    self._device_data_from_config.setdefault(
                        device_type, []).append(device_data)

    def _backup_corrupted_config(self, original_file_path: str):
        directory_name = os.path.dirname(original_file_path)
        original_file_name = os.path.basename(original_file_path)
        new_file_name = original_file_name.replace(".", "_corrupted_backup.")

        LOGGER.error(f'{original_file_name} has invalid JSON format. Creating default...')
        answer = QtWidgets.QMessageBox.question(
            None,
            'Invalid project settings',
            f'Config file { original_file_name } is invalid JSON and will be replaced by a new default JSON config.'
            f'\n\nDo you want to save a backup named { new_file_name }?',
            QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No
        )
        if answer == QtWidgets.QMessageBox.Yes:
            new_file_path = os.path.join(directory_name, new_file_name)
            if os.path.exists(new_file_path):
                os.remove(new_file_path)
            shutil.copy(original_file_path, new_file_path)

    def init_new_config(self, file_path: typing.Union[str, pathlib.Path], uproject, engine_dir, p4_settings):
        ''' 
        Initialize new configuration
        '''

        self.file_path = get_absolute_config_path(file_path)
        self.init_switchboard_settings()
        self.init_project_settings(
            { 
                "project_name": self.file_path.stem, 
                "uproject": uproject, 
                "engine_dir": engine_dir,
            } | p4_settings)
        self.init_unreal_insights()
        self.init_muserver()

        self.CURRENT_LEVEL = DEFAULT_MAP_TEXT

        self._device_data_from_config = {}
        self._plugin_data_from_config = {}
        self._plugin_settings = {}
        self._device_settings = {}

        LOGGER.info(f"Creating new config saved in {self.file_path}")
        self.save()

        SETTINGS.CONFIG = self.file_path
        SETTINGS.save()
        
    def init_switchboard_settings(self, data={}):
        self.switchboard_settings = {
            "listener_exe": StringSetting(
                "listener_exe",
                "Listener Executable Name",
                 data.get('listener_exe', 'SwitchboardListener')
            )
        }
        
        self.LISTENER_EXE = self.switchboard_settings["listener_exe"]

    def init_project_settings(self, data={}):
        self.basic_project_settings = {
            "project_name": StringSetting(
                "project_name",
                "Project Name",
                data.get('project_name', 'Default')
            ),
            "uproject": FilePathSetting(
                "uproject", "uProject Path",
                data.get('uproject', ''),
                tool_tip="Path to uProject"
            ),
            "engine_dir": DirectoryPathSetting(
                "engine_dir",
                "Engine Directory",
                data.get('engine_dir', ''),
                tool_tip="Path to UE 'Engine' directory"
            ),
            "build_engine": BoolSetting(
                "build_engine",
                "Build Engine",
                data.get('build_engine', False),
                tool_tip="Is Engine built from source?"
            ),
            "maps_path": StringSetting(
                "maps_path",
                "Map Path",
                data.get('maps_path', ''),
                placeholder_text="Maps",
                tool_tip="Relative path from Content folder that contains maps to launch into."
            ),
            "maps_filter": StringSetting(
                "maps_filter",
                "Map Filter",
                data.get('maps_filter', '*.umap'),
                placeholder_text="*.umap",
                tool_tip="Walk every file in the Map Path and run a fnmatch to filter the file names"
            ),
            'maps_plugin_filters': StringListSetting(
                "maps_plugin_filters",
                "Map Plugin Filters",
                data.get('maps_plugin_filters', []),
                tool_tip="Plugins whose name matches any of these filters will also be searched for maps.",
                migrate_data=migrate_comma_separated_string_to_list
            ),
        }

        self.PROJECT_NAME = self.basic_project_settings["project_name"]
        self.UPROJECT_PATH = self.basic_project_settings["uproject"]
        self.ENGINE_DIR = self.basic_project_settings["engine_dir"]
        self.BUILD_ENGINE = self.basic_project_settings["build_engine"]
        self.MAPS_PATH = self.basic_project_settings["maps_path"]
        self.MAPS_FILTER = self.basic_project_settings["maps_filter"]
        self.MAPS_PLUGIN_FILTERS = self.basic_project_settings["maps_plugin_filters"]

        self.osc_settings = {
            "osc_server_port": IntSetting(
                "osc_server_port",
                "OSC Server Port",
                data.get('osc_server_port', 6000)
            ),
            "osc_client_port": IntSetting(
                "osc_client_port",
                "OSC Client Port",
                data.get('osc_client_port', 8000)
            )
        }

        self.OSC_SERVER_PORT = self.osc_settings["osc_server_port"]
        self.OSC_CLIENT_PORT = self.osc_settings["osc_client_port"]

        self.source_control_settings = {
            "p4_enabled": BoolSetting(
                "p4_enabled",
                "Perforce Enabled",
                data.get("p4_enabled", False),
                tool_tip="Toggle Perforce support for the entire application"
            ),
            "source_control_workspace": StringSetting(
                "source_control_workspace", "Workspace Name",
                data.get("source_control_workspace"),
                tool_tip="SourceControl Workspace/Branch"
            ),
            "p4_sync_path": PerforcePathSetting(
                "p4_sync_path",
                "Perforce Project Path",
                data.get("p4_sync_path", ''),
                placeholder_text="//UE/Project"
            ),
            "p4_engine_path": PerforcePathSetting(
                "p4_engine_path",
                "Perforce Engine Path",
                data.get("p4_engine_path", ''),
                placeholder_text="//UE/Project/Engine"
            )
        }

        self.P4_ENABLED = self.source_control_settings["p4_enabled"]
        self.SOURCE_CONTROL_WORKSPACE = self.source_control_settings["source_control_workspace"]
        self.P4_PROJECT_PATH = self.source_control_settings["p4_sync_path"]
        self.P4_ENGINE_PATH = self.source_control_settings["p4_engine_path"]

    def init_unreal_insights(self, data={}):
        self.unreal_insight_settings = {
            "tracing_enabled": BoolSetting(
                "tracing_enabled",
                "Unreal Insights Tracing State",
                data.get("tracing_enabled", False),
            ),
            "tracing_args": StringSetting(
                "tracing_args",
                "Unreal Insights Tracing Args",
                data.get('tracing_args', 'log,cpu,gpu,frame,bookmark,concert,messaging')
            ),
            "tracing_stat_events": BoolSetting(
                "tracing_stat_events",
                "Unreal Insights Tracing with Stat Events",
                data.get('tracing_stat_events', True)
            )
        }

        self.INSIGHTS_TRACE_ENABLE = self.unreal_insight_settings["tracing_enabled"]
        self.INSIGHTS_TRACE_ARGS = self.unreal_insight_settings["tracing_args"]
        self.INSIGHTS_STAT_EVENTS = self.unreal_insight_settings["tracing_stat_events"]

    def default_mu_server_name(self):
        ''' Returns default server name based on current settings '''
        return f'{self.PROJECT_NAME.get_value()}_MU_Server'

    def init_muserver(self, data={}):
        self.mu_settings = {
            "muserver_server_name": StringSetting(
                "muserver_server_name",
                "Server name",
                data.get('muserver_server_name', self.default_mu_server_name()),
                tool_tip="The name that will be given to the server"
            ),
            "muserver_command_line_arguments": StringSetting(
                "muserver_command_line_arguments",
                "Command Line Args",
                data.get('muserver_command_line_arguments', ''),
                tool_tip="Additional command line arguments to pass to multiuser"
            ),
            "muserver_endpoint": StringSetting(
                "muserver_endpoint",
                "Unicast Endpoint",
                data.get('muserver_endpoint', ':9030')
            ),
            "udpmessaging_multicast_endpoint": StringSetting(
                attr_name='udpmessaging_multicast_endpoint',
                nice_name='Multicast Endpoint',
                value=data.get('muserver_multicast_endpoint', '230.0.0.1:6666'),
                tool_tip=(
                    'Multicast group and port (-UDPMESSAGING_TRANSPORT_MULTICAST) '
                    'in the {address}:{port} endpoint format. The multicast group address '
                    'must be in the range 224.0.0.0 to 239.255.255.255.'),
            ),
            "multiuser_exe": StringSetting(
                "multiuser_exe",
                "Multiuser Executable Name",
                data.get('multiuser_exe', 'UnrealMultiUserServer')
            ),
            "multiuserslate_exe": StringSetting(
                "multiuserslate_exe",
                "Multiuser Slate Executable Name",
                data.get('multiuserslate_exe', 'UnrealMultiUserSlateServer')
            ),
            "muserver_archive_dir": DirectoryPathSetting(
                "muserver_archive_dir",
                "Directory for Saved Archives",
                data.get('muserver_archive_dir', '')
            ),
            "muserver_working_dir": DirectoryPathSetting(
                "muserver_working_dir",
                "Directory for Live Sessions",
                data.get('muserver_working_dir', '')
            ),
            "muserver_auto_launch": BoolSetting(
                "muserver_auto_launch",
                "Auto Launch",
                data.get('muserver_auto_launch', True)
            ),
            "muserver_slate_mode": BoolSetting(
                "muserver_slate_mode",
                "Launch Multi-user server in UI mode",
                data.get('muserver_slate_mode', True)
            ),
            "muserver_clean_history": BoolSetting(
                "muserver_clean_history",
                "Clean History",
                data.get('muserver_clean_history', False)
            ),
            "muserver_auto_build": BoolSetting(
                "muserver_auto_build",
                "Auto Build",
                data.get('muserver_auto_build', True)
            ),
            "muserver_auto_endpoint": BoolSetting(
                "muserver_auto_endpoint",
                "Auto Endpoint",
                data.get('muserver_auto_endpoint', True)
            ),
            "muserver_auto_join": BoolSetting(
                "muserver_auto_join",
                "Unreal Multi-user Server Auto-join",
                data.get('muserver_auto_join', True)
            )
        }

        self.MUSERVER_SERVER_NAME = self.mu_settings["muserver_server_name"]
        self.MUSERVER_COMMAND_LINE_ARGUMENTS = self.mu_settings["muserver_command_line_arguments"]
        self.MUSERVER_ENDPOINT = self.mu_settings["muserver_endpoint"]
        self.MUSERVER_MULTICAST_ENDPOINT = self.mu_settings["udpmessaging_multicast_endpoint"]
        self.MULTIUSER_SERVER_EXE = self.mu_settings["multiuser_exe"]
        self.MULTIUSER_SLATE_SERVER_EXE = self.mu_settings["multiuserslate_exe"]
        self.MUSERVER_AUTO_LAUNCH = self.mu_settings["muserver_auto_launch"]
        self.MUSERVER_SLATE_MODE = self.mu_settings["muserver_slate_mode"]
        self.MUSERVER_CLEAN_HISTORY = self.mu_settings["muserver_clean_history"]
        self.MUSERVER_AUTO_BUILD = self.mu_settings["muserver_auto_build"]
        self.MUSERVER_AUTO_ENDPOINT = self.mu_settings["muserver_auto_endpoint"]
        self.MUSERVER_AUTO_JOIN = self.mu_settings["muserver_auto_join"]
        self.MUSERVER_WORKING_DIR = self.mu_settings["muserver_working_dir"]
        self.MUSERVER_ARCHIVE_DIR = self.mu_settings["muserver_archive_dir"]

    def save_unreal_insights(self, data):
        data['tracing_enabled'] = self.INSIGHTS_TRACE_ENABLE.get_value()
        data['tracing_args'] = self.INSIGHTS_TRACE_ARGS.get_value()
        data['tracing_stat_events'] = self.INSIGHTS_STAT_EVENTS.get_value()

    def save_muserver(self, data):
        data["muserver_command_line_arguments"] = self.MUSERVER_COMMAND_LINE_ARGUMENTS.get_value()
        data["muserver_server_name"] = self.MUSERVER_SERVER_NAME.get_value()
        data["muserver_endpoint"] = self.MUSERVER_ENDPOINT.get_value()
        data["multiuser_exe"] = self.MULTIUSER_SERVER_EXE.get_value()
        data["multiuserslate_exe"] = self.MULTIUSER_SLATE_SERVER_EXE.get_value()
        data["muserver_auto_launch"] = self.MUSERVER_AUTO_LAUNCH.get_value()
        data["muserver_slate_mode"] = self.MUSERVER_SLATE_MODE.get_value()
        data["muserver_clean_history"] = self.MUSERVER_CLEAN_HISTORY.get_value()
        data["muserver_auto_build"] = self.MUSERVER_AUTO_BUILD.get_value()
        data["muserver_auto_endpoint"] = self.MUSERVER_AUTO_ENDPOINT.get_value()
        data["muserver_multicast_endpoint"] = self.MUSERVER_MULTICAST_ENDPOINT.get_value()
        data["muserver_auto_join"] = self.MUSERVER_AUTO_JOIN.get_value()
        data["muserver_archive_dir"] = self.MUSERVER_ARCHIVE_DIR.get_value()
        data["muserver_working_dir"] = self.MUSERVER_WORKING_DIR.get_value()

    def load_plugin_settings(self, device_type, settings):
        ''' Updates plugin settings values with those read from the config file.
        '''

        loaded_settings = self._plugin_data_from_config.get(device_type, [])

        if loaded_settings:
            for setting in settings:
                if setting.attr_name in loaded_settings:
                    setting.update_value(loaded_settings[setting.attr_name])
            del self._plugin_data_from_config[device_type]

    def register_plugin_settings(self, device_type, settings):

        self._plugin_settings[device_type] = settings

        for setting in settings:
            setting.signal_setting_changed.connect(lambda: self.save())
            setting.signal_setting_overridden.connect(
                self.on_device_override_changed)

    def register_device_settings(
            self, device_type, device_name, settings, overrides):
        self._device_settings[(device_type, device_name)] = (
            settings, overrides)

        for setting in settings:
            setting.signal_setting_changed.connect(lambda: self.save())

    def on_device_override_changed(self, device_name, old_value, override):
        # Only do a save operation when the device is known (has called
        # register_device_settings) otherwise it is still loading and we want
        # to avoid saving during device loading to avoid errors in the cfg
        # file.
        known_devices = [name for (_, name) in self._device_settings.keys()]
        if device_name in known_devices:
            self.save()

    def replace(self, new_config_path: typing.Union[str, pathlib.Path]):
        """
        Move the file.

        If a file already exists at the new path, it will be overwritten.
        """
        new_config_path = get_absolute_config_path(new_config_path)

        if self.file_path:
            new_config_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(self.file_path, new_config_path)

        self.file_path = new_config_path
        self.save()

    def save_as(self, new_config_path: typing.Union[str, pathlib.Path]):
        """
        Copy the file.

        If a file already exists at the new path, it will be overwritten.
        """
        new_config_path = get_absolute_config_path(new_config_path)

        new_project_name = new_config_path.stem

        if self.file_path:
            new_config_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src=self.file_path, dst=new_config_path)

            # Don't change project name if the user customized it
            if self.PROJECT_NAME.get_value() != self.file_path.stem:
                new_project_name = self.file_path.stem

        self.file_path = new_config_path

        self.PROJECT_NAME.update_value(new_project_name)

        self.save()

    def save(self):
        if not self.file_path or not self.saving_allowed:
            return

        data = {}

        # General settings
        data['project_name'] = self.PROJECT_NAME.get_value()
        data['uproject'] = self.UPROJECT_PATH.get_value()
        data['engine_dir'] = self.ENGINE_DIR.get_value()
        data['build_engine'] = self.BUILD_ENGINE.get_value()
        data["maps_path"] = self.MAPS_PATH.get_value()
        data["maps_filter"] = self.MAPS_FILTER.get_value()
        data["maps_plugin_filters"] = self.MAPS_PLUGIN_FILTERS.get_value()
        data["listener_exe"] = self.LISTENER_EXE.get_value()

        self.save_unreal_insights(data)

        # OSC settings
        data["osc_server_port"] = self.OSC_SERVER_PORT.get_value()
        data["osc_client_port"] = self.OSC_CLIENT_PORT.get_value()

        # Source Control Settings
        data["p4_enabled"] = self.P4_ENABLED.get_value()
        data["p4_sync_path"] = self.P4_PROJECT_PATH.get_value()
        data["p4_engine_path"] = self.P4_ENGINE_PATH.get_value()
        data["source_control_workspace"] = (
            self.SOURCE_CONTROL_WORKSPACE.get_value())

        self.save_muserver(data)

        # Current Level
        data["current_level"] = self.CURRENT_LEVEL

        # Devices
        data["devices"] = {}

        # Plugin settings
        for device_type, plugin_settings in self._plugin_settings.items():

            if not plugin_settings:
                continue

            settings = {}

            for setting in plugin_settings:
                settings[setting.attr_name] = setting.get_value()

            data["devices"][device_type] = {
                "settings": settings,
            }

        # Device settings
        for (device_type, device_name), (settings, overrides) in \
                self._device_settings.items():

            if device_type not in data["devices"].keys():
                data["devices"][device_type] = {}

            serialized_settings = {}

            for setting in settings:
                serialized_settings[setting.attr_name] = setting.get_value()

            for setting in overrides:
                if setting.is_overridden(device_name):
                    serialized_settings[setting.attr_name] = setting.get_value(
                        device_name)

            data["devices"][device_type][device_name] = serialized_settings

        # Save to file
        #
        self.file_path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.file_path, 'w') as f:
            json.dump(data, f, indent=4)
            LOGGER.debug(f'Config File: {self.file_path} updated')

    def on_device_name_changed(self, old_name, new_name):
        old_key = None

        # update the entry in device_settings as they are identified by name
        for (device_type, device_name), (_, overrides) in \
                self._device_settings.items():
            if device_name == old_name:
                old_key = (device_type, old_name)
                # we also need to patch the overrides for the same reason
                for setting in overrides:
                    setting.on_device_name_changed(old_name, new_name)
                break

        new_key = (old_key[0], new_name)
        self._device_settings[new_key] = self._device_settings.pop(old_key)

        self.save()

    def on_device_removed(self, _, device_type, device_name, update_config):
        if not update_config:
            return

        del self._device_settings[(device_type, device_name)]
        self.save()

    def shrink_path(self, path):
        path_name = path.replace(self.get_project_dir(), '', 1)
        path_name = path_name.replace(os.sep, '/')
        return path_name

    def get_project_dir(self) -> str:
        '''
        Get the root directory of the project.

        This is the directory in which the .uproject file lives.
        '''
        return os.path.dirname(self.UPROJECT_PATH.get_value().replace('"', ''))

    def get_project_content_dir(self) -> str:
        '''
        Get the "Content" directory of the project.
        '''
        return os.path.join(self.get_project_dir(), 'Content')

    def get_project_plugins_dir(self) -> str:
        '''
        Get the "Plugins" directory of the project where all of the
        project-based plugins are located.
        '''
        return os.path.join(self.get_project_dir(), 'Plugins')

    def get_project_plugin_names(self) -> typing.List[str]:
        '''
        Get a list of the names of all project-based plugins in the project.
        '''
        project_plugins_path = os.path.normpath(self.get_project_plugins_dir())
        if not os.path.isdir(project_plugins_path):
            return []

        plugin_names = [
            x.name for x in os.scandir(project_plugins_path) if x.is_dir()]

        return plugin_names

    def get_project_plugin_content_dir(self, plugin_name: str) -> str:
        '''
        Get the "Content" directory of the project-based plugin with the
        given name.
        '''
        return os.path.join(
            self.get_project_plugins_dir(), plugin_name, 'Content')

    def plugin_name_matches(self, plugin_name: str, name_filters: typing.List[str]) -> bool:
        '''
        Test whether a plugin name matches any of the provided filters.

        Returns True if any one of the filters matches, or False otherwise.
        '''
        for name_filter in name_filters:
            if fnmatch.fnmatch(plugin_name, name_filter):
                return True

        return False

    def resolve_content_path(self, file_path: str, plugin_name: str = None) -> str:
        '''
        Resolve a file path on the file system to the corresponding content
        path in UE.

        If a plugin_name is provided, the file is assumed to live inside that
        plugin and its content path will have the appropriate plugin
        name-based prefix. Otherwise, the file is assumed to live inside the
        project's content folder.
        '''
        if plugin_name:
            content_dir = self.get_project_plugin_content_dir(plugin_name)
            ue_path_prefix = f'/{plugin_name}'
        else:
            content_dir = self.get_project_content_dir()
            ue_path_prefix = '/Game'

        path_name = file_path.replace(content_dir, ue_path_prefix, 1)
        path_name = self.shrink_path(path_name)

        return path_name

    def maps(self):
        '''
        Returns a list of full map paths in an Unreal Engine project and
        in project-based plugins such as:
            [
                "/Game/Maps/MapName",
                "/MyPlugin/Levels/MapName"
            ]
        The slashes will always be "/" independent of the platform's separator.
        '''
        content_maps_path = os.path.normpath(
            os.path.join(
                self.get_project_content_dir(),
                self.MAPS_PATH.get_value()))

        # maps_paths stores a list of tuples of the form
        # (plugin_name, file_path). This allows us to differentiate between
        # maps in the project and maps in a plugin.
        maps_paths = [(None, content_maps_path)]

        maps_plugin_filters = self.MAPS_PLUGIN_FILTERS.get_value()
        if maps_plugin_filters:
            plugin_names = self.get_project_plugin_names()

            for plugin_name in plugin_names:
                if self.plugin_name_matches(plugin_name, maps_plugin_filters):
                    maps_paths.append(
                        (plugin_name, self.get_project_plugin_content_dir(plugin_name)))

        maps = []
        for (plugin_name, maps_path) in maps_paths:
            for dirpath, b, file_names in os.walk(maps_path):
                for file_name in file_names:
                    if not fnmatch.fnmatch(file_name, self.MAPS_FILTER.get_value()):
                        continue

                    map_name, _ = os.path.splitext(file_name)
                    file_path_to_map = os.path.join(dirpath, map_name)

                    content_path_to_map = self.resolve_content_path(
                        file_path_to_map, plugin_name=plugin_name)

                    if content_path_to_map not in maps:
                        maps.append(content_path_to_map)

        maps.sort()
        return maps

    def multiuser_server_path(self):
        if self.MUSERVER_SLATE_MODE.get_value():
            return self.engine_exe_path(
                self.ENGINE_DIR.get_value(), self.MULTIUSER_SLATE_SERVER_EXE.get_value())
        else:
            return self.engine_exe_path(
                self.ENGINE_DIR.get_value(), self.MULTIUSER_SERVER_EXE.get_value())

    def multiuser_server_session_directory_path(self):
        if self.MUSERVER_WORKING_DIR.get_value():
            return self.MUSERVER_WORKING_DIR.get_value()

        if self.MUSERVER_SLATE_MODE.get_value():
            return os.path.join(self.ENGINE_DIR.get_value(), "Programs", "UnrealMultiUserSlateServer", "Intermediate", "MultiUser")

        return os.path.join(self.ENGINE_DIR.get_value(), "Programs", "UnrealMultiUserServer", "Intermediate", "MultiUser")

    def multiuser_server_log_path(self):
        if self.MUSERVER_SLATE_MODE.get_value():
            return os.path.join(self.ENGINE_DIR.get_value(), "Programs", "UnrealMultiUserSlateServer", "Saved", "Logs", "UnrealMultiUserSlateServer.log")
        # else we get the path to the console server.
        return os.path.join(self.ENGINE_DIR.get_value(), "Programs", "UnrealMultiUserServer", "Saved", "Logs", "UnrealMultiUserServer.log")

    def listener_path(self):
        return self.engine_exe_path(
            self.ENGINE_DIR.get_value(), self.LISTENER_EXE.get_value())

    # todo-dara: find a way to do this directly in the LiveLinkFace plugin code
    def unreal_device_addresses(self):
        unreal_addresses = []
        for (device_type, device_name), (settings, overrides) in \
                self._device_settings.items():
            if device_type == "Unreal":
                for setting in settings:
                    if setting.attr_name == "address":
                        unreal_addresses.append(setting.get_value(device_name))
        return unreal_addresses

    @staticmethod
    def engine_exe_path(engine_dir: str, exe_basename: str):
        '''
        Returns platform-dependent path to the specified engine executable.
        '''
        exe_name = exe_basename
        platform_bin_subdir = ''

        if sys.platform.startswith('win'):
            platform_bin_subdir = 'Win64'
            platform_bin_path = os.path.normpath(
                os.path.join(engine_dir, 'Binaries', platform_bin_subdir))
            given_path = os.path.join(platform_bin_path, exe_basename)
            if os.path.exists(given_path):
                return given_path

            # Use %PATHEXT% to resolve executable extension ambiguity.
            pathexts = os.environ.get(
                'PATHEXT', '.COM;.EXE;.BAT;.CMD').split(';')
            for ext in pathexts:
                testpath = os.path.join(
                    platform_bin_path, f'{exe_basename}{ext}')
                if os.path.isfile(testpath):
                    return testpath

            # Fallback despite non-existence.
            return given_path
        else:
            if sys.platform.startswith('linux'):
                platform_bin_subdir = 'Linux'
            elif sys.platform.startswith('darwin'):
                platform_bin_subdir = 'Mac'

            return os.path.normpath(
                os.path.join(
                    engine_dir, 'Binaries', platform_bin_subdir, exe_name))


class UserSettings(object):
    def init(self):
        try:
            with open(USER_SETTINGS_FILE_PATH) as f:
                LOGGER.debug(f'Loading Settings {USER_SETTINGS_FILE_PATH}')
                data = json.load(f)
        except FileNotFoundError:
            # Create a default user_settings
            LOGGER.debug('Creating default user settings')
            data = {}
        except ValueError:
            LOGGER.error(f'{USER_SETTINGS_FILE_NAME} has invalid JSON format. ')
            
            answer = QtWidgets.QMessageBox.question(
                None,
                'Invalid User Settings',
                 f'User settings has invalid JSON format and will be replaced with a valid a new default JSON.'
                 f'\n\nDo you want to save a backup named {USER_SETTINGS_BACKUP_FILE_NAME} (overrides existing)?',
                 QtWidgets.QMessageBox.Yes | QtWidgets.QMessageBox.No
            )
            if answer == QtWidgets.QMessageBox.Yes:
                if os.path.exists(USER_SETTINGS_BACKUP_FILE_PATH):
                    os.remove(USER_SETTINGS_BACKUP_FILE_PATH)
                shutil.copy(USER_SETTINGS_FILE_PATH, USER_SETTINGS_BACKUP_FILE_PATH)
                
            data = {}

        self.CONFIG = data.get('config')
        if self.CONFIG:
            try:
                self.CONFIG = get_absolute_config_path(self.CONFIG)
            except ConfigPathError as e:
                LOGGER.error(e)
                self.CONFIG = None

        if not self.CONFIG:
            config_paths = list_config_paths()
            self.CONFIG = config_paths[0] if config_paths else None

        # Address of the machine running Switchboard
        self.ADDRESS = LocalAddressSetting(
            "address",
            "Address",
            data.get("address", socket.gethostbyname(socket.gethostname()))
        )
        self.TRANSPORT_PATH = FilePathSetting(
                "transport_path",
                "Transport path",
                data.get('transport_path', '')
            )
        

        # UI Settings
        self.MUSERVER_SESSION_NAME = data.get(
            'muserver_session_name', 'MU_Session').replace(' ','_')
        self.CURRENT_SEQUENCE = data.get('current_sequence', 'Default')
        self.CURRENT_SLATE = data.get('current_slate', 'Scene')
        self.CURRENT_TAKE = data.get('current_take', 1)
        self.CURRENT_LEVEL = data.get('current_level', None)
        self.LAST_BROWSED_PATH = data.get('last_browsed_path', None)

        # Save so any new defaults are written out
        self.save()

    def save(self):
        data = {
            'config': '',
            'address': self.ADDRESS.get_value(),
            'transport_path': self.TRANSPORT_PATH.get_value(),
            'muserver_session_name': self.MUSERVER_SESSION_NAME,
            'current_sequence': self.CURRENT_SEQUENCE,
            'current_slate': self.CURRENT_SLATE,
            'current_take': self.CURRENT_TAKE,
            'current_level': self.CURRENT_LEVEL,
            'last_browsed_path': self.LAST_BROWSED_PATH,
        }

        if self.CONFIG:
            try:
                data['config'] = str(get_relative_config_path(self.CONFIG))
            except ConfigPathError as e:
                LOGGER.error(e)
            except ValueError as e:
                data['config'] = str(self.CONFIG)

        with open(USER_SETTINGS_FILE_PATH, 'w') as f:
            json.dump(data, f, indent=4)


def list_config_paths() -> typing.List[pathlib.Path]:
    '''
    Returns a list of absolute paths to all config files in the configs dir.
    '''
    ROOT_CONFIGS_PATH.mkdir(parents=True, exist_ok=True)

    # Find all JSON files in the config dir recursively, but exclude the user
    # settings file.
    config_paths = [
        path for path in ROOT_CONFIGS_PATH.rglob(f'*{CONFIG_SUFFIX}')
        if path != USER_SETTINGS_FILE_PATH and path != USER_SETTINGS_BACKUP_FILE_PATH]

    return config_paths


# Get the user settings and load their config
SETTINGS = UserSettings()
CONFIG = Config()
    
