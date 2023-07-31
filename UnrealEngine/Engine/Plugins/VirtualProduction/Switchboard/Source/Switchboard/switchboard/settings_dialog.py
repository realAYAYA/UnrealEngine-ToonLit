# Copyright Epic Games, Inc. All Rights Reserved.

import os
import pathlib
import typing

from PySide2 import QtCore
from PySide2 import QtGui
from PySide2 import QtUiTools
from PySide2 import QtWidgets

from switchboard import config
from switchboard import switchboard_widgets as sb_widgets
from switchboard.config import CONFIG, SETTINGS, Config, UserSettings, Setting
from switchboard.settings_search import SettingsSearch
from switchboard.switchboard_widgets import CollapsibleGroupBox, DropDownMenuComboBox
from switchboard.ui.horizontal_tabs import HorizontalTabWidget

RELATIVE_PATH = os.path.dirname(__file__)


def clear_widgets(layout):
    for i in range(layout.count()):
        layout.takeAt(0) 

COLLAPSE_ALL_TEXT = "Collapse all"
EXPAND_ALL_TEXT = "Expand all"

class SettingsDialog(QtCore.QObject):
    def __init__(self, settings: UserSettings, config: Config):
        super().__init__()
        self.plugin_widgets = {}

        # Set the UI object
        self.ui = QtWidgets.QDialog()
        self.ui.resize(600, 800)
        dialog_layout = QtWidgets.QVBoxLayout(self.ui)
        dialog_layout.setContentsMargins(2, 2, 2, 2)
        self.ui.setWindowTitle("Settings")
        self.ui.finished.connect(
            lambda result, config=config:
                self._on_finished(result, config)
        )

        self._create_search_area(dialog_layout)
        self.general_settings_list = [
            self._create_config_path_settings(),
            self._create_switchboard_settings(settings, config),
            self._create_project_settings(config),
            self._create_multi_user_server_settings(config)
        ]
        
        self._create_tab_widget(dialog_layout)
        
        self.settings_search = SettingsSearch([self.ui.all_settings_scroll_area, self.ui.general_settings_scroll_area])
        self.ui.searchBar.textChanged.connect(
            self._on_search_text_edited)
        
    def select_all_tab(self):
        self._on_tab_changed(0)
        
    def _on_tab_changed(self, index: int):
        """
        Because of the All category, widgets need to be re-parented constantly.
        """
        clear_widgets(self.ui.all_settings_scroll_area.layout())
        clear_widgets(self.ui.general_settings_scroll_area.layout())
        
        is_all_category = index == 0
        if is_all_category:
            for general_setting_widget in self.general_settings_list:
                self.ui.all_settings_scroll_area.layout().addWidget(general_setting_widget)
               
            for (group_box, scroll_bar) in self.plugin_widgets.values():
                scroll_bar.takeWidget()
                self.ui.all_settings_scroll_area.layout().addWidget(group_box)
                
            # Pull all widgets up so they do not attempt to divide space among themselves when filtered by search
            self.ui.all_settings_scroll_area.layout().addItem(
                QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding)
            )
        
        # General settings may have been in All
        is_general_category = index == 1
        if is_general_category:
            for general_setting_widget in self.general_settings_list:
                self.ui.general_settings_scroll_area.layout().addWidget(general_setting_widget)
            # Pull all widgets up so they do not attempt to divide space among themselves when filtered by search
            self.ui.general_settings_scroll_area.layout().addItem(
                QtWidgets.QSpacerItem(20, 40, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding)
            )
           
        # Add the plugin back to its tabs - it may have been in All
        is_plugin = not is_all_category and not is_general_category 
        if is_plugin:
            plugin_name = self.ui.tab_widget.tabText(index)
            (group_box, scroll_bar) = self.plugin_widgets[plugin_name]
            is_plugin_widget_missing = scroll_bar.widget() is None
            if is_plugin_widget_missing:
                scroll_bar.setWidget(group_box)
        
        self._search_settings_again()
        return

    def _on_finished(self, result: int, config: Config):
        config.P4_ENABLED.signal_setting_changed.disconnect(self._on_source_control_setting_changed)
        
        # Currently, the only way to dismiss the settings dialog is by using
        # the close button as opposed to ok/cancel buttons, so we intercept the
        # close to issue a warning if the config path was changed and we're
        # about to overwrite some other existing config file.
        if (self._changed_config_path in self._config_paths
                and self._changed_config_path != self._current_config_path):
            # Show the confirmation dialog using a relative path to the config.
            rel_config_path = config.get_relative_config_path(
                self._changed_config_path)
            reply = QtWidgets.QMessageBox.question(
                self.ui, 'Confirm Overwrite',
                ('Are you sure you would like to change the config path and '
                 f'overwrite the existing config file "{rel_config_path}"?'),
                QtWidgets.QMessageBox.Yes, QtWidgets.QMessageBox.No)

            if reply != QtWidgets.QMessageBox.Yes:
                # Clear the changed config path so that the dialog returns the
                # originally specified path when queried via config_path().
                self._changed_config_path = None

    def _search_settings_again(self):
        self._on_search_text_edited(self.ui.searchBar.text())

    def _on_search_text_edited(self, search_string: str):
        self.settings_search.search(search_string)
        
    def _create_search_area(self, dialog_layout):
        self.ui.search_area = QtWidgets.QWidget()
        search_layout = QtWidgets.QHBoxLayout(self.ui.search_area)
        search_layout.setContentsMargins(1, 1, 1, 1)
        
        self.ui.searchBar = QtWidgets.QLineEdit()
        self.ui.searchBar.setPlaceholderText("Search")
        search_layout.addWidget(self.ui.searchBar)

        pixmap = QtGui.QPixmap(":icons/images/view_button.png")
        self.ui.view_options = DropDownMenuComboBox(QtGui.QIcon(pixmap))
        self.ui.view_options.addItem(COLLAPSE_ALL_TEXT)
        self.ui.view_options.addItem(EXPAND_ALL_TEXT)
        
        self.ui.view_options.on_select_option.connect(self._on_view_option_selected)
        search_layout.addWidget(self.ui.view_options)
        dialog_layout.addWidget(self.ui.search_area)

    def _on_view_option_selected(self, selected_item):
        if selected_item == COLLAPSE_ALL_TEXT:
            self._set_categories_expanded(False)
        elif selected_item == EXPAND_ALL_TEXT:
            self._set_categories_expanded(True)
            
    def _set_categories_expanded(self, should_expand: bool):
        def _set_expanded_recursive(widget_or_layout, should_expand: bool):
            if isinstance(widget_or_layout, CollapsibleGroupBox):
                widget_or_layout.set_expanded(should_expand)
            
            layout = widget_or_layout
            if isinstance(widget_or_layout, QtWidgets.QWidget):
                if isinstance(widget_or_layout, QtWidgets.QScrollArea):
                    layout = widget_or_layout.widget().layout()
                else:
                    layout = widget_or_layout.layout()
                
            if layout is None:
                return
            
            for i in range(layout.count()):
                layout_item = layout.itemAt(i)
                if layout_item.widget():
                    _set_expanded_recursive(layout_item.widget(), should_expand)
                elif layout_item.layout():
                    _set_expanded_recursive(layout_item.layout(), should_expand)
            
        active_tab_content = self.ui.tab_widget.currentWidget()
        _set_expanded_recursive(active_tab_content, should_expand)
        
    def _create_config_path_settings(self):
        self.ui.config_path_layout = QtWidgets.QWidget()
        layout = QtWidgets.QHBoxLayout(self.ui.config_path_layout)
        
        self.ui.config_path_label = QtWidgets.QLabel()
        self.ui.config_path_label.setText("Config Path")
        
        self.ui.config_path_line_edit = QtWidgets.QLineEdit()
        self.ui.config_path_line_edit.textChanged.connect(
            self.config_path_text_changed)

        layout.addWidget(self.ui.config_path_label)
        layout.addWidget(self.ui.config_path_line_edit)

        # Store the current config paths so we can warn about overwriting an existing config.
        self._config_paths = config.list_config_paths()
        self.set_config_path(SETTINGS.CONFIG)
        
        return self.ui.config_path_layout
        
    def _create_switchboard_settings(self, settings: UserSettings, config: Config):
        self.ui.switchboard_settings_group = CollapsibleGroupBox()
        self.ui.switchboard_settings_group.setTitle("Switchboard")
        layout = QtWidgets.QFormLayout(self.ui.switchboard_settings_group)

        settings.ADDRESS.create_ui(form_layout=layout)
        settings.TRANSPORT_PATH.create_ui(form_layout=layout)
        config.LISTENER_EXE.create_ui(form_layout=layout)

        return self.ui.switchboard_settings_group
        
    def _create_project_settings(self, config: Config):
        self.ui.project_settings_group = CollapsibleGroupBox()
        self.ui.project_settings_group.setTitle("Project Settings")
        layout = QtWidgets.QVBoxLayout(self.ui.project_settings_group)
        
        project_settings_root = QtWidgets.QWidget()
        form_layout = QtWidgets.QFormLayout(project_settings_root)
        self._create_settings_section(config.basic_project_settings, form_layout)
        layout.addWidget(project_settings_root)
        
        # Sub settings
        self._create_osc_settings(config, layout)
        self._create_source_control_settings(config, layout)
        
        return self.ui.project_settings_group
    
    def _create_osc_settings(self, config: Config, layout: QtWidgets.QFormLayout):
        self.ui.osc_settings_group = QtWidgets.QGroupBox()
        self.ui.osc_settings_group.setTitle("OSC")
        form_layout = QtWidgets.QFormLayout(self.ui.osc_settings_group)
        layout.addWidget(self.ui.osc_settings_group)
        self._create_settings_section(config.osc_settings, form_layout)
    
    def _create_source_control_settings(self, config: Config, layout: QtWidgets.QFormLayout):
        self.ui.source_control_settings_group = QtWidgets.QGroupBox()
        self.ui.source_control_settings_group.setTitle("Source Control")
        self.ui.source_control_settings_group.setCheckable(True)
        self.ui.source_control_settings_group.setStyleSheet(
            'QGroupBox::indicator:checked:hover {image: url(:icons/images/check_box_checked_hovered.png);}'
            'QGroupBox::indicator:checked {image: url(:icons/images/check_box_checked.png);}'
            'QGroupBox::indicator:unchecked:hover {image: url(:icons/images/check_box_hovered.png);}'
            'QGroupBox::indicator:unchecked {image: url(:icons/images/check_box.png);}'
        )
        form_layout = QtWidgets.QFormLayout(self.ui.source_control_settings_group)
        layout.addWidget(self.ui.source_control_settings_group)

        # Source control setting UI is fully enabled / disabled depending on P4_ENABLED
        self.ui.source_control_settings_group.setChecked(config.P4_ENABLED.get_value())
        self.ui.source_control_settings_group.toggled.connect(self._on_group_source_control_changed)
        config.P4_ENABLED.signal_setting_changed.connect(self._on_source_control_setting_changed)
        settings_to_show = {key: setting for (key, setting) in config.source_control_settings.items() if setting != config.P4_ENABLED}

        self._create_settings_section(settings_to_show, form_layout)

    def _on_group_source_control_changed(self):
        CONFIG.P4_ENABLED.update_value(self.ui.source_control_settings_group.isChecked())
        CONFIG.save();

    def _on_source_control_setting_changed(self, old_value: bool, new_value: bool):
        self.ui.source_control_settings_group.setChecked(new_value)

    def _create_multi_user_server_settings(self, config: Config):
        self.ui.multi_user_settings = CollapsibleGroupBox()
        self.ui.multi_user_settings.setTitle("Multi User Server")
        form_layout = QtWidgets.QFormLayout(self.ui.multi_user_settings)
        
        self._create_settings_section(config.mu_settings, form_layout)
        return self.ui.multi_user_settings
    
    def _create_settings_section(self, dict: typing.Dict[str, Setting], layout: QtWidgets.QFormLayout):
        setting_list = [setting for _, setting in dict.items()]
        for setting in setting_list:
            setting.create_ui(override_device_name=None, form_layout=layout)

    def _create_tab_widget(self, parent_layout):
        self.ui.tab_widget = HorizontalTabWidget()
        self.ui.tab_widget.setTabPosition(QtWidgets.QTabWidget.West)
        parent_layout.addWidget(self.ui.tab_widget)
    
        # All tab
        self.ui.all_tab_root = QtWidgets.QScrollArea()
        self.ui.all_tab_root.setWidgetResizable(True)
        self.ui.all_tab_root.setSizeAdjustPolicy(QtWidgets.QAbstractScrollArea.AdjustToContents)
        self.ui.all_tab_root.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)

        self.ui.all_settings_scroll_area = QtWidgets.QWidget()
        all_layout = QtWidgets.QVBoxLayout(self.ui.all_settings_scroll_area)
        all_layout.setContentsMargins(2, 2, 2, 2)
        self.ui.all_tab_root.setWidget(self.ui.all_settings_scroll_area)
    
        # General tab
        self.ui.general_tab_root = QtWidgets.QScrollArea()
        self.ui.general_tab_root.setWidgetResizable(True)
        self.ui.general_tab_root.setSizeAdjustPolicy(QtWidgets.QAbstractScrollArea.AdjustToContents)
        self.ui.general_tab_root.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        
        self.ui.general_settings_scroll_area = QtWidgets.QWidget()
        general_layout = QtWidgets.QVBoxLayout(self.ui.general_settings_scroll_area)
        general_layout.setContentsMargins(2, 2, 2, 2)
        self.ui.general_tab_root.setWidget(self.ui.general_settings_scroll_area)

        # Register tabs after all widgets have been allocated
        self.ui.tab_widget.addTab(self.ui.all_tab_root, "All")
        self.ui.tab_widget.addTab(self.ui.general_tab_root, "General")
        # Register callback last because addTab should not trigger our callback before everything is initialized
        self.ui.tab_widget.currentChanged.connect(
            self._on_tab_changed
        )

    def config_path(self):
        if self._changed_config_path:
            return self._changed_config_path

        return self._current_config_path

    def set_config_path(self, config_path: pathlib.Path):
        self._current_config_path = config_path
        self._changed_config_path = None

        if not config_path:
            self._changed_config_path = config.Config.DEFAULT_CONFIG_PATH
            config_path_str = self._changed_config_path.stem
        else:
            # prefer relative path when possible
            try:
                config_path_str = str(config.get_relative_config_path(config_path).with_suffix(''))
            except ValueError:
                config_path_str = str(config_path)

        self.ui.config_path_line_edit.setText(config_path_str)

    def config_path_text_changed(self, config_path_str):
        config_path = config.Config.DEFAULT_CONFIG_PATH

        sb_widgets.set_qt_property(
            self.ui.config_path_line_edit, "input_error", False)

        try:
            config_path = config.get_absolute_config_path(config_path_str)
        except Exception as e:
            sb_widgets.set_qt_property(
                self.ui.config_path_line_edit, "input_error", True)

            rect = self.ui.config_path_line_edit.parent().mapToGlobal(
                self.ui.config_path_line_edit.geometry().topRight())
            QtWidgets.QToolTip().showText(rect, str(e))

        self._changed_config_path = config_path

    # Devices
    def add_section_for_plugin(self, plugin_name, plugin_settings, device_settings):
        any_device_settings = (
            any([device[1] for device in device_settings]) or
            any([device[2] for device in device_settings]))
        if not any_device_settings:
            return  # no settings to show

        # Create a group box per plugin
        device_override_group_box = CollapsibleGroupBox()
        device_override_group_box.setTitle(f'{plugin_name} Settings')
        device_override_group_box.setLayout(QtWidgets.QVBoxLayout())
        device_override_group_box.setSizePolicy(
            QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Maximum)
        )

        # Need to manually create scroll bar for tab
        scroll_bar = QtWidgets.QScrollArea()
        scroll_bar.setWidget(device_override_group_box)
        scroll_bar.setWidgetResizable(True)
        scroll_bar.setSizeAdjustPolicy(QtWidgets.QAbstractScrollArea.AdjustIgnored)
        scroll_bar.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        scroll_layout = QtWidgets.QVBoxLayout(scroll_bar)
        scroll_layout.setContentsMargins(1, 1, 1, 1)
        
        self.ui.tab_widget.addTab(scroll_bar, f'{plugin_name}')
        self.plugin_widgets[plugin_name] = (device_override_group_box, scroll_bar)
        self.settings_search.searched_widgets.append(device_override_group_box)

        plugin_layout = QtWidgets.QFormLayout()
        device_override_group_box.layout().addLayout(plugin_layout)

        # add widgets for settings shared by all devices of a plugin
        for setting in plugin_settings:
            setting.create_ui(form_layout=plugin_layout)

        # add widgets for settings and overrides of individual devices
        for device_name, settings, overrides in device_settings:
            group_box = CollapsibleGroupBox()
            group_box.setTitle(device_name)
            group_box.setLayout(QtWidgets.QVBoxLayout())

            device_override_group_box.layout().addWidget(group_box)

            layout = QtWidgets.QFormLayout()
            group_box.layout().addLayout(layout)

            # regular "instance" settings
            for setting in settings:
                setting.create_ui(form_layout=layout)

            for setting in overrides:
                setting.create_ui(
                    form_layout=layout, override_device_name=device_name)

        # Pull all widgets up so they do not attempt to divide space among themselves when filtered by search
        device_override_group_box.layout().addItem(
            QtWidgets.QSpacerItem(0, 0, QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Expanding)
        )
