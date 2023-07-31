# Copyright Epic Games, Inc. All Rights Reserved.

import datetime
import logging
import os
import threading
import time
import re
import shutil
from typing import List, Optional, Set, Union

from pathlib import Path

from PySide2 import QtCore
from PySide2 import QtGui
from PySide2 import QtUiTools
from PySide2 import QtWidgets

from PySide2.QtWidgets import QWidgetAction, QMenu

from switchboard import config
from switchboard import config_osc as osc
from switchboard import p4_utils
from switchboard import recording
from switchboard import resources  # noqa
from switchboard import switchboard_application
from switchboard import switchboard_utils
from switchboard import switchboard_widgets as sb_widgets
from switchboard.add_config_dialog import AddConfigDialog
from switchboard.config import CONFIG, DEFAULT_MAP_TEXT, SETTINGS
from switchboard.device_list_widget import DeviceListWidget, DeviceWidgetHeader
from switchboard.devices.device_base import DeviceStatus
from switchboard.devices.device_manager import DeviceManager
from switchboard.settings_dialog import SettingsDialog
from switchboard.switchboard_logging import ConsoleStream, LOGGER
from switchboard.tools.insights_launcher import InsightsLauncher
from switchboard.tools.listener_launcher import ListenerLauncher
from switchboard.devices.unreal.plugin_unreal import DeviceUnreal
from switchboard.util import collect_logs

ENGINE_PATH = "../../../../.."
RELATIVE_PATH = os.path.dirname(__file__)
EMPTY_SYNC_ENTRY = "-- None --"

class TraceSettings(QtWidgets.QDialog):
    """
    Custom class to prompt user for the trace arguments to use for Unreal Insights trace collection.
    """
    def __init__(self, trace_settings, parent=None):
        super().__init__(parent=parent, f=QtCore.Qt.WindowCloseButtonHint)

        self.setWindowTitle("Trace Settings")

        self.arguments_field = QtWidgets.QLineEdit(self)
        self.arguments_field.setText(trace_settings)

        self.form_layout = QtWidgets.QFormLayout()
        self.form_layout.addRow("Arguments", self.arguments_field)

        layout = QtWidgets.QVBoxLayout()
        layout.insertLayout(0, self.form_layout)

        button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        button_box.accepted.connect(lambda: self.accept())
        button_box.rejected.connect(lambda: self.reject())
        layout.addWidget(button_box)

        self.setLayout(layout)

    def trace_settings(self):
        """
        Return the current trace settings.
        """
        return self.arguments_field.text()

class DeviceAdditionalSettingsUI(QtCore.QObject):
    signal_device_widget_tracing = QtCore.Signal(object)

    enable_insight_trace = False
    insight_trace_args = "log,cpu,gpu,frame,bookmark,concert,messaging"

    def __init__(self, name, parent = None):
        super().__init__(parent)
        self.name = name

    def _update_enable_state(self):
        """
        The button assigned for the settings UI should be "checked" when the user has traces enabled.
        """
        self._button.setChecked(self.enable_insights_menu().isChecked())

    def set_insight_trace_state(self, is_checked):
        """
        External setter for the current tracing state.  This is applied by the device one loading the setting from the global
        configuration.
        """
        if is_checked is not self.enable_insights_menu().isChecked():
            self.enable_insights_menu().setChecked(is_checked)
            self._update_enable_state()

    def set_insight_tracing_args(self, value):
        """
        External setter for tracing arguments. This is applied by the device one loading the setting from the global
        configuration.
        """
        if value != self.insight_trace_args:
            self.insight_trace_args = value
            self._update_enable_state()

    def enable_insights_menu(self):
        return self._tracing_action

    def trace_settings(self):
        return (self.enable_insights_menu().isChecked(), self.insight_trace_args)

    def _set_tracing(self, is_checked):
        self.signal_device_widget_tracing.emit(self)
        self._update_enable_state()

    def _set_trace_settings(self, is_checked):
        self._tracing_settings.setChecked(False)
        settings = TraceSettings(self.insight_trace_args)
        settings.exec()
        self.insight_trace_args = settings.trace_settings()
        self.signal_device_widget_tracing.emit(self)
        self._update_enable_state()

    def _generate_trace_menu_item(self, menu):
        action_item = QtWidgets.QAction("Enable Unreal Insights Tracing",
                                        menu, checked=DeviceAdditionalSettingsUI.enable_insight_trace, checkable=True)
        action_item.setChecked(DeviceAdditionalSettingsUI.enable_insight_trace)
        action_item.triggered.connect(self._set_tracing)
        self._tracing_action = action_item
        return action_item

    def _generate_trace_settings_item(self, menu):
        action_item = QtWidgets.QAction("Unreal Insights Trace Settings...",
                                        menu, checked=DeviceAdditionalSettingsUI.enable_insight_trace, checkable=True)
        action_item.setChecked(DeviceAdditionalSettingsUI.enable_insight_trace)
        action_item.triggered.connect(self._set_trace_settings)
        self._tracing_settings = action_item
        return action_item

    def _generate_settings_menu_items(self, menu):
        self.settings_menu.addAction(self._generate_trace_menu_item(menu))
        self.settings_menu.addAction(self._generate_trace_settings_item(menu))

    def get_button(self):
        return self._button

    def assign_button(self, button, parent):
        self._button = button
        self.settings_menu = QtWidgets.QMenu(parent)
        self._generate_settings_menu_items(self.settings_menu)
        self._button.setMenu(self.settings_menu)
        self._button.setStyleSheet("QPushButton::menu-indicator{image:none;}");
        self._button.setDisabled(False)

    def make_button(self, parent):
        """
        Make a new device setting push button.
        """
        button = sb_widgets.ControlQPushButton.create(
                icon_size=QtCore.QSize(21, 21),
                tool_tip=f'Change device settings',
                hover_focus=False,
                name='settings')

        self.assign_button(button,parent)
        return button

class PeriodicRunnable(QtCore.QRunnable):
    ''' Performs periodic tasks on the switchboard dialog. '''

    def __init__(self, switchboard):
        super().__init__()
        self._switchboard = switchboard
        self._exiting = False

    def exit(self):
        self._exiting = True

    def run(self):
        while not self._exiting:
            self._switchboard.update_muserver_button()
            self._switchboard.update_locallistener_menuitem()
            self._switchboard.update_insights_menuitem()

            time.sleep(1.0)

