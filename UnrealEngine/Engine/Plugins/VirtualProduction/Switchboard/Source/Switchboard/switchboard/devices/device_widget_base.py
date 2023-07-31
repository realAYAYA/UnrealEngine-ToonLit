# Copyright Epic Games, Inc. All Rights Reserved.
from typing import Dict, Optional

from PySide2 import QtWidgets, QtGui, QtCore

from switchboard.config import CONFIG
from switchboard.switchboard_widgets import FramelessQLineEdit
from switchboard.devices.device_base import DeviceStatus
import switchboard.switchboard_widgets as sb_widgets


class DeviceWidgetItem(QtWidgets.QWidget):
    """
    Custom class to get QSS working correctly to achieve a look.
    This allows the QSS to set the style of the DeviceWidgetItem and change its color when recording
    """
    def __init__(self, parent=None):
        super().__init__(parent)

    def paintEvent(self, event):
        opt = QtWidgets.QStyleOption()
        opt.initFrom(self)
        painter = QtGui.QPainter(self)
        self.style().drawPrimitive(QtWidgets.QStyle.PE_Widget, opt, painter, self)


class DeviceAutoJoinMUServerUI(QtCore.QObject):
    signal_device_widget_autojoin_mu = QtCore.Signal(object)

    autojoin_mu_default = True

    def __init__(self, name, parent = None):
        super().__init__(parent)
        self.name = name

    def is_autojoin_enabled(self):
        return self._button.isChecked()

    def set_autojoin_mu(self, is_checked):
        if is_checked is not self.is_autojoin_enabled():
            self._button.setChecked(is_checked)
            self._set_autojoin_mu(is_checked)

    def _set_autojoin_mu(self, is_checked):
        self.signal_device_widget_autojoin_mu.emit(self)

    def get_button(self):
        return self._button

    def disable_enable_based_on_global(self):
        self._button.setEnabled(CONFIG.MUSERVER_AUTO_JOIN.get_value())

    def make_button(self, parent):
        """
        Make a new device setting push button.
        """
        self._button = sb_widgets.ControlQPushButton.create(
                icon_size=QtCore.QSize(15, 15),
                tool_tip=f'Toggle Auto-join for Multi-user Server',
                hover_focus=False,
                name='autojoin'
        )

        self.set_autojoin_mu(self.autojoin_mu_default)
        self._button.toggled.connect(self._set_autojoin_mu)
        CONFIG.MUSERVER_AUTO_JOIN.signal_setting_changed.connect(self.disable_enable_based_on_global)
        self.disable_enable_based_on_global()
        return self._button


