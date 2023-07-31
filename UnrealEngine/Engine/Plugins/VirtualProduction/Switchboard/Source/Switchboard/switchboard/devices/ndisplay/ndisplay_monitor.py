# Copyright Epic Games, Inc. All Rights Reserved.

from collections import OrderedDict
from itertools import count
import time
import traceback

from PySide2 import QtCore
from PySide2.QtCore import QAbstractTableModel, QModelIndex, Qt, QTimer
from PySide2.QtGui import QColor

from switchboard import message_protocol
from switchboard.switchboard_logging import LOGGER


class nDisplayMonitor(QAbstractTableModel):
    '''
    This will monitor the status of the nDisplay nodes, in particular regarding
    sync. It polls the listener at the specified rate and the UI should update
    with this info.
    '''

    COLOR_WARNING = QColor(0x70, 0x40, 0x00)
    COLOR_NORMAL = QColor(0x3d, 0x3d, 0x3d)
    CORE_OVERLOAD_THRESH = 90  # percent utilization

    # Special meaning when received from listener for PresentMode, but
    # sometimes also used for other values' display for consistency.
    DATA_MISSING = 'n/a'

    def __init__(self, parent):
        QAbstractTableModel.__init__(self, parent)

        self.polling_period_ms = 1000

        # ordered so that we can map row indices to devices
        self.devicedatas = OrderedDict()

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.poll_sync_status)

        HEADER_DATA = {
            'Node': 'The cluster name of this device',
            'Host': 'The URL of the remote PC',
            'Connected': 'If we are connected to the listener of this device',
            'Driver': 'GPU driver version',
            'PresentMode':
                'Current presentation mode. Only available once the render '
                'node process is running. Expects "Hardware Composed: '
                'Independent Flip"',
            'Gpus': 'Informs if GPUs are synced.',
            'Displays': 'Detected displays and whether they are in sync',
            'SyncRate': 'Sync Frame Rate',
            'HouseSync':
                'Presence of an external sync signal connected to the remote '
                'Quadro Sync card',
            'SyncSource': 'The source of the GPU sync signal',
            'Mosaics': 'Display grids and their resolutions',
            'Taskbar':
                'Whether the taskbar is set to auto hide or always on top. It '
                'is recommended to be consistent across the cluster',
            'InFocus':
                'Whether nDisplay instance window is in focus. It is '
                'recommended to be in focus.',
            'ExeFlags':
                'It is recommended to disable fullscreen optimizations in the\n'
                'unreal executable because it has been associated with tearing.\n'
                'Only available once the render node process is running. \n'
                '(Expected value is "DISABLEDXMAXIMIZEDWINDOWEDMODE")',
            'OsVer': 'Operating system version',
            'CpuUtilization':
                'CPU utilization average. The number of overloaded cores (>'
                f'{self.CORE_OVERLOAD_THRESH}% load) will be displayed in '
                'parentheses.',
            'MemUtilization': 'Physical memory, utilized / total.',
            'GpuUtilization':
                'GPU utilization. The GPU clock speed is displayed in '
                'parentheses.',
            'GpuTemperature':
                'GPU temperature in degrees celsius. (Max across all '
                'sensors.)',
        }

        self.colnames = list(HEADER_DATA.keys())
        self.tooltips = list(HEADER_DATA.values())

    def color_for_column(self, colname, value, data):
        ''' Returns the background color for the given cell '''
        if data['Connected'].lower() == 'no':
            if colname == 'Connected':
                return self.COLOR_WARNING
            return self.COLOR_NORMAL

        if colname == 'PresentMode':
            is_good = 'Hardware Composed: Independent Flip' in value
            return self.COLOR_NORMAL if is_good else self.COLOR_WARNING

        if colname == 'Gpus':
            is_synced = ('Synced' in value) and ('Free' not in value)
            return self.COLOR_NORMAL if is_synced else self.COLOR_WARNING

        if colname == 'InFocus':
            return self.COLOR_NORMAL if 'yes' in value else self.COLOR_WARNING

        if colname == 'ExeFlags':
            is_good = 'DISABLEDXMAXIMIZEDWINDOWEDMODE' in value
            return self.COLOR_NORMAL if is_good else self.COLOR_WARNING

        if colname == 'Displays':
            is_normal = (('Follower' in value or 'Leader' in value)
                         and ('Unsynced' not in value))
            return self.COLOR_NORMAL if is_normal else self.COLOR_WARNING

        if colname == 'CpuUtilization':
            no_overload = '(' not in value  # "(# cores > threshold%)"
            return self.COLOR_NORMAL if no_overload else self.COLOR_WARNING

        return self.COLOR_NORMAL

    def friendly_osver(self, device):
        ''' Returns a display-friendly string for the device OS version '''
        if device.os_version_label.startswith('Windows'):
            try:
                # Destructuring according to FWindowsPlatformMisc::GetOSVersion
                # (ex: "10.0.19041.1.256.64bit")
                [major, minor, build, product_type, suite_mask, arch] = \
                    device.os_version_number.split('.')

                if major == '10' and minor == '0':
                    # FWindowsPlatformMisc::GetOSVersions (with an s) returns a
                    # label that includes this "release ID"/"SDK version", but
                    # the mechanism for retrieving it seems fragile.
                    # Based on: https://docs.microsoft.com/en-us/windows/release-health/release-information  # noqa
                    BUILD_TO_SDK_VERSION = {
                        '19044': '21H2',
                        '19043': '21H1',
                        '19042': '20H2',
                        '19041': '2004',
                        '18363': '1909',
                        '17763': '1809',
                        '17134': '1803',
                        '14393': '1607',
                        '10240': '1507',
                    }

                    sdk_version = BUILD_TO_SDK_VERSION.get(build)
                    if sdk_version:
                        return f'Windows 10, version {sdk_version}'

            except ValueError:
                # Mismatched count of splits vs. destructure
                pass  # Fall through to default

        # Default / fallback
        friendly = device.os_version_label

        if device.os_version_label_sub != '':
            friendly += " " + device.os_version_label_sub

        if device.os_version_number != '':
            friendly += " " + device.os_version_number

        if friendly == '':
            friendly = self.DATA_MISSING

        return friendly

    def reset_device_data(self, device, data):
        ''' Sets device data to unconnected state '''
        for colname in self.colnames:
            data[colname] = self.DATA_MISSING

        data['Host'] = str(device.address)
        data['Node'] = device.name
        data['Connected'] = \
            'yes' if device.unreal_client.is_connected else 'no'

        # extra data not in columns
        data['TimeLastFlipGlitch'] = time.time()

    def added_device(self, device):
        ''' Called by the plugin when a new nDisplay device has been added. '''
        data = {}

        self.reset_device_data(device, data)
        self.devicedatas[device.device_hash] = {
            'device': device, 'data': data, 'time_last_update': 0,
            'stale': True
        }

        # notify the UI of the change
        self.layoutChanged.emit()

        # start/continue polling since there is at least one device
        if not self.timer.isActive():
            self.timer.start(self.polling_period_ms)

    def removed_device(self, device):
        ''' Called by the plugin when an nDisplay device has been removed. '''
        self.devicedatas.pop(device.device_hash)

        # notify the UI of the change
        self.layoutChanged.emit()

        # turn off the timer if there are no devices left
        if not self.devicedatas:
            self.timer.stop()

    def handle_stale_device(self, devicedata, deviceIdx):
        '''
        Detects if the device is stale, and resets the data if so as to not
        mislead the user.
        '''
        # if already flagged as stale, no need to do anything
        if devicedata['stale']:
            return

        # check if it has been too long
        time_elapsed_since_last_update = \
            time.time() - devicedata['time_last_update']

        timeout_factor = 4
        timeout = timeout_factor * self.polling_period_ms * 1e-3
        if time_elapsed_since_last_update < timeout:
            return

        # if we're here, it has been too long since the last update
        self.reset_device_data(devicedata['device'], devicedata['data'])
        devicedata['stale'] = True

        # notify the UI
        row = deviceIdx + 1
        self.dataChanged.emit(self.createIndex(row, 1),
                              self.createIndex(row, len(self.colnames)))

    def handle_connection_change(self, devicedata, deviceIdx):
        '''
        Detects if device connection changed and notifies the UI if a
        disconnection happens.
        '''
        device = devicedata['device']
        data = devicedata['data']

        is_connected = device.unreal_client.is_connected
        was_connected = True if data['Connected'] == 'yes' else False

        data['Connected'] = 'yes' if is_connected else 'no'

        if was_connected != is_connected:
            if not is_connected:
                self.reset_device_data(device, data)

            row = deviceIdx + 1
            self.dataChanged.emit(self.createIndex(row, 1),
                                  self.createIndex(row, len(self.colnames)))

    def poll_sync_status(self):
        ''' Polls sync status for all nDisplay devices '''
        for deviceIdx, devicedata in enumerate(self.devicedatas.values()):
            device = devicedata['device']

            # detect connection changes (a disconnection invalidates data)
            self.handle_connection_change(devicedata, deviceIdx)

            # detect stale devices
            self.handle_stale_device(devicedata, deviceIdx)

            # no point in continuing of not connected to listener
            if not device.unreal_client.is_connected:
                continue

            # create message
            try:
                program_id = device.program_start_queue.running_puuids_named(
                    'unreal')[-1]
            except IndexError:
                program_id = '00000000-0000-0000-0000-000000000000'

            _, msg = message_protocol.create_get_sync_status_message(
                program_id)

            # send get sync status message
            device.unreal_client.send_message(msg)

    def devicedata_from_device(self, device):
        ''' Retrieves the devicedata and index for given device '''
        for device_idx, hash_devicedata in enumerate(self.devicedatas.items()):
            device_hash, devicedata = hash_devicedata[0], hash_devicedata[1]
            if device_hash == device.device_hash:
                return (device_idx, devicedata)

        raise KeyError

    def populate_sync_data(self, devicedata, message):
        '''
        Populates model data with message contents, which comes from 'get sync
        data' command.
        '''
        data = devicedata['data']
        device = devicedata['device']

        #
        # Sync Topology
        #
        sync_status = message['syncStatus']
        sync_topos = sync_status['syncTopos']

        # Build list informing which Gpus in each Sync group are in sync
        gpus = []

        for sync_topo in sync_topos:
            gpu_sync_oks = [gpu['bIsSynced'] for gpu in sync_topo['syncGpus']]
            gpu_syncs = map(lambda x: "Synced" if x else 'Free', gpu_sync_oks)
            gpus.append('%s' % (', '.join(gpu_syncs)))

        data['Gpus'] = '\n'.join(gpus) if len(gpus) > 0 else self.DATA_MISSING

        # Build list informing which Display in each Sync group are in sync.
        displays = []

        bpc_strings = {1: 6, 2: 8, 3: 10, 4: 12, 5: 16}

        for sync_topo in sync_topos:
            display_sync_states = [
                f"{syncDisplay['syncState']}"
                f"({bpc_strings.get(syncDisplay['bpc'], '??')}bpc)"
                for syncDisplay in sync_topo['syncDisplays']]
            displays.append(', '.join(display_sync_states))

        if len(displays) > 0:
            data['Displays'] = '\n'.join(displays)
        else:
            data['Displays'] = self.DATA_MISSING

        # Build Fps
        refresh_rates = \
            [f"{syncTopo['syncStatusParams']['refreshRate']*1e-4:.3f}"
                for syncTopo in sync_topos]

        if len(refresh_rates) > 0:
            data['Fps'] = '\n'.join(refresh_rates)
        else:
            data['Fps'] = self.DATA_MISSING

        # Build House Sync
        house_fpss = [syncTopo['syncStatusParams']['houseSyncIncoming']*1e-4
                      for syncTopo in sync_topos]
        house_syncs = [syncTopo['syncStatusParams']['bHouseSync']
                       for syncTopo in sync_topos]
        house_sync_fpss = list(map(lambda x: f"{x[1]:.3f}" if x[0] else 'no',
                               zip(house_syncs, house_fpss)))

        if len(house_sync_fpss) > 0:
            data['HouseSync'] = '\n'.join(house_sync_fpss)
        else:
            data['HouseSync'] = self.DATA_MISSING

        # Build Sync Source
        source_str = {0: 'Vsync', 1: 'House'}
        sync_sources = [sync_topo['syncControlParams']['source']
                        for sync_topo in sync_topos]
        sync_sources = [source_str.get(sync_source, 'Unknown')
                        for sync_source in sync_sources]
        bInternalSecondaries = [
            sync_topo['syncStatusParams']['bInternalSecondary']
            for sync_topo in sync_topos]

        sync_followers = []

        for i in range(len(sync_sources)):
            if bInternalSecondaries[i] and sync_sources[i] == 'Vsync':
                sync_followers.append('Vsync(daisy)')
            else:
                sync_followers.append(sync_sources[i])

        if len(sync_followers) > 0:
            data['SyncSource'] = '\n'.join(sync_followers)
        else:
            data['SyncSource'] = self.DATA_MISSING

        # Mosaic Topology
        mosaic_topos = sync_status['mosaicTopos']

        mosaic_topo_lines = []

        for mosaic_topo in mosaic_topos:
            display_settings = mosaic_topo['displaySettings']
            width_per_display = display_settings['width']
            height_per_display = display_settings['height']

            width = mosaic_topo['columns'] * width_per_display
            height = mosaic_topo['rows'] * height_per_display

            # Ignoring displaySettings['freq'] because it seems to be fixed
            # and ignores sync frequency.
            line = f"{width}x{height} {display_settings['bpp']}bpp"
            mosaic_topo_lines.append(line)

        data['Mosaics'] = '\n'.join(mosaic_topo_lines)

        # Build PresentMode.
        flip_history = sync_status['flipModeHistory']

        if len(flip_history) > 0:
            data['PresentMode'] = flip_history[-1]

        # Detect PresentMode glitches
        if len(set(flip_history)) > 1:
            data['PresentMode'] = 'GLITCH!'
            data['TimeLastFlipGlitch'] = time.time()

        # Write time since last glitch
        if data['PresentMode'] != self.DATA_MISSING:
            time_since_flip_glitch = time.time() - data['TimeLastFlipGlitch']

            # For 1 minute, let the user know that there was a flip mode glitch
            if time_since_flip_glitch < 1*60:
                data['PresentMode'] = data['PresentMode'].split('\n')[0] \
                    + '\n' + str(int(time_since_flip_glitch))

        # Window in focus or not
        data['InFocus'] = 'no'
        for prg in device.program_start_queue.running_programs_named('unreal'):
            if prg.pid and prg.pid == sync_status['pidInFocus']:
                data['InFocus'] = 'yes'
                break

        # Show Exe flags (like Disable Fullscreen Optimization)
        data['ExeFlags'] = '\n'.join([
            layer for layer in sync_status['programLayers'][1:]])

        # Driver version
        try:
            driver = sync_status['driverVersion']
            data['Driver'] = f'{int(driver/100)}.{driver % 100}'
        except (KeyError, TypeError):
            data['Driver'] = self.DATA_MISSING

        # Taskbar visibility
        data['Taskbar'] = sync_status.get('taskbar', self.DATA_MISSING)

        # Operating system version
        data['OsVer'] = self.friendly_osver(device)

        # CPU utilization
        try:
            num_cores = len(sync_status['cpuUtilization'])
            num_overloaded_cores = 0
            cpu_load_avg = 0.0
            for core_load in sync_status['cpuUtilization']:
                cpu_load_avg += float(core_load) * (1.0 / num_cores)
                if core_load > self.CORE_OVERLOAD_THRESH:
                    num_overloaded_cores += 1

            data['CpuUtilization'] = f"{cpu_load_avg:.0f}%"
            if num_overloaded_cores > 0:
                data['CpuUtilization'] += f' ({num_overloaded_cores} cores >' \
                    f' {self.CORE_OVERLOAD_THRESH}%)'
        except (KeyError, ValueError):
            data['CpuUtilization'] = self.DATA_MISSING

        # Memory utilization
        try:
            gb = 1024 * 1024 * 1024
            mem_total = device.total_phys_mem
            mem_avail = sync_status.get('availablePhysicalMemory', 0)
            mem_utilized = mem_total - mem_avail
            data['MemUtilization'] = \
                f'{mem_utilized/gb:.1f} / {mem_total/gb:.0f} GB'
        except TypeError:
            data['MemUtilization'] = self.DATA_MISSING

        # GPU utilization + clocks
        try:
            gpu_stats = list(map(
                lambda x: f"#{x[0]}: {x[1]:.0f}% ({x[2] / 1000:.0f} MHz)",
                zip(count(), sync_status['gpuUtilization'],
                    sync_status['gpuCoreClocksKhz'])))

            if len(gpu_stats) > 0:
                data['GpuUtilization'] = '\n'.join(gpu_stats)
            else:
                data['GpuUtilization'] = self.DATA_MISSING
        except (KeyError, TypeError):
            data['GpuUtilization'] = self.DATA_MISSING

        # GPU temperature
        try:
            temps = [t if t != -2147483648 else self.DATA_MISSING
                     for t in sync_status['gpuTemperature']]

            if len(temps) > 0:
                data['GpuTemperature'] = '\n'.join(
                    map(lambda x: f"#{x[0]}: {x[1]}° C", zip(count(), temps)))
            else:
                data['GpuTemperature'] = self.DATA_MISSING
        except (KeyError, TypeError):
            data['GpuTemperature'] = self.DATA_MISSING

    def on_get_sync_status(self, device, message):
        '''
        Called when the listener has sent a message with the sync status
        '''
        try:
            if message['bAck'] is False:
                return
        except KeyError:
            LOGGER.error('Error parsing "get sync status" (missing "bAck")')
            return

        # Parse and update the model.
        deviceIdx, devicedata = self.devicedata_from_device(device)
        devicedata['time_last_update'] = time.time()
        devicedata['stale'] = False

        try:
            self.populate_sync_data(devicedata=devicedata, message=message)
        except (KeyError, ValueError):
            LOGGER.error(
                'Error parsing "get sync status" message and populating'
                'model data\n\n=== Traceback BEGIN ===\n'
                f'{traceback.format_exc()}=== Traceback END ===\n')
            return

        row = deviceIdx + 1
        self.dataChanged.emit(self.createIndex(row, 1),
                              self.createIndex(row, len(self.colnames)))

    def try_issue_console_exec(self, exec_str, executor=''):
        ''' Issues a console exec to the cluster '''
        devices = [devicedata['device']
                   for devicedata in self.devicedatas.values()]

        if len(devices):
            try:
                devices[0].console_exec_cluster(devices, exec_str, executor)
                return True
            except Exception as exc:
                LOGGER.warning(f'Could not issue console exec ({exc})',
                               exc_info=exc)

        return False

    # ~ QAbstractTableModel interface begin

    def rowCount(self, parent=QModelIndex()):
        return len(self.devicedatas)

    def columnCount(self, parent=QModelIndex()):
        return len(self.colnames)

    def headerData(self, section, orientation, role):
        if role == Qt.DisplayRole:
            if orientation == Qt.Horizontal:
                return self.colnames[section]
            else:
                return "{}".format(section)

        if role == Qt.ToolTipRole:
            return self.tooltips[section]

        return None

    def data(self, index, role=Qt.DisplayRole):
        column = index.column()
        row = index.row()

        # get column name
        colname = self.colnames[column]

        # grab (device_hash, device_data) from ordered dict
        _, devicedata = list(self.devicedatas.items())[row]
        data = devicedata['data']
        value = data[colname]

        if role == Qt.DisplayRole:
            return value

        elif role == Qt.BackgroundRole:
            return self.color_for_column(colname=colname, value=value,
                                         data=data)

        elif role == Qt.TextAlignmentRole:
            if colname in ('CpuUtilization', 'GpuUtilization'):
                return Qt.AlignLeft
            return Qt.AlignRight

        return None

    # ~ QAbstractTableModel interface end

    @QtCore.Slot()
    def on_refresh_mosaics_clicked(self):
        for devicedata in self.devicedatas.values():
            device = devicedata['device']
            device.refresh_mosaics()

    @QtCore.Slot()
    def on_fix_exe_flags_clicked(self):
        ''' Tries to force the correct UnrealEditor.exe flags '''
        for devicedata in self.devicedatas.values():
            device = devicedata['device']
            data = devicedata['data']

            good_string = 'DISABLEDXMAXIMIZEDWINDOWEDMODE'

            if good_string not in data['ExeFlags']:
                device.fix_exe_flags()

    @QtCore.Slot()
    def on_soft_kill_clicked(self):
        ''' Kills the cluster by sending a message to the primary. '''
        devices = [devicedata['device']
                   for devicedata in self.devicedatas.values()]

        if len(devices):
            try:
                devices[0].soft_kill_cluster(devices)
            except Exception:
                LOGGER.warning("Could not soft kill cluster")

    @QtCore.Slot()
    def on_minimize_windows_clicked(self):
        ''' Tries to minimize all windows in the nodes. '''
        for devicedata in self.devicedatas.values():
            device = devicedata['device']
            device.minimize_windows()