class SwitchboardDialog(QtCore.QObject):

    STYLESHEET_PATH = os.path.join(RELATIVE_PATH, 'ui/switchboard.qss')

    _stylesheet_watcher: Optional[QtCore.QFileSystemWatcher] = None

    @classmethod
    def init_stylesheet_watcher(cls):
        ''' Load Qt stylesheet, and reload it whenever the file is changed. '''
        if not cls._stylesheet_watcher:
            cls._stylesheet_watcher = QtCore.QFileSystemWatcher()
            cls._stylesheet_watcher.addPath(cls.STYLESHEET_PATH)
            cls._stylesheet_watcher.fileChanged.connect(
                lambda: cls.reload_stylesheet())

            cls.reload_stylesheet()

    @classmethod
    def reload_stylesheet(cls):
        with open(cls.STYLESHEET_PATH, "r") as styling:
            stylesheet = styling.read()
            QtWidgets.QApplication.instance().setStyleSheet(stylesheet)

    def __init__(self, script_manager):
        super().__init__()

        self.script_manager = script_manager

        font_dir = os.path.join(ENGINE_PATH, 'Content/Slate/Fonts')
        font_files = ['Roboto-Regular.ttf', 'Roboto-Bold.ttf',
                      'DroidSansMono.ttf']

        for font_file in font_files:
            font_path = os.path.join(font_dir, font_file)
            QtGui.QFontDatabase.addApplicationFont(font_path)

        self.logger_autoscroll = True
        ConsoleStream.stderr().message_written.connect(self._console_pipe)

        # Set the UI object
        loader = QtUiTools.QUiLoader()
        loader.registerCustomWidget(sb_widgets.FramelessQLineEdit)
        loader.registerCustomWidget(DeviceWidgetHeader)
        loader.registerCustomWidget(DeviceListWidget)
        loader.registerCustomWidget(sb_widgets.ControlQPushButton)

        self.window = loader.load(
            os.path.join(RELATIVE_PATH, "ui/switchboard.ui"))

        # Add Tools Menu
        self.add_tools_menu()

        # used to shut down services cleanly on exit
        self.window.installEventFilter(self)
        self.close_event_counter = 0

        self.init_stylesheet_watcher()

        self._periodic_runner = None
        self._shoot = None
        self._sequence = None
        self._slate = None
        self._take = None
        self._project_changelist = None
        self._engine_changelist = None
        self._level = None
        self._multiuser_session_name = None
        self._is_recording = False
        self._description = 'description'
        self._started_mu_server = False

        # Recording Manager
        self.recording_manager = recording.RecordingManager(CONFIG.SWITCHBOARD_DIR)
        self.recording_manager.signal_recording_manager_saved.connect(self.recording_manager_saved)

        # DeviceManager
        self.device_manager = DeviceManager()
        self.device_manager.signal_device_added.connect(self.device_added)

        # Convenience UnrealInsights launcher
        self.init_insights_launcher()

        # Convenience local Switchboard lister launcher
        self.init_listener_launcher()

        # Convenience Open Logs Folder menu item
        self.register_open_logs_menuitem()
        self.register_zip_logs_menuitem()

        # Transport Manager
        #self.transport_queue = recording.TransportQueue(CONFIG.SWITCHBOARD_DIR)
        #self.transport_queue.signal_transport_queue_job_started.connect(self.transport_queue_job_started)
        #self.transport_queue.signal_transport_queue_job_finished.connect(self.transport_queue_job_finished)

        # add level picker combo box and refresh button
        self.level_combo_box = sb_widgets.SearchableComboBox(self.window)
        self.level_combo_box.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)
        
        self.refresh_levels_button = sb_widgets.ControlQPushButton()
        self.refresh_levels_button.setMaximumSize(22, 22)
        self.refresh_levels_button.setIcon(QtGui.QIcon("icon_refresh.png"))
        self.refresh_levels_button.setProperty("frameless", True)
        self.refresh_levels_button.setToolTip("Refresh level list")
        
        self.window.horizontalLayout_7.addWidget(self.level_combo_box)
        self.window.horizontalLayout_7.addWidget(self.refresh_levels_button)
        
        self.shoot = 'Default'
        self.sequence = SETTINGS.CURRENT_SEQUENCE
        self.slate = SETTINGS.CURRENT_SLATE
        self.take = SETTINGS.CURRENT_TAKE

        # Device List Widget
        self.device_list_widget: DeviceListWidget = self.window.device_list_widget
        # When new widgets are added, register the signal/slots
        self.device_list_widget.signal_register_device_widget.connect(self.register_device_widget)
        self.device_list_widget.signal_connect_all_plugin_devices_toggled.connect(self.on_all_plugin_devices_connect_toggled)
        self.device_list_widget.signal_open_all_plugin_devices_toggled.connect(self.on_all_plugin_devices_open_toggled)

        # forward device(widget) removal between ListWidget and DeviceManager
        self.device_list_widget.signal_remove_device.connect(self.device_manager.remove_device_by_hash)
        self.device_manager.signal_device_removed.connect(self.on_device_removed)

        # DeviceManager initialize with from the config
        #
        CONFIG.push_saving_allowed(False)
        try:
            self.device_manager.reset_plugins_settings(CONFIG)
            self.device_manager.add_devices(CONFIG._device_data_from_config)
        finally:
            CONFIG.pop_saving_allowed()

        # add menu for adding new devices
        self.device_add_menu = QtWidgets.QMenu()
        self.device_add_menu.aboutToShow.connect(lambda: self.show_device_add_menu())
        self.window.device_add_tool_button.setMenu(self.device_add_menu)
        self.window.device_add_tool_button.clicked.connect(lambda: self.show_device_add_menu())
        self.device_add_menu.triggered.connect(self.on_triggered_add_device)

        # Start the OSC server
        self.osc_server = switchboard_application.OscServer()
        self.osc_server.launch(SETTINGS.ADDRESS.get_value(), CONFIG.OSC_SERVER_PORT.get_value())

        # Register with OSC server
        self.osc_server.dispatcher_map(osc.TAKE, self.osc_take)
        self.osc_server.dispatcher_map(osc.SLATE, self.osc_slate)
        self.osc_server.dispatcher_map(osc.SLATE_DESCRIPTION, self.osc_slate_description)
        self.osc_server.dispatcher_map(osc.RECORD_START, self.osc_record_start)
        self.osc_server.dispatcher_map(osc.RECORD_STOP, self.osc_record_stop)
        self.osc_server.dispatcher_map(osc.RECORD_CANCEL, self.osc_record_cancel)
        self.osc_server.dispatcher_map(osc.RECORD_START_CONFIRM, self.osc_record_start_confirm)
        self.osc_server.dispatcher_map(osc.RECORD_STOP_CONFIRM, self.osc_record_stop_confirm)
        self.osc_server.dispatcher_map(osc.RECORD_CANCEL_CONFIRM, self.osc_record_cancel_confirm)
        self.osc_server.dispatcher_map(osc.UE4_LAUNCH_CONFIRM, self.osc_ue4_launch_confirm)
        self.osc_server.dispatcher.map(osc.OSC_ADD_SEND_TARGET_CONFIRM, self.osc_add_send_target_confirm, 1, needs_reply_address=True)
        self.osc_server.dispatcher.map(osc.ARSESSION_START_CONFIRM, self.osc_arsession_start_confirm, 1, needs_reply_address=True)
        self.osc_server.dispatcher.map(osc.ARSESSION_STOP_CONFIRM, self.osc_arsession_stop_confirm, 1, needs_reply_address=True)
        self.osc_server.dispatcher_map(osc.BATTERY, self.osc_battery)
        self.osc_server.dispatcher_map(osc.DATA, self.osc_data)

        # Connect UI to methods
        self.window.multiuser_session_lineEdit.textChanged.connect(self.on_multiuser_session_lineEdit_textChanged)
        self.window.slate_line_edit.textChanged.connect(self._set_slate)
        self.window.take_spin_box.valueChanged.connect(self._set_take)
        self.window.sequence_line_edit.textChanged.connect(self._set_sequence)
        self.level_combo_box.currentIndexChanged.connect(self._on_selected_level_changed)
        self.refresh_levels_button.clicked.connect(self.refresh_levels_incremental)
        self.window.project_cl_combo_box.currentTextChanged.connect(self._set_project_changelist)
        self.window.engine_cl_combo_box.currentIndexChanged.connect(
            lambda _: self._set_engine_changelist(self.window.engine_cl_combo_box.currentText()))
        self.window.engine_cl_combo_box.lineEdit().editingFinished.connect(
            lambda: self._set_engine_changelist(self.window.engine_cl_combo_box.currentText()))
        self.window.logger_level_comboBox.currentTextChanged.connect(self.logger_level_comboBox_currentTextChanged)
        self.window.logger_autoscroll_checkbox.stateChanged.connect(self.logger_autoscroll_stateChanged)
        self.window.logger_wrap_checkbox.stateChanged.connect(self.logger_wrap_stateChanged)
        self.window.record_button.released.connect(self.record_button_released)
        self.window.sync_all_button.clicked.connect(self.sync_all_button_clicked)
        self.window.build_all_button.clicked.connect(self.build_all_button_clicked)
        self.window.sync_and_build_all_button.clicked.connect(self.sync_and_build_all_button_clicked)
        self.window.refresh_project_cl_button.clicked.connect(self.refresh_project_cl_button_clicked)
        self.window.refresh_engine_cl_button.clicked.connect(self.refresh_engine_cl_button_clicked)
        self.window.connect_all_button.clicked.connect(self.connect_all_button_clicked)
        self.window.launch_all_button.clicked.connect(self.launch_all_button_clicked)
        self.window.settings_button.clicked.connect(self.settings_button_clicked)

        self.window.additional_settings = DeviceAdditionalSettingsUI("global")
        self.window.additional_settings.assign_button(self.window.device_settings_button, self.window)

        self.window.use_device_autojoin_setting_checkbox.toggled.connect(self.on_device_autojoin_changed)
        self.refresh_muserver_autojoin()

        self.window.muserver_start_stop_button.clicked.connect(self.on_muserver_start_stop_click)

        self.window.additional_settings.signal_device_widget_tracing.connect(self.on_tracing_settings_changed)
        self.refresh_trace_settings()

        # set up a thread that does periodic maintenace tasks
        self.setup_periodic_tasks_thread()

        # Connect to the session increment number.
        self.window.multiuser_session_inc_button.clicked.connect(self.on_multiuser_session_inc)

         # Stylesheet-related: Object names used for selectors, no focus forcing
        def configure_ctrl_btn(btn: sb_widgets.ControlQPushButton, name: str):
            btn.setObjectName(name)
            btn.hover_focus = False

        configure_ctrl_btn(self.refresh_levels_button, 'refresh')
        configure_ctrl_btn(self.window.sync_all_button, 'sync')
        configure_ctrl_btn(self.window.build_all_button, 'build')
        configure_ctrl_btn(self.window.sync_and_build_all_button, 'sync_and_build')
        configure_ctrl_btn(self.window.refresh_project_cl_button, 'refresh')
        configure_ctrl_btn(self.window.refresh_engine_cl_button, 'refresh')
        configure_ctrl_btn(self.window.launch_all_button, 'open')
        configure_ctrl_btn(self.window.connect_all_button, 'connect')

        # TransportQueue Menu
        #self.window.transport_queue_push_button.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        #self.transport_queue_menu = QtWidgets.QMenu(self.window.transport_queue_push_button)
        #self.transport_queue_menu.aboutToShow.connect(self.transport_queue_menu_about_to_show)
        #self.window.transport_queue_push_button.setMenu(self.transport_queue_menu)

        # Log pane
        self.window.logger_autoscroll_checkbox.setCheckState(QtCore.Qt.Checked if self.logger_autoscroll else QtCore.Qt.Unchecked)
        self.window.base_console.horizontalScrollBar().sliderPressed.connect(lambda: self.window.logger_autoscroll_checkbox.setCheckState(QtCore.Qt.Unchecked))
        self.window.base_console.verticalScrollBar().sliderPressed.connect(lambda: self.window.logger_autoscroll_checkbox.setCheckState(QtCore.Qt.Unchecked))
        # entries will be removed from the log window after the number of maximumBlockCount entries has been reached
        self.window.base_console.document().setMaximumBlockCount(1000)

        # Menu items
        self.window.menu_new_config.triggered.connect(self.menu_new_config)
        self.window.menu_save_config_as.triggered.connect(self.menu_save_config_as)
        self.window.menu_delete_config.triggered.connect(self.menu_delete_config)
        self.window.update_settings.triggered.connect(self.menu_update_settings)

        # The "Load Config" menu is populated lazily.
        self.window.menu_load_config.aboutToShow.connect(
            self._on_menu_load_config_about_to_show)

        # Plugin UI
        self.device_manager.plug_into_ui(self.window.menu_bar, self.window.tabs_main)

        if CONFIG.file_path:
            self.toggle_p4_controls(CONFIG.P4_ENABLED.get_value())
            self.refresh_levels()
        else:
            self.menu_new_config()

        self.set_config_hooks()

        self.set_multiuser_session_name(f'{SETTINGS.MUSERVER_SESSION_NAME}')

        # Run the transport queue
        #self.transport_queue_resume()
        
        self.update_current_config_text()
        self.update_current_address_text()
        SETTINGS.ADDRESS.signal_setting_changed.connect(
            lambda: self.update_current_address_text()
        )
        self.window.current_address_value.editingFinished.connect(self._try_change_address)

        self.refresh_window_title()

        self.script_manager.on_postinit(self)
        self.have_warned_about_muserver = False
        
    def _try_change_address(self):
        new_value = self.window.current_address_value.text()
        old_address = SETTINGS.ADDRESS.get_value()
        
        if not SETTINGS.ADDRESS.possible_values.__contains__(new_value):
            SETTINGS.ADDRESS.possible_values.append(new_value)
            
        SETTINGS.ADDRESS.update_value(new_value)
        SETTINGS.save()
        # Check if osc connection binds before commiting changes
        if self.osc_restart_check(old_address):
            # Print to make it clear to the user that the Address was updated
            if old_address != new_value:
                LOGGER.info(f"Updated address to {new_value}")
        else:
            LOGGER.warning(f"Reverting to the previous address ({old_address})")
            SETTINGS.ADDRESS.update_value(old_address)
            SETTINGS.save()
            self.osc_server.launch(SETTINGS.ADDRESS.get_value(), CONFIG.OSC_SERVER_PORT.get_value())

    def warn_user_about_muserver(self):
        if self.have_warned_about_muserver:
            return

        ServerInstance = switchboard_application.get_multi_user_server_instance()
        expected_server = ServerInstance.server_name()
        expected_endpoint = ServerInstance.endpoint_address()
        actual_server = ServerInstance.running_server_name()
        actual_endpoint = ServerInstance.running_endpoint()
        LOGGER.warning(f'The running Multi-user server does not match Switchboard configuration. Expected "{expected_server}" with "{expected_endpoint}" but found "{actual_server}" with "{actual_endpoint}". Please restart the multi-user server.')

        self.have_warned_about_muserver = True

    def get_muserver_label(self, is_running, is_valid):
        ServerInstance = switchboard_application.get_multi_user_server_instance()
        if is_running and is_valid:
            actual_server = ServerInstance.running_server_name()
            actual_endpoint = ServerInstance.running_endpoint()
            return f'Multi-user Server ({actual_server} {actual_endpoint})'
        elif is_running and not is_valid:
            return 'Multi-user Server (WARNING: RUNNING SERVER DOES NOT MATCH CONFIGURATION)'
        else:
            return 'Multi-user Server'

    def update_muserver_button(self):
        '''
        Update the status of the Multi-user start/stop button to reflect the status of Multi-user server.
        '''
        ServerInstance = switchboard_application.get_multi_user_server_instance()
        is_checked = self.window.muserver_start_stop_button.isChecked()
        is_running = ServerInstance.is_running()
        is_valid =  ServerInstance.validate_process()
        if  (is_running or self._started_mu_server) and not is_checked:
            self.window.muserver_start_stop_button.setChecked( True )
        elif not is_running and is_checked:
            self.window.muserver_start_stop_button.setChecked( False )
            self.have_warned_about_muserver = False

        label = self.get_muserver_label(is_running or self._started_mu_server,is_valid)
        self.window.muserver_label.setText(label)
        if is_running and not is_valid:
            self.warn_user_about_muserver()

        if is_running and self._started_mu_server:
            # Server has finished starting so reset our start flag.
            self._started_mu_server = False

    def setup_periodic_tasks_thread(self):
        '''
        Sets up a thread for performing maintenance tasks
        '''
        self._periodic_runner = PeriodicRunnable(self)
        QtCore.QThreadPool.globalInstance().start(self._periodic_runner)

    def update_locallistener_menuitem(self):
        ''' 
        Enables/disables the local listener launch menu item depending on whether 
        it is already running or not.
        '''
        self.locallistener_launcher_menuitem.setEnabled(not self.listener_launcher.is_running())

    def update_insights_menuitem(self):
        ''' 
        Enables/disables the UnrealInsights launch menu item depending on whether 
        it is already running or not.
        '''
        self.insights_launcher_menuitem.setEnabled(not self.insights_launcher.is_running())

    def on_muserver_start_stop_click(self):
        '''
        Handle the multi-user server button click. If we are running we stop the process. If we are not
        running then we launch the multi-user server.
        '''
        ServerInstance = switchboard_application.get_multi_user_server_instance()
        if ServerInstance.is_running():
            ServerInstance.terminate(bypolling=True)
            self._started_mu_server = False
        else:
            self._started_mu_server = True
            ServerInstance.launch()
        self.update_muserver_button()

    def init_insights_launcher(self):
        ''' Initializes insights launcher '''

        self.insights_launcher = InsightsLauncher()

        def launch_insights():
            try:
                self.insights_launcher.launch()
            except Exception as e:
                LOGGER.error(e)

        action = self.register_tools_menu_action("&Insights")
        action.triggered.connect(launch_insights)

        self.insights_launcher_menuitem = action

    def init_listener_launcher(self):
        ''' Initializes switcboard listener launcher '''
        self.listener_launcher = ListenerLauncher()

        def launch_listener():
            try:
                self.listener_launcher.launch()
            except Exception as e:
                LOGGER.error(e)

        action = self.register_tools_menu_action("&Listener")
        action.triggered.connect(launch_listener)

        self.locallistener_launcher_menuitem = action

    def register_open_logs_menuitem(self):
        ''' Registers convenience "Open Logs Folder" menu item '''
        action = self.register_tools_menu_action("&Open Logs Folder")
        action.triggered.connect(collect_logs.open_logs_folder)

    def register_zip_logs_menuitem(self):
        def save_logs():
            status = collect_logs.execute_zip_logs_workflow(
                CONFIG,
                [device for device in self.device_manager.devices() if isinstance(device, DeviceUnreal)]
            )
            if status:
                collect_logs.open_logs_folder()
        action = self.register_tools_menu_action("&Zip Logs")
        action.triggered.connect(save_logs)

    def add_tools_menu(self):
        ''' Adds tools menu to menu bar and populates built-in items '''

        self.tools_menu = self.window.menu_bar.addMenu("&Tools")

    def register_tools_menu_action(self, actionname:str, menunames:List[str] = []) -> QWidgetAction:
        ''' Registers a QWidgetAction with the tools menu

        Args:
            actionname: Name of the action to be added
            menunames: Submenu(s) where to place the given action

        Returns:
            QWidgetAction: The action that was created.
        '''

        # Find find submenu, and create the ones that don't exist along the way

        current_menu:QMenu = self.tools_menu

        # iterate over menunames give
        for menuname in menunames:

            create_menu = True

            # try to find existing menu
            for current_action in current_menu.actions():

                submenu = current_action.menu()

                if submenu and (menuname == submenu.title()):
                    current_menu = submenu
                    create_menu = False
                    break
            
            # if we didn't find the submenu, create it
            if create_menu:
                newmenu = QMenu(parent=current_menu)
                newmenu.setTitle(menuname)
                current_menu.addMenu(newmenu)
                current_menu = newmenu

        # add the given action
        action = QWidgetAction(current_menu)
        action.setText(actionname)
        current_menu.addAction(action)
        
        return action

    def on_device_autojoin_changed(self):
        CONFIG.MUSERVER_AUTO_JOIN.update_value(
            self.window.use_device_autojoin_setting_checkbox.isChecked())

    def set_config_hooks(self):
        CONFIG.P4_PROJECT_PATH.signal_setting_changed.connect(lambda: self.p4_refresh_project_cl())
        CONFIG.P4_ENGINE_PATH.signal_setting_changed.connect(lambda: self.p4_refresh_engine_cl())
        CONFIG.BUILD_ENGINE.signal_setting_changed.connect(lambda: self.p4_refresh_engine_cl())
        CONFIG.P4_ENABLED.signal_setting_changed.connect(lambda _, enabled: self.toggle_p4_controls(enabled))
        CONFIG.MAPS_PATH.signal_setting_changed.connect(lambda: self.refresh_levels())
        CONFIG.MAPS_FILTER.signal_setting_changed.connect(lambda: self.refresh_levels())
        CONFIG.INSIGHTS_TRACE_ENABLE.signal_setting_changed.connect(lambda: self.refresh_trace_settings())
        CONFIG.INSIGHTS_TRACE_ARGS.signal_setting_changed.connect(lambda: self.refresh_trace_settings())
        CONFIG.INSIGHTS_STAT_EVENTS.signal_setting_changed.connect(lambda: self.refresh_trace_settings())
        CONFIG.MUSERVER_AUTO_JOIN.signal_setting_changed.connect(lambda: self.refresh_muserver_autojoin())
        CONFIG.PROJECT_NAME.signal_setting_changed.connect(lambda: self.refresh_window_title())

    def refresh_trace_settings(self):
        self.window.additional_settings.set_insight_trace_state(CONFIG.INSIGHTS_TRACE_ENABLE.get_value())
        self.window.additional_settings.set_insight_tracing_args(CONFIG.INSIGHTS_TRACE_ARGS.get_value())

    def refresh_muserver_autojoin(self):
        self.window.use_device_autojoin_setting_checkbox.setChecked(CONFIG.MUSERVER_AUTO_JOIN.get_value())

    def on_tracing_settings_changed(self):
        settings = self.window.additional_settings
        trace_tuple = settings.trace_settings()
        CONFIG.INSIGHTS_TRACE_ENABLE.update_value(trace_tuple[0])
        CONFIG.INSIGHTS_TRACE_ARGS.update_value(trace_tuple[1])

    def show_device_add_menu(self):
        self.device_add_menu.clear()
        plugins = sorted(self.device_manager.available_device_plugins(), key=str.lower)
        for plugin in plugins:
            icons = self.device_manager.plugin_icons(plugin)
            icon = icons["enabled"] if "enabled" in icons.keys() else QtGui.QIcon()
            self.device_add_menu.addAction(icon, plugin)
        self.window.device_add_tool_button.showMenu()

    def on_triggered_add_device(self, action):
        device_type = action.text()
        dialog = self.device_manager.get_device_add_dialog(device_type)
        dialog.exec()

        if dialog.result() == QtWidgets.QDialog.Accepted:
            for device in dialog.devices_to_remove():
                # this is pretty specific to nDisplay. It will remove all existing nDisplay devices before the devices of a new nDisplay config are added.
                # this offers a simple way to update nDisplay should the config file have been changed.
                self.device_manager.remove_device(device)

            self.device_manager.add_devices({device_type : dialog.devices_to_add()})
            CONFIG.save()

    def on_device_removed(self, device_hash, device_type, device_name, update_config):
        self.device_list_widget.on_device_removed(device_hash, device_type, device_name, update_config)
        CONFIG.on_device_removed(device_hash, device_type, device_name, update_config)

    def eventFilter(self, obj, event: QtCore.QEvent):
        if obj == self.window and event.type() == QtCore.QEvent.Close:
            self.close_event_counter += 1
            if not self.should_allow_exit(self.close_event_counter):
                event.ignore()
                return True
            else:
                self.on_exit()

        return self.window.eventFilter(obj, event)

    def should_allow_exit(self, close_req_id: int) -> bool:
        return all(device.should_allow_exit(close_req_id)
                   for device in self.device_manager.devices())

    def on_exit(self):
        if self._periodic_runner:
            self._periodic_runner.exit()
        self.osc_server.close()
        for device in self.device_manager.devices():
            device.disconnect_listener()
        self.window.removeEventFilter(self)

    def transport_queue_menu_about_to_show(self):
        self.transport_queue_menu.clear()

        action = QtWidgets.QWidgetAction(self.transport_queue_menu)
        action.setDefaultWidget(TransportQueueHeaderActionWidget())
        self.transport_queue_menu.addAction(action)

        for job_name in self.transport_queue.transport_jobs.keys():
            action = QtWidgets.QWidgetAction(self.transport_queue_menu)
            action.setDefaultWidget(TransportQueueActionWidget(job_name))
            self.transport_queue_menu.addAction(action)

    def _on_menu_load_config_about_to_show(self):
        from functools import partial

        self.window.menu_load_config.clear()

        # We'll build up a dictionary of directory paths to the submenu for
        # that directory as we go. Config files in the root configs directory
        # will go at the top level.
        menu_hierarchy = {
            config.ROOT_CONFIGS_PATH: self.window.menu_load_config
        }

        def _get_menu_for_path(path):
            # Safe guard to make sure we don't accidentally traverse up past
            # the root configs path.
            if (path != config.ROOT_CONFIGS_PATH and
                    config.ROOT_CONFIGS_PATH not in path.parents):
                return None

            path_menu = menu_hierarchy.get(path)
            if not path_menu:
                parent_menu = _get_menu_for_path(path.parent)
                if not parent_menu:
                    return None

                path_menu = parent_menu.addMenu(path.name)
                menu_hierarchy[path] = path_menu

            return path_menu

        config_paths = config.list_config_paths()

        # Take a first pass through the config paths just creating the
        # submenus. This makes sure that all submenus appear before any configs
        # for any given menu level.
        for config_path in config_paths:
            _get_menu_for_path(config_path.parent)

        # Now the dictionary of menus should be populated, so create actions
        # for each config in the appropriate menu.
        for config_path in config_paths:
            menu = _get_menu_for_path(config_path.parent)
            if not menu:
                continue

            config_action = menu.addAction(config_path.stem,
                partial(self.set_current_config, config_path))

            if config_path == SETTINGS.CONFIG:
                config_action.setEnabled(False)

        # Make a special entry for a config not in the normal area
        externalconfig_action = QtWidgets.QAction("Browse...", self.window.menu_load_config)
        externalconfig_action.triggered.connect(self._on_open_external_config)
        self.window.menu_load_config.addAction(externalconfig_action)

    def _on_open_external_config(self):
        ''' When the user wants to open a config located outside of the designated area
        This is useful when the user keeps the configs somewhere else (e.g. under the project)
        '''
        config_path, _ = QtWidgets.QFileDialog.getOpenFileName(self.window,
            'Select config file', str(config.ROOT_CONFIGS_PATH),
            f'Config files (*{config.CONFIG_SUFFIX})'
        )

        # Do nothing if the user didn't choose a config path
        if not config_path:
            return

        # ok, let's open it
        self.set_current_config(config_path)

    def set_current_config(self, config_path):

        # Update to the new config
        CONFIG.init_with_file_path(config_path)

        SETTINGS.CONFIG = config_path
        SETTINGS.save()

        # Disable saving while loading
        CONFIG.push_saving_allowed(False)

        try:
            # Remove all devices
            self.device_manager.clear_device_list()
            self.device_list_widget.clear_widgets()

            # Reset plugin settings
            self.device_manager.reset_plugins_settings(CONFIG)

            # Set hooks to this dialog's UI
            self.set_config_hooks()

            # Add new devices
            self.device_manager.add_devices(CONFIG._device_data_from_config)
        finally:
            # Re-enable saving after loading.
            CONFIG.pop_saving_allowed()

        self.p4_refresh_project_cl()
        self.p4_refresh_engine_cl()
        self.refresh_levels()
        self.update_current_config_text()
        self.refresh_muserver_autojoin()
        self.refresh_trace_settings()
        self.refresh_window_title()

    def refresh_window_title(self):
        ''' Updates the window title based on the project name '''

        project_name = CONFIG.PROJECT_NAME.get_value()

        if project_name:
            self.window.setWindowTitle(f"Switchboard - {project_name}")
        else:
            self.window.setWindowTitle(f"Switchboard")

    def update_current_config_text(self):
        # Can be none when current file is deleted
        if SETTINGS.CONFIG is not None:
            file_name = os.path.basename(SETTINGS.CONFIG)
            self.window.current_config_file_value.setText(file_name)
        else:
            self.window.current_config_file_value.setText("No config loaded")
    
    def update_current_address_text(self):
        self.window.current_address_value.setText(SETTINGS.ADDRESS.get_value())

    def create_new_config(self, file_path: Union[str, Path], uproject, engine_dir, p4_settings):
        ''' Creates a new config file

        Args:
            file_path: Path to the new config file
            uproject: Path to the unreal project (.uproject) file
            engine_dir: Path to the Engine/ directory
            p4_settings: Desired Perforce settings
        '''

        CONFIG.init_new_config(
            file_path=file_path,
            uproject=uproject,
            engine_dir=engine_dir,
            p4_settings=p4_settings
        )

        # Disable saving while loading
        CONFIG.push_saving_allowed(False)
        try:
            # Remove all devices
            self.device_manager.clear_device_list()
            self.device_list_widget.clear_widgets()

            # Reset plugin settings
            self.device_manager.reset_plugins_settings(CONFIG)

            # Set hooks to this dialog's UI
            self.set_config_hooks()
        finally:
            # Re-enable saving after loading
            CONFIG.pop_saving_allowed()

        # Update the UI
        self.toggle_p4_controls(CONFIG.P4_ENABLED.get_value())
        self.refresh_levels()
        self.update_current_config_text()
        self.refresh_muserver_autojoin()
        self.refresh_trace_settings()
        self.refresh_window_title()

    def menu_save_config_as(self):
        ''' Copy the current config file and move to it '''

        # get the destination path for the copy

        new_config_path, _ = QtWidgets.QFileDialog.getSaveFileName(self.window,
            'Select config file', str(config.ROOT_CONFIGS_PATH),
            f'Config files (*{config.CONFIG_SUFFIX})',
            #options=QtWidgets.QFileDialog.DontUseNativeDialog
        )

        # Do nothing if the user didn't choose a destination config path
        if not new_config_path:
            return

        # If replacing the same config, just save
        if Path(new_config_path).resolve() == Path(SETTINGS.CONFIG).resolve():
            CONFIG.save()
            return

        # Switch to the new config
        CONFIG.save_as(new_config_path)

        # Update the settings
        SETTINGS.CONFIG = new_config_path
        SETTINGS.save()

        # Make sure we reflect the new config in the status bar
        self.update_current_config_text()

        # Reset mu server name
        # * unless it is running to avoid devices failing to connect to the current server
        # * if there are devices running, they won't auto-connect when you launch the renamed server
        mu_server = switchboard_application.get_multi_user_server_instance()
        if not mu_server.is_running():
            CONFIG.MUSERVER_SERVER_NAME.update_value(CONFIG.default_mu_server_name())

    def menu_new_config(self):
        uproject_search_path = os.path.dirname(CONFIG.UPROJECT_PATH.get_value().replace('"',''))

        if not os.path.exists(uproject_search_path):
            uproject_search_path = SETTINGS.LAST_BROWSED_PATH

        dialog = AddConfigDialog(
            uproject_search_path=uproject_search_path,
            previous_engine_dir=CONFIG.ENGINE_DIR.get_value(),
            parent=self.window)
        dialog.exec()

        if dialog.result() == QtWidgets.QDialog.Accepted:

            self.create_new_config(
                file_path=dialog.config_path,
                uproject=dialog.uproject, 
                engine_dir=dialog.engine_dir,
                p4_settings=dialog.p4_settings()
            )

    def menu_delete_config(self):
        """
        Delete the currently loaded config.

        After deleting, it will load the first config found by
        config.list_config_paths(), or it will create a new config.
        """
        # Show the confirmation dialog using a relative path to the config.
        rel_config_path = config.get_relative_config_path(SETTINGS.CONFIG)
        reply = QtWidgets.QMessageBox.question(self.window, 'Delete Config',
            f'Are you sure you would like to delete config "{rel_config_path}"?',
            QtWidgets.QMessageBox.Yes, QtWidgets.QMessageBox.No)

        if reply == QtWidgets.QMessageBox.Yes:
            # Remove the old config
            try:
                SETTINGS.CONFIG.unlink()
                self.set_current_config(None)
            except FileNotFoundError as e:
                LOGGER.error(f'menu_delete_config: {e}')

            # Grab a new config to lead once this one is deleted
            config_paths = config.list_config_paths()

            if config_paths:
                self.set_current_config(config_paths[0])
            else:
                # Create a blank config
                self.menu_new_config()

    def menu_update_settings(self):
        """
        Settings window
        """
        # TODO: VALIDATE RECORD PATH
        settings_dialog = SettingsDialog(SETTINGS, CONFIG)

        for plugin_name in sorted(self.device_manager.available_device_plugins(), key=str.lower):
            device_instances = self.device_manager.devices_of_type(plugin_name)
            device_settings = [(device.name, device.device_settings(), device.setting_overrides()) for device in device_instances]
            settings_dialog.add_section_for_plugin(plugin_name, self.device_manager.plugin_settings(plugin_name), device_settings)
        
        settings_dialog.select_all_tab()

        old_address = SETTINGS.ADDRESS.get_value()

        # avoid saving the config all the time while in the settings dialog
        CONFIG.push_saving_allowed(False)
        try:
            # Show the Settings Dialog
            settings_dialog.ui.exec()
        finally:
            # Restore saving, which should happen at the end of this function
            CONFIG.pop_saving_allowed()

        new_config_path = settings_dialog.config_path()
        if new_config_path != SETTINGS.CONFIG and new_config_path is not None:
            CONFIG.replace(new_config_path)
            SETTINGS.CONFIG = new_config_path
            SETTINGS.save()
        if old_address != SETTINGS.ADDRESS.get_value() or SETTINGS.TRANSPORT_PATH.get_value():
            SETTINGS.save()

        self.osc_restart_check(old_address)

        CONFIG.save()

    def osc_restart_check(self, old_address):
        if old_address != SETTINGS.ADDRESS.get_value():
            self.osc_server.close()
            return self.osc_server.launch(SETTINGS.ADDRESS.get_value(), CONFIG.OSC_SERVER_PORT.get_value())
        else:
            return True

    def sync_all_button_clicked(self):
        if not CONFIG.P4_ENABLED.get_value():
            return
        device_widgets = self.device_list_widget.device_widgets()

        for device_widget in device_widgets:
            if device_widget.can_sync():
                device_widget.sync_button_clicked()

    def build_all_button_clicked(self):
        device_widgets = self.device_list_widget.device_widgets()

        for device_widget in device_widgets:
            if device_widget.can_build():
                device_widget.build_button_clicked()

    def sync_and_build_all_button_clicked(self):
        if not CONFIG.P4_ENABLED.get_value():
            return
        device_widgets = self.device_list_widget.device_widgets()

        for device_widget in device_widgets:
            if device_widget.can_sync() and device_widget.can_build():
                device_widget.sync_button_clicked()
                device_widget.build_button_clicked()

    def refresh_project_cl_button_clicked(self):
        self.p4_refresh_project_cl()

    def refresh_engine_cl_button_clicked(self):
        self.p4_refresh_engine_cl()

    def connect_all_button_clicked(self, button_state):
        devices = self.device_manager.devices()
        self.set_device_connection_state(devices, button_state)

    def launch_all_button_clicked(self, button_state):
        devices = self.device_manager.devices()
        self.set_device_launch_state(devices, button_state)

    def settings_button_clicked(self, button_state):
        self.menu_update_settings()

    def set_device_launch_state(self, devices, launch_state):
        for device in devices:
            try:
                open_button = device.widget.open_button
                if open_button.isEnabled():
                    if launch_state:
                        device.widget._open()
                    elif open_button.isChecked():
                        device.widget._close()
            except Exception:
                pass

    def set_device_connection_state(self, devices, connection_state):
        for device in devices:
            try:
                if connection_state:
                    device.widget._connect()
                else:
                    device.widget._disconnect()
            except Exception:
                pass

    @QtCore.Slot(object)
    def recording_manager_saved(self, recording):
        """
        When the RecordingManager saves a recording
        """
        pass

    # START HERE
    # TODO
    # If JOB IS ADDED, RESUME
    # If DEVICE CONNECTS, RESUME
    def transport_queue_resume(self):
        # Do not allow transport while recording
        if self._is_recording:
            return

        # Do not transport if the active queue is full
        if self.transport_queue.active_queue_full():
            return

        for _, transport_job in self.transport_queue.transport_jobs.items():
            # Do not transport if the device is disconnected
            device = self.device_manager.device_with_name(transport_job.device_name)
            if device.status < DeviceStatus.OPEN:
                continue

            # Only Transport jobs that are ready
            if transport_job.transport_status != recording.TransportStatus.READY_FOR_TRANSPORT:
                continue

            # Transport the file
            self.transport_queue.run_transport_job(transport_job, device)

            # Bail if active queue is full
            if self.transport_queue.active_queue_full():
                break

    @QtCore.Slot(object)
    def transport_queue_job_finished(self, transport_job):
        """
        When the TransportQueue finished a job
        """
        LOGGER.debug(f'transport_queue_job_finished {transport_job.job_name}')
        '''
        # If the device is connected, set that status as READY_FOR_TRANSPORT
        transport_job.transport_status = recording.TransportStatus.READY_FOR_TRANSPORT
        #transport_queue_job_added
        '''

    @QtCore.Slot(object)
    def transport_queue_job_started(self, transport_job):
        """
        When the TransportQueue is ready to transport a new job
        Grab the device and send it back to the transport queue
        """
        LOGGER.debug('transport_queue_job_started')

    @QtCore.Slot(object)
    def device_added(self, device):
        """
        When a new device is added to the DeviceManger, connect its signals
        """
        device.device_qt_handler.signal_device_connect_failed.connect(self.device_connect_failed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_client_disconnected.connect(self.device_client_disconnected, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_project_changelist_changed.connect(self.device_project_changelist_changed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_engine_changelist_changed.connect(self.device_engine_changelist_changed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_built_engine_changelist_changed.connect(self.device_built_engine_changelist_changed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_status_changed.connect(self.device_status_changed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_sync_failed.connect(self.device_sync_failed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_is_recording_device_changed.connect(self.device_is_recording_device_changed, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_build_update.connect(self.device_build_update, QtCore.Qt.QueuedConnection)
        device.device_qt_handler.signal_device_sync_update.connect(self.device_sync_update, QtCore.Qt.QueuedConnection)

        # Add the view
        self.device_list_widget.add_device_widget(device)

    @QtCore.Slot(object)
    def register_device_widget(self, device_widget):
        """
        When a new DeviceWidget is added, connect all the signals
        """
        device_widget.signal_device_widget_connect.connect(self.device_widget_connect)
        device_widget.signal_device_widget_disconnect.connect(self.device_widget_disconnect)
        device_widget.signal_device_widget_open.connect(self.device_widget_open)
        device_widget.signal_device_widget_close.connect(self.device_widget_close)
        device_widget.signal_device_widget_sync.connect(self.device_widget_sync)
        device_widget.signal_device_widget_build.connect(self.device_widget_build)
        device_widget.signal_device_widget_trigger_start_toggled.connect(self.device_widget_trigger_start_toggled)
        device_widget.signal_device_widget_trigger_stop_toggled.connect(self.device_widget_trigger_stop_toggled)

        # KiPro Signal Support
        try:
            device_widget.signal_device_widget_play.connect(self.device_widget_play)
            device_widget.signal_device_widget_stop.connect(self.device_widget_stop)
        except:
            pass

        try:
            device = self.device_manager.device_with_hash(device_widget.device_hash)
            device.device_widget_registered(device_widget)
        except:
            LOGGER.error(f'Could not find device with hash {device_widget.device_hash} when registering its widget')

    def on_all_plugin_devices_connect_toggled(self, plugin_name, button_state):
        devices = self.device_manager.devices_of_type(plugin_name)
        self.set_device_connection_state(devices, button_state)

    def on_all_plugin_devices_open_toggled(self, plugin_name, button_state):
        devices = self.device_manager.devices_of_type(plugin_name)
        self.set_device_launch_state(devices, button_state)

    def multiuser_session_name(self):
        return self._multiuser_session_name

    def on_multiuser_session_lineEdit_textChanged(self, text):
        self.set_multiuser_session_name(text)

    def on_multiuser_session_inc(self):
        '''
        Increment the session name by 1.
        '''
        current_name = self.multiuser_session_name()
        match = re.search(r'\d+$', current_name)
        basename = current_name
        num_as_int = 1
        padding = 0
        if match is not None:
            num_as_str = match.group()
            basename = str.join(num_as_str, current_name.split(num_as_str)[:-1])
            try:
                num_as_int = int(num_as_str) + 1
                padding = len(num_as_str)
            except ValueError:
                # Do not treat value conversion as an error it will just refer back to default value.
                pass
        new_num = f'{num_as_int}'.zfill(padding)
        self.set_multiuser_session_name(f'{basename}{new_num}')

    def set_multiuser_session_name(self, value):

        # sanitize the session name
        value = value.replace(' ', '_')

        self._multiuser_session_name = value

        if self.window.multiuser_session_lineEdit.text() != value:
            self.window.multiuser_session_lineEdit.setText(value)
        
        if value != SETTINGS.MUSERVER_SESSION_NAME:
            SETTINGS.MUSERVER_SESSION_NAME = value
            SETTINGS.save()

    @property
    def shoot(self):
        return self._shoot

    @shoot.setter
    def shoot(self, value):
        self._shoot = value

    @property
    def sequence(self):
        return self._sequence

    @sequence.setter
    def sequence(self, value):
        self._set_sequence(value)

    def _set_sequence(self, value):
        self._sequence = value
        self.window.sequence_line_edit.setText(value)

        # Reset the take number to 1 if setting the sequence
        self.take = 1

    @property
    def slate(self):
        return self._slate

    @slate.setter
    def slate(self, value):
        self._set_slate(value, reset_take=False)

    def _set_slate(self, value, exclude_addresses=[], reset_take=True):
        """
        Internal setter that allows exclusion of addresses
        """
        # Protect against blank slates
        if value == '':
            return

        if self._slate == value:
            return

        self._slate = value
        SETTINGS.CURRENT_SLATE = value
        SETTINGS.save()

        # Reset the take number to 1 if setting the slate
        if reset_take:
            self.take = 1

        # UI out of date with control means external message
        if self.window.slate_line_edit.text() != self._slate:
            self.window.slate_line_edit.setText(self._slate)

        thread = threading.Thread(target=self._set_slate_all_devices, args=[value], kwargs={'exclude_addresses':exclude_addresses})
        thread.start()

    def _set_slate_all_devices(self, value, exclude_addresses=[]):
        """
        Tell all devices the new slate
        """
        for device in self.device_manager.devices():
            if device.address in exclude_addresses:
                continue

            # Do not send updates to disconnected devices
            if device.is_disconnected:
                continue

            device.set_slate(self._slate)

    @property
    def take(self):
        return self._take

    @take.setter
    def take(self, value):
        self._set_take(value)

    def _set_take(self, value, exclude_addresses=[]):
        """
        Internal setter that allows exclusion of addresses
        """
        requested_take = value

        # TODO: Add feedback in UI
        # Check is that slate/take combo has been used before
        while not self.recording_manager.slate_take_available(self._sequence, self._slate, requested_take):
            requested_take += 1
        
        if requested_take == value == self._take:
            return

        if requested_take != value:
            LOGGER.warning(f'Slate: "{self._slate}" Take: "{value}" have already been used. Auto incremented up to take: "{requested_take}"')
            # Clear the exclude list since Switchboard changed the incoming value
            exclude_addresses = []
        

        self._take = requested_take
        SETTINGS.CURRENT_TAKE = value
        SETTINGS.save()

        if self.window.take_spin_box.value() != self._take:
            self.window.take_spin_box.setValue(self._take)

        thread = threading.Thread(target=self._set_take_all_devices, args=[value], kwargs={'exclude_addresses':exclude_addresses})
        thread.start()

    def _set_take_all_devices(self, value, exclude_addresses=[]):
        """
        Tell all devices the new take
        """
        # Tell all devices the new take
        for device in self.device_manager.devices():
            # Do not send updates to disconnected devices
            if device.is_disconnected:
                continue

            if device.address in exclude_addresses:
                continue

            device.set_take(self._take)

    @property
    def description(self):
        return self._description

    @description.setter
    def description(self, value):
        self._description = f'{self.level} {self.slate} {self.take}\nvalue'

    @property
    def level(self):
        return self._level

    @level.setter
    def level(self, value):
        self._set_level(value)

    def _on_selected_level_changed(self, index: int):
        full_map_path = self._get_level_from_combo_box(index)
        self._set_level(full_map_path)
        
    def _get_level_from_combo_box(self, index: int):
        # Data stores full path
        return self.level_combo_box.itemData(index)

    def _set_level(self, value):
        ''' Called when level dropdown text changes
        '''
        self._level = value

        if CONFIG.CURRENT_LEVEL != value:
            CONFIG.CURRENT_LEVEL = value
            CONFIG.save()

        if self.level_combo_box.currentText() != self._level:
            for index in range(self.level_combo_box.count()):
                if self._get_level_from_combo_box(index) == self._level:
                    self.level_combo_box.blockSignals(True)
                    self.level_combo_box.setCurrentIndex(index)
                    self.level_combo_box.blockSignals(False)
                    break

    @property
    def project_changelist(self):
        return self._project_changelist

    @project_changelist.setter
    def project_changelist(self, value):
        self._set_project_changelist(value)

    def _set_project_changelist(self, value):
        self._project_changelist = value

        if self.window.project_cl_combo_box.currentText() != self._project_changelist:
            self.window.project_cl_combo_box.setText(self._project_changelist)

        # Check if all of the devices are on the right changelist
        for device in self.device_manager.devices():
            if device.project_changelist:
                device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
                device_widget.update_project_changelist(
                    required_cl=self.project_changelist if self.project_changelist != EMPTY_SYNC_ENTRY else None,
                    current_device_cl=device.project_changelist
                )

    @property
    def engine_changelist(self):
        return self._engine_changelist

    @engine_changelist.setter
    def engine_changelist(self, value):
        self._set_engine_changelist(value)

    def _set_engine_changelist(self, changelist_value):
        self._engine_changelist = changelist_value

        if self.window.engine_cl_combo_box.currentText() != self._engine_changelist:
            self.window.engine_cl_combo_box.setText(self._engine_changelist)

        # Check if all of the devices are on the right changelist
        for device in self.device_manager.devices():
            if device.engine_changelist:
                device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
                device_widget.update_engine_changelist(
                    required_cl=self.engine_changelist if self.engine_changelist != EMPTY_SYNC_ENTRY else None, 
                    synched_cl=device.engine_changelist,
                    built__cl=device.built_engine_changelist
                )

    @QtCore.Slot(object)
    def device_widget_connect(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        if not device:
            return

        if device.device_type == 'LiveLinkFace':
            device.look_for_device = True
        else:
            device.connect_listener()

    @QtCore.Slot(object)
    def device_connect_failed(self, device):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)

        if not device_widget:
            return

        device_widget._disconnect()

        LOGGER.warning(f'{device.name}: Could not connect to device')

    @QtCore.Slot(object)
    def device_widget_disconnect(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        if not device:
            return

        if device.device_type == 'LiveLinkFace':
            device.look_for_device = False
        else:
            device.disconnect_listener()

    @QtCore.Slot(object)
    def device_client_disconnected(self, device):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)

        device_widget._disconnect()
        LOGGER.warning(f'{device.name}: Client disconnected')

    @QtCore.Slot(object)
    def device_widget_open(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)

        device.launch(self.level)

    @QtCore.Slot(object)
    def device_widget_close(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.close(force=True)

    @QtCore.Slot(object)
    def device_widget_sync(self, device_widget):
        if not CONFIG.P4_ENABLED.get_value():
            return
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        project_cl = None if self.project_changelist == EMPTY_SYNC_ENTRY else self.project_changelist
        engine_cl = None if self.engine_changelist == EMPTY_SYNC_ENTRY else self.engine_changelist
        device.sync(engine_cl, project_cl)

    @QtCore.Slot(object)
    def device_widget_build(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.build()

    @QtCore.Slot(object)
    def device_widget_play(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.play()

    @QtCore.Slot(object)
    def device_widget_stop(self, device_widget):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.stop()

    @QtCore.Slot(object)
    def device_sync_failed(self, device):
        #LOGGER.debug(f'{device.name} device_sync_failed')
        # CHANGE THE SYNC ICON HERE
        pass

    @QtCore.Slot(object)
    def device_build_update(self, device, step, percent):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.update_build_status(device, step, percent)

    @QtCore.Slot(object)
    def device_sync_update(self, device, progress):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.update_sync_status(device, progress)

    @QtCore.Slot(object)
    def device_project_changelist_changed(self, device):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.update_project_changelist(
            required_cl=self.project_changelist if self.project_changelist != EMPTY_SYNC_ENTRY else None,
            current_device_cl=device.project_changelist
        )
        
        cl = device.project_changelist
        address = device.address
        for device in self.device_manager.devices():
            if device.address == address and device.project_changelist and device.project_changelist != cl:
                device.project_changelist = cl

    @QtCore.Slot(object)
    def device_engine_changelist_changed(self, device):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.update_engine_changelist(
            required_cl=self.engine_changelist if self.engine_changelist != EMPTY_SYNC_ENTRY else None,
            synched_cl=device.engine_changelist,
            built__cl=device.built_engine_changelist
        )

        cl = device.engine_changelist
        address = device.address
        for device in self.device_manager.devices():
            if device.address == address and device.engine_changelist and device.engine_changelist != cl:
                device.engine_changelist = cl

    @QtCore.Slot(object)
    def device_built_engine_changelist_changed(self, device):
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.update_build_info(
            synched_cl=device.engine_changelist,
            built_cl=device.built_engine_changelist
        )

    @QtCore.Slot(object)
    def device_status_changed(self, device, previous_status):
        # Update the device widget
        device.widget.update_status(device.status, previous_status)

        devices = self.device_manager.devices_of_type(device.device_type)
        self.device_list_widget.update_category_status(device.category_name, devices)
        self.update_connect_and_open_button_states()

        if previous_status != device.status:
            LOGGER.debug(f'{device.name}: device status change: {device.status.name}')

        if previous_status == DeviceStatus.RECORDING and device.status >= DeviceStatus.OPEN:
            self.device_record_stop_confirm(device)
        elif previous_status == DeviceStatus.READY and device.status >= DeviceStatus.RECORDING:
            self.device_record_start_confirm(device)

        # Send Slate/Take to the device when it connects
        if previous_status <= DeviceStatus.OPEN and device.status >= DeviceStatus.READY:
            device.set_take(self.take)
            device.set_slate(self.slate)

    def update_connect_and_open_button_states(self):
        """ Refresh states of connect-all and start-all buttons. """
        connect_checked, connect_enabled, open_checked, open_enabled = \
            self.device_list_widget.get_connect_and_open_all_button_states()

        self.update_connect_all_button_state(checked=connect_checked,
                                            enabled=connect_enabled)
        self.update_start_all_button_state(checked=open_checked,
                                        enabled=open_enabled)

    def update_connect_all_button_state(self, checked, enabled):
        """ Refresh state of connect-all button. """
        self.window.connect_all_button.setChecked(checked)
        self.window.connect_all_button.setEnabled(enabled)
        if checked:
            self.window.connect_all_button.setToolTip("Disconnect all connected devices")
        else:
            self.window.connect_all_button.setToolTip("Connect all devices")

    def update_start_all_button_state(self, checked, enabled):
        """ Refresh state of start-all button. """
        self.window.launch_all_button.setChecked(checked)
        self.window.launch_all_button.setEnabled(enabled)
        if checked:
            self.window.launch_all_button.setToolTip("Stop all running devices")
        else:
            self.window.launch_all_button.setToolTip("Start all connected devices")

    @QtCore.Slot(object)
    def device_is_recording_device_changed(self, device, is_recording_device):
        """
        When the is_recording_device bool changes, fresh the device status to force the repositioning
        of the device in the UI
        """
        self.device_status_changed(device, DeviceStatus.OPEN)

    def device_record_start_confirm(self, device):
        """
        Callback when the device has started recording
        """
        LOGGER.info(f'{device.name}: Recording started') # {timecode}

    def device_record_stop_confirm(self, device):
        """
        Callback when the device has stopped recording
        """
        LOGGER.info(f'{device.name}: Recording stopped')

        latest_recording = self.recording_manager.latest_recording
        device_recording = device.get_device_recording()

        # Add the device to the latest recording
        self.recording_manager.add_device_to_recording(device_recording, latest_recording)
        '''
        # TransportJob
        # If the device produces transport paths, create a transport job
        paths = device.transport_paths(device_recording)
        if not paths:
            return

        # If the status is not on device, do not create jobs
        if device_recording.status != recording.RecordingStatus.ON_DEVICE:
            return

        device_name = device_recording.device_name
        slate = latest_recording.slate
        take = latest_recording.take
        date = latest_recording.date
        job_name = self.transport_queue.job_name(slate, take, device_name)

        # Create a transport job
        transport_job = recording.TransportJob(job_name, device_name, slate, take, date, paths)
        self.transport_queue.add_transport_job(transport_job)
        '''

    @QtCore.Slot(object)
    def device_widget_trigger_start_toggled(self, device_widget, value):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.trigger_start = value

    @QtCore.Slot(object)
    def device_widget_trigger_stop_toggled(self, device_widget, value):
        device = self.device_manager.device_with_hash(device_widget.device_hash)
        device.trigger_stop = value

    def _console_pipe(self, msg):
        """
        Pipes the emiting message from the QtHandler to the base_console widget.
        Scrolls on each emit signal.
        :param msg: This is a built in event, QT Related, not given.
        """
        if self.window:
            self.window.base_console.appendHtml(msg)

        if self.logger_autoscroll:
            self.logger_scroll_to_end()

    def logger_scroll_to_end(self):
        # This combination keeps the cursor at the bottom left corner in all cases.
        if self.window:
            self.window.base_console.moveCursor(QtGui.QTextCursor.End)
            self.window.base_console.moveCursor(QtGui.QTextCursor.StartOfLine)

    # Allow user to change logging level
    def logger_level_comboBox_currentTextChanged(self):
        value = self.window.logger_level_comboBox.currentText()

        if value == 'Message':
            LOGGER.setLevel(logging.MESSAGE_LEVEL_NUM)
        elif value == 'OSC':
            LOGGER.setLevel(logging.OSC_LEVEL_NUM)
        elif value == 'Debug':
            LOGGER.setLevel(logging.DEBUG)
        else:
            LOGGER.setLevel(logging.INFO)

    def logger_autoscroll_stateChanged(self, value):
        if value == QtCore.Qt.Checked:
            self.logger_autoscroll = True
            self.logger_scroll_to_end()
        else:
            self.logger_autoscroll = False

    def logger_wrap_stateChanged(self, value):
        if value == QtCore.Qt.Checked:
            self.window.base_console.setLineWrapMode(QtWidgets.QPlainTextEdit.LineWrapMode.WidgetWidth)
        else:
            self.window.base_console.setLineWrapMode(QtWidgets.QPlainTextEdit.LineWrapMode.NoWrap)

        if self.logger_autoscroll:
            self.logger_scroll_to_end()

    # Update UI with latest CLs
    def p4_refresh_project_cl(self):
        if not CONFIG.P4_ENABLED.get_value():
            return
        LOGGER.info("Refreshing p4 project changelists")
        working_dir = os.path.dirname(CONFIG.UPROJECT_PATH.get_value())
        changelists = p4_utils.p4_latest_changelist(CONFIG.P4_PROJECT_PATH.get_value(), working_dir)
        self.window.project_cl_combo_box.clear()

        if changelists:
            self.window.project_cl_combo_box.addItems(changelists)
            self.window.project_cl_combo_box.setCurrentIndex(0)
        self.window.project_cl_combo_box.addItem(EMPTY_SYNC_ENTRY)

    def p4_refresh_engine_cl(self):
        if not CONFIG.P4_ENABLED.get_value():
            return
        self.window.engine_cl_combo_box.clear()
        # if engine is built from source, refresh the engine cl dropdown
        if CONFIG.BUILD_ENGINE.get_value():
            LOGGER.info("Refreshing p4 engine changelists")
            self.window.engine_cl_label.setEnabled(True)
            self.window.engine_cl_combo_box.setEnabled(True)
            self.window.engine_cl_combo_box.setToolTip("Select changelist to sync the engine to")
            self.window.refresh_engine_cl_button.setEnabled(True)
            self.window.refresh_engine_cl_button.setToolTip("Click to refresh changelists")

            engine_p4_path = CONFIG.P4_ENGINE_PATH.get_value()
            if engine_p4_path:
                working_dir = os.path.dirname(CONFIG.UPROJECT_PATH.get_value())
                changelists = p4_utils.p4_latest_changelist(engine_p4_path, working_dir)
                if changelists:
                    self.window.engine_cl_combo_box.addItems(changelists)
                    self.window.engine_cl_combo_box.setCurrentIndex(0)
            else:
                LOGGER.warning('"Build Engine" is enabled in the settings but the engine does not seem to be under perforce control.')
                LOGGER.warning("Please check your perforce settings.")
        else:
            # disable engine cl controls if engine is not built from source
            self.window.engine_cl_label.setEnabled(False)
            self.window.engine_cl_combo_box.setEnabled(False)
            tool_tip = "Engine is not build from source. To use this make sure the Engine is on p4 and 'Build Engine' "
            tool_tip += "is enabled in the Settings."
            self.window.engine_cl_combo_box.setToolTip(tool_tip)
            self.window.refresh_engine_cl_button.setEnabled(False)
            self.window.refresh_engine_cl_button.setToolTip(tool_tip)
        self.window.engine_cl_combo_box.addItem(EMPTY_SYNC_ENTRY)

    def refresh_levels(self, levels: Optional[List[str]] = None):
        if levels is None:
            levels = CONFIG.maps()

        current_level = CONFIG.CURRENT_LEVEL

        self.level_combo_box.clear()
        self._update_level_list(self.level_combo_box, levels)

        if current_level and current_level in levels:
            self.level = current_level

    def filter_empty_abiguated_path(path, file_name):
        path = path.removesuffix(file_name)
        if path == "/":
            return f"{file_name}"
        else:
            return f"{file_name} ({path})"

    def generate_short_map_path(path: str, file_name: str) -> str:
            path = path.replace("/Game", "", 1)
            return SwitchboardDialog.filter_empty_abiguated_path(path, file_name)
        
    def generate_disambiguated_names(path_list, shortening_function):
        name_counts = {}
        for path in path_list:
            file_name = os.path.basename(path)
            name_counts[file_name] = name_counts.get(file_name, 0) + 1
            
        # Show only level name if unique and show path behind to disambiguate duplicates
        short_name_list = []
        short_name_to_path = {}
        for path in path_list:
            file_name = os.path.basename(path)
            short_name = file_name if name_counts[file_name] == 1 else shortening_function(path, file_name)
            short_name_list.append(short_name)
            short_name_to_path[short_name] = path
            
        return short_name_list, short_name_to_path

    def _update_level_list(self, level_combo_box: sb_widgets.SearchableComboBox, level_path_list: List[str]):
        def compare_file_names(path_a: str, path_b: str):
            return -1 if path_a.lower() < path_b.lower() \
                else 1 if path_a.lower() > path_b.lower() else 0
        
        short_name_list, short_name_to_path = SwitchboardDialog.generate_disambiguated_names(level_path_list, SwitchboardDialog.generate_short_map_path)
            
        from functools import cmp_to_key
        short_name_list = sorted(short_name_list, key=cmp_to_key(compare_file_names))
        level_combo_box.addItems([DEFAULT_MAP_TEXT] + short_name_list)
        
        if len(level_path_list) == 0:
            return
        
        # To disambiguate, show the full path name in the drop-down
        level_combo_box.setItemData(0, "Default level")
        level_combo_box.setItemData(0, "Default level", QtCore.Qt.ToolTipRole)
        for short_name_index in range(len(short_name_list)):
            short_name = short_name_list[short_name_index]
            level_combo_box.setItemData(short_name_index + 1, short_name_to_path[short_name])
            level_combo_box.setItemData(short_name_index + 1, short_name_to_path[short_name], QtCore.Qt.ToolTipRole)

    def get_current_level_list(self):
        level_combo = self.level_combo_box
        return [self._get_level_from_combo_box(i) for i in range(1, level_combo.count())] # skip DEFAULT_MAP_TEXT

    def refresh_levels_incremental(self):
        ''' Wrapper around `refresh_levels` with the following differences:
            - Only resaves config if the selected level was removed (as opposed
              to always generating 2-3 change events/config saves).
            - Logs messages indicating which levels were added/removed.
        '''

        current_level: str = CONFIG.CURRENT_LEVEL
        prev_levels_list: List[str] = self.get_current_level_list()
        updated_levels_list: List[str] = CONFIG.maps()

        prev_levels: Set[str] = set(prev_levels_list)
        updated_levels: Set[str] = set(updated_levels_list)
        levels_added = updated_levels - prev_levels
        levels_removed = prev_levels - updated_levels

        if not (levels_added or levels_removed):
            return

        if levels_added:
            LOGGER.info(f'Levels added: {", ".join(levels_added)}')

        if levels_removed:
            LOGGER.info(f'Levels removed: {", ".join(levels_removed)}')

        if current_level in levels_removed:
            LOGGER.warning(f'Selected level "{current_level}" was removed; reverting to default')
            self.level = DEFAULT_MAP_TEXT

        CONFIG.push_saving_allowed(False)
        self.refresh_levels(updated_levels_list)
        CONFIG.pop_saving_allowed()


    def toggle_p4_controls(self, enabled):
        self.window.engine_cl_label.setEnabled(enabled)
        self.window.engine_cl_combo_box.setEnabled(enabled)
        self.window.refresh_engine_cl_button.setEnabled(enabled)

        self.window.project_cl_label.setEnabled(enabled)
        self.window.project_cl_combo_box.setEnabled(enabled)
        self.window.refresh_project_cl_button.setEnabled(enabled)

        self.window.sync_all_button.setEnabled(enabled)
        self.window.sync_and_build_all_button.setEnabled(enabled)
        if enabled:
            self.p4_refresh_engine_cl()
            self.p4_refresh_project_cl()

    def osc_take(self, address, command, value):
        device = self._device_from_address(address, command, value=value)

        if not device:
            return

        self._set_take(value, exclude_addresses=[device.address])

    # OSC: Set Slate
    def osc_slate(self, address, command, value):
        device = self._device_from_address(address, command, value=value)
        if not device:
            return

        self._set_slate(value, exclude_addresses=[device.address], reset_take=False)

    # OSC: Set Description UPDATE THIS TO MAKE IT WORK
    def osc_slate_description(self, address, command, value):
        self.description = value

    def record_button_released(self):
        """
        User press record button
        """
        if self._is_recording:
            LOGGER.debug('Record stop button pressed')
            self._record_stop(1)
            self.window.take_spin_box.setValue(self.window.take_spin_box.value() + 1)
        else:
            LOGGER.debug('Record start button pressed')
            self._record_start(self.slate, self.take, self.description)

    def _device_from_address(self, address, command, value=''):
        device = self.device_manager.device_with_address(address[0])

        if not device:
            LOGGER.warning(f'{address} is not registered with a device in Switchboard')
            return None

        # Do not receive osc from disconnected devices
        if device.is_disconnected:
            LOGGER.warning(f'{device.name}: is sending OSC commands but is not connected. Ignoring "{command} {value}"')
            return None

        LOGGER.osc(f'OSC Server: Received "{command} {value}" from {device.name} ({device.address})')
        return device

    # OSC: Start a recording
    def osc_record_start(self, address, command, slate, take, description):
        '''
        OSC message Recieved /RecordStart
        '''
        device = self._device_from_address(address, command, value=[slate, take, description])
        if not device:
            return

        # There is a bug that causes a slate of None. If this occurs, use the stored slate in control
        # Try to track down this bug in sequencer
        if not slate or slate == 'None':
            LOGGER.critical(f'Slate is None, using {self.slate}')
        else:
            self.slate = slate

        self.take = take
        self.description = description

        self._record_start(self.slate, self.take, self.description, exclude_address=device.address)

    def _record_start(self, slate, take, description, exclude_address=None):
        LOGGER.success(f'Record Start: "{self.slate}" {self.take}')

        # Update the UI button
        pixmap = QtGui.QPixmap(":/icons/images/record_start.png")
        self.window.record_button.setIcon(QtGui.QIcon(pixmap))

        # Pause the TransportQueue
        #self.transport_queue.pause()

        # Start a new recording
        self._is_recording = True

        # TODO: Lock SLATE/TAKE/SESSION/CL

        # Return a Recording object
        new_recording = recording.Recording()
        new_recording.project = CONFIG.PROJECT_NAME.get_value()
        new_recording.shoot = self.shoot
        new_recording.sequence = self.sequence
        new_recording.slate = self.slate
        new_recording.take = self.take
        new_recording.description = self.description
        new_recording.date = switchboard_utils.date_to_string(datetime.date.today())
        new_recording.map = self.level
        new_recording.multiuser_session = self.multiuser_session_name()
        new_recording.changelist = self.project_changelist

        self.recording_manager.add_recording(new_recording)

        # Sends the message to all recording devices
        devices = self.device_manager.devices()
        for device in devices:
            # Do not send a start record message to whichever device sent it
            if exclude_address and exclude_address == device.address:
                continue

            # Do not send updates to disconnected devices
            if device.is_disconnected:
                continue

            LOGGER.debug(f'Record Start {device.name}')
            device.record_start(slate, take, description)

    def _record_stop(self, exclude_address=None):
        LOGGER.success(f'Record Stop: "{self.slate}" {self.take}')

        pixmap = QtGui.QPixmap(":/icons/images/record_stop.png")
        self.window.record_button.setIcon(QtGui.QIcon(pixmap))

        # Resume the transport queue
        #self.transport_queue.resume()

        # End the recording
        self._is_recording = False

        # Pull the latest recording down
        new_recording = self.recording_manager.latest_recording
        # Start the auto_save timer
        self.recording_manager.auto_save(new_recording)

        # TODO: If no UE4 auto incriment 

        # TODO: Unlock SLATE/TAKE/SESSION/CL

        devices = self.device_manager.devices()

        # Sends the message to all recording devices
        for device in devices:
            # Do not send a start record message to whichever device sent it
            if exclude_address and exclude_address == device.address:
                continue

            # Do not send updates to disconnected devices
            if device.is_disconnected:
                continue

            device.record_stop()

    def _record_cancel(self, exclude_address=None):
        self._record_stop(exclude_address=exclude_address)

        # Increment Take
        #new_recording = self.recording_manager.latest_recording
        #self.take = new_recording.take + 1

    def osc_record_start_confirm(self, address, command, timecode):
        device = self._device_from_address(address, command, value=timecode)
        if not device:
            return

        device.record_start_confirm(timecode)

    def osc_record_stop(self, address, command):
        device = self._device_from_address(address, command)
        if not device:
            return

        self._record_stop(exclude_address=device.address)

    def osc_record_stop_confirm(self, address, command, timecode, *paths):
        device = self._device_from_address(address, command, value=timecode)
        if not device:
            return

        if not paths:
            paths = None
        device.record_stop_confirm(timecode, paths=paths)

    def osc_record_cancel(self, address, command):
        """
        This is called when record has been pressed and stopped before the countdown in take recorder
        has finished
        """
        device = self._device_from_address(address, command)
        if not device:
            return

        self._record_cancel(exclude_address=device.address)

    def osc_record_cancel_confirm(self, address, command, timecode):
        pass
        #device = self._device_from_address(address, command, value=timecode)
        #if not device:
        #    return

        #self.record_cancel_confirm(device, timecode)

    def osc_ue4_launch_confirm(self, address, command):
        device = self._device_from_address(address, command)
        if not device:
            return

        # If the device is already ready, bail
        if device.status == DeviceStatus.READY:
            return

        # Set the device status to ready
        device.status = DeviceStatus.READY

    def osc_add_send_target_confirm(self, address, command, value):
        device = self.device_manager.device_with_address(address[0])
        if not device:
            return

        device.osc_add_send_target_confirm()

    def osc_arsession_stop_confirm(self, address, command, value):
        LOGGER.debug(f'osc_arsession_stop_confirm {value}')

    def osc_arsession_start_confirm(self, address, command, value):
        device = self._device_from_address(address, command, value=value)
        if not device:
            return

        device.connect_listener()

    def osc_battery(self, address, command, value):
        # The Battery command is used to handshake with LiveLinkFace. Don't reject it if it's not connected 
        device = self.device_manager.device_with_address(address[0])
        
        if not device:
            return

        # Update the device
        device.battery = value

        # Update the UI
        device_widget = self.device_list_widget.device_widget_by_hash(device.device_hash)
        device_widget.set_battery(value)

    def osc_data(self, address, command, value):
        device = self._device_from_address(address, command)
        if not device:
            return


class TransportQueueHeaderActionWidget(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)

        self.layout = QtWidgets.QHBoxLayout()
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.setLayout(self.layout)

        def __label(label_text):
            label = QtWidgets.QLabel()
            label.setText(label_text)
            label.setObjectName('widget_header')
            return label

        self.name_label = __label('Transport Queue')

        self.layout.addWidget(self.name_label)
    
    def paintEvent(self, event):
        opt = QtWidgets.QStyleOption()
        opt.initFrom(self)
        painter = QtGui.QPainter(self)
        self.style().drawPrimitive(QtWidgets.QStyle.PE_Widget, opt, painter, self)


class TransportQueueActionWidget(QtWidgets.QWidget):
    def __init__(self, name, parent=None):
        super().__init__(parent)

        self.layout = QtWidgets.QHBoxLayout()
        self.layout.setContentsMargins(20, 2, 20, 2)
        self.layout.setSpacing(2)
        self.setLayout(self.layout)

        # Job Label
        label = QtWidgets.QLabel(name)
        self.layout.addWidget(label)

        spacer = QtWidgets.QSpacerItem(0, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.layout.addItem(spacer)

        # Remove button
        button = sb_widgets.ControlQPushButton()
        sb_widgets.set_qt_property(button, 'no_background', True)

        icon = QtGui.QIcon()
        pixmap = QtGui.QPixmap(f":/icons/images/icon_close_disabled.png")
        icon.addPixmap(pixmap, QtGui.QIcon.Normal, QtGui.QIcon.Off)
        pixmap = QtGui.QPixmap(f":/icons/images/icon_close.png")
        icon.addPixmap(pixmap, QtGui.QIcon.Normal, QtGui.QIcon.On)
        pixmap = QtGui.QPixmap(f":/icons/images/icon_close_hover.png")
        icon.addPixmap(pixmap, QtGui.QIcon.Active, QtGui.QIcon.Off)

        button.setIcon(icon)
        button.setIconSize(pixmap.rect().size()*0.75)
        button.setMinimumSize(QtCore.QSize(20, 20))

        button.setCheckable(False)
        #button.clicked.connect(self.device_button_clicked)
        self.layout.addWidget(button)

    def test(self):
        LOGGER.debug('BOOM!')
    
    def paintEvent(self, event):
        opt = QtWidgets.QStyleOption()
        opt.initFrom(self)
        painter = QtGui.QPainter(self)
        self.style().drawPrimitive(QtWidgets.QStyle.PE_Widget, opt, painter, self)