class DeviceWidget(QtWidgets.QWidget):
    signal_device_widget_connect = QtCore.Signal(object)
    signal_device_widget_disconnect = QtCore.Signal(object)
    signal_device_widget_open = QtCore.Signal(object)
    signal_device_widget_close = QtCore.Signal(object)
    signal_device_widget_sync = QtCore.Signal(object)
    signal_device_widget_build = QtCore.Signal(object)
    signal_device_widget_trigger_start_toggled = QtCore.Signal(object, bool)
    signal_device_widget_trigger_stop_toggled = QtCore.Signal(object, bool)

    signal_device_name_changed = QtCore.Signal(str)
    signal_address_changed = QtCore.Signal(str)

    hostname_validator = sb_widgets.HostnameValidator()

    def __init__(self, name, device_hash, address, icons, parent=None):
        super().__init__(parent)

        # Lookup device by a hash instead of name/address
        self.device_hash = device_hash
        self.icons = icons

        # Status Label
        self.status_icon = QtWidgets.QLabel()
        self.status_icon.setGeometry(0, 0, 11, 1)
        pixmap = QtGui.QPixmap(":/icons/images/status_blank_disabled.png")
        self.status_icon.setPixmap(pixmap)
        self.status_icon.resize(pixmap.width(), pixmap.height())

        # Device icon
        self.device_icon = QtWidgets.QLabel()
        self.device_icon.setGeometry(0, 0, 40, 40)
        pixmap = self.icon_for_state("enabled").pixmap(QtCore.QSize(40, 40))
        self.device_icon.setPixmap(pixmap)
        self.device_icon.resize(pixmap.width(), pixmap.height())
        self.device_icon.setMinimumSize(QtCore.QSize(60, 40))
        self.device_icon.setAlignment(QtCore.Qt.AlignCenter)

        self.name_validator = None

        # Device name
        self.name_line_edit = FramelessQLineEdit()
        self.name_line_edit.textChanged[str].connect(self.on_name_changed)
        self.name_line_edit.editingFinished.connect(self.on_name_edited)

        self.name_line_edit.setText(name)
        self.name_line_edit.setObjectName('device_name')
        self.name_line_edit.setMaximumSize(QtCore.QSize(150, 40))
        # 20 + 11 + 60 + 150

        # Address Label
        self.address_line_edit = FramelessQLineEdit()
        self.address_line_edit.setObjectName('device_address')
        self.address_line_edit.setValidator(DeviceWidget.hostname_validator)
        self.address_line_edit.editingFinished.connect(self.on_address_edited)
        self.address_line_edit.setText(address)
        self.address_line_edit.setAlignment(QtCore.Qt.AlignCenter)
        self.address_line_edit.setMaximumSize(QtCore.QSize(100, 40))

        # Create a widget where the body of the item will go
        # This is made to allow the edit buttons to sit "outside" of the item
        self.widget = DeviceWidgetItem()
        self.edit_layout = QtWidgets.QHBoxLayout()
        self.edit_layout.setContentsMargins(0,0,0,0)
        self.setLayout(self.edit_layout)
        self.edit_layout.addWidget(self.widget)

        # Main layout where the contents of the item will live
        self.layout = QtWidgets.QHBoxLayout()
        self.layout.setContentsMargins(20, 2, 20, 2)
        self.layout.setSpacing(2)
        self.widget.setLayout(self.layout)

        self.add_widget_to_layout(self.status_icon)
        self.add_widget_to_layout(self.device_icon)
        self.add_widget_to_layout(self.name_line_edit)
        self.add_widget_to_layout(self.address_line_edit)

        spacer = QtWidgets.QSpacerItem(0, 20, QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Minimum)
        self.add_item_to_layout(spacer)

        # Store previous status for efficiency
        #self.previous_status = DeviceStatus.DISCONNECTED

        # Set style as disconnected
        for label in [self.name_line_edit, self.address_line_edit]:
            sb_widgets.set_qt_property(label, 'disconnected', True)

        # Store the control buttons by name ("connect", "open", etc.)
        self.control_buttons: Dict[str, sb_widgets.ControlQPushButton] = {}

        self._add_control_buttons()

        self.help_tool_tip = QtWidgets.QToolTip()

    def add_widget_to_layout(self, widget):
        ''' Adds a widget to the layout '''

        self.layout.addWidget(widget)

    def add_item_to_layout(self, item):
        ''' Adds an item to the layout '''

        self.layout.addItem(item)

    def can_sync(self):
        return False

    def can_build(self):
        return False

    def icon_for_state(self, state):
        if state in self.icons.keys():
            return self.icons[state]
        else:
            if "enabled" in self.icons.keys():
                return self.icons["enabled"]
            else:
                return QtGui.QIcon()

    def set_name_validator(self, name_validator):
        self.name_validator = name_validator

    def on_name_changed(self, text):
        if not self.name_validator:
            return
        if self.name_validator.validate(text, self.name_line_edit.cursorPosition()) == QtGui.QValidator.Invalid:
            rect = self.name_line_edit.parent().mapToGlobal(self.name_line_edit.geometry().topRight())
            self.help_tool_tip.showText(rect, "Names must be unique")

            sb_widgets.set_qt_property(self.name_line_edit, "input_error", True)
            self.name_line_edit.is_valid = False
        else:
            self.name_line_edit.is_valid = True
            sb_widgets.set_qt_property(self.name_line_edit, "input_error", False)
            self.help_tool_tip.hideText()

    def on_name_edited(self):
        new_value = self.name_line_edit.text()

        if self.name_line_edit.is_valid and self.name_line_edit.current_text != new_value:
            sb_widgets.set_qt_property(self.name_line_edit, "input_error", False)

            self.signal_device_name_changed.emit(new_value)

    def on_address_edited(self):
        new_value = self.address_line_edit.text()

        if self.address_line_edit.is_valid and self.address_line_edit.current_text != new_value:
            sb_widgets.set_qt_property(self.address_line_edit, "input_error", False)

            self.signal_address_changed.emit(new_value)

    def on_address_changed(self, new_address):
        self.address_line_edit.setText(new_address)

    def _add_control_buttons(self):
        pass

    def update_status(self, status, previous_status):
        # Status Icon
        if status >= DeviceStatus.READY:
            self.status_icon.setPixmap(QtGui.QPixmap(":/icons/images/status_green.png"))
            self.status_icon.setToolTip("Ready to start recording")
        elif status == DeviceStatus.DISCONNECTED:
            pixmap = QtGui.QPixmap(":/icons/images/status_blank_disabled.png")
            self.status_icon.setPixmap(pixmap)
            self.status_icon.setToolTip("Disconnected")
        elif status == DeviceStatus.CONNECTING:
            pixmap = QtGui.QPixmap(":/icons/images/status_orange.png")
            self.status_icon.setPixmap(pixmap)
            self.status_icon.setToolTip("Connecting...")
        elif status == DeviceStatus.OPEN:
            pixmap = QtGui.QPixmap(":/icons/images/status_orange.png")
            self.status_icon.setPixmap(pixmap)
            self.status_icon.setToolTip("Device has been started")
        else:
            self.status_icon.setPixmap(QtGui.QPixmap(":/icons/images/status_cyan.png"))
            self.status_icon.setToolTip("Connected")

        # Device icon
        if status in {DeviceStatus.DISCONNECTED, DeviceStatus.CONNECTING}:
            for label in [self.name_line_edit, self.address_line_edit]:
                sb_widgets.set_qt_property(label, 'disconnected', True)

            pixmap = self.icon_for_state("disabled").pixmap(QtCore.QSize(40, 40))
            self.device_icon.setPixmap(pixmap)

            if status == DeviceStatus.DISCONNECTED:
                # Make the name and address editable when disconnected.
                self.name_line_edit.setReadOnly(False)
                self.address_line_edit.setReadOnly(False)
            elif status == DeviceStatus.CONNECTING:
                # Make the name and address non-editable while connecting.
                self.name_line_edit.setReadOnly(True)
                self.address_line_edit.setReadOnly(True)
        elif ((previous_status in {DeviceStatus.DISCONNECTED, DeviceStatus.CONNECTING}) and
                status > DeviceStatus.CONNECTING):
            for label in [self.name_line_edit, self.address_line_edit]:
                sb_widgets.set_qt_property(label, 'disconnected', False)

            pixmap = self.icon_for_state("enabled").pixmap(QtCore.QSize(40, 40))
            self.device_icon.setPixmap(pixmap)

            # Make the name and address non-editable when connected.
            self.name_line_edit.setReadOnly(True)
            self.address_line_edit.setReadOnly(True)

        # Handle coloring List Widget items if they are recording
        if status == DeviceStatus.RECORDING:
            sb_widgets.set_qt_property(self.widget, 'recording', True)
        else:
            sb_widgets.set_qt_property(self.widget, 'recording', False)

    def resizeEvent(self, event):
        super().resizeEvent(event)

        width = event.size().width()

        if width < sb_widgets.DEVICE_WIDGET_HIDE_ADDRESS_WIDTH:
            self.address_line_edit.hide()
        else:
            self.address_line_edit.show()

    def assign_button_to_name(self, name, button):
        if name:
            self.control_buttons[name] = button

    def add_control_button(self, *args, name: Optional[str] = None, **kwargs):
        button = sb_widgets.ControlQPushButton.create(*args, name=name,
                                                      **kwargs)
        self.add_widget_to_layout(button)

        self.assign_button_to_name(name, button)
        return button

    def populate_context_menu(self, cmenu: QtWidgets.QMenu):
        ''' Called to populate the given context menu with any desired actions'''
        pass


class AddDeviceDialog(QtWidgets.QDialog):
    def __init__(self, device_type, existing_devices, parent=None):
        super().__init__(parent=parent, f=QtCore.Qt.WindowCloseButtonHint)

        self.device_type = device_type
        self.setWindowTitle(f"Add {self.device_type} Device")

        self.name_field = QtWidgets.QLineEdit(self)

        self.address_field = QtWidgets.QLineEdit(self)
        self.address_field.setValidator(DeviceWidget.hostname_validator)

        self.form_layout = QtWidgets.QFormLayout()
        self.form_layout.addRow("Name", self.name_field)
        self.form_layout.addRow("Address", self.address_field)

        layout = QtWidgets.QVBoxLayout()
        layout.insertLayout(0, self.form_layout)

        button_box = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        button_box.accepted.connect(lambda: self.accept())
        button_box.rejected.connect(lambda: self.reject())
        layout.addWidget(button_box)

        self.setLayout(layout)

    def add_name_validator(self, validator):
        if self.name_field:
            self.name_field.setValidator(validator)

    def devices_to_add(self):
        return [{"type": self.device_type, "name": self.name_field.text(), "address": self.address_field.text(), "kwargs": {}}]

    def devices_to_remove(self):
        return []
