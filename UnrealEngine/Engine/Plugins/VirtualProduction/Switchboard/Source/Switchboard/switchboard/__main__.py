# Copyright Epic Games, Inc. All Rights Reserved.
from switchboard.config import SETTINGS, CONFIG
from switchboard.switchboard_scripting import ScriptManager
from .switchboard_dialog import SwitchboardDialog
from .switchboard_logging import LOGGER

import signal
import sys
import argparse

from PySide2 import QtCore, QtWidgets

# Build resources
# "C:\Program Files (x86)\Python37-32\Lib\site-packages\PySide2\pyside2-rcc" -o D:\Switchboard\switchboard\resources.py D:\Switchboard\switchboard\ui\resources.qrc

def parse_arguments():
    ''' Parses command line arguments and returns the populated namespace 
    '''
    parser = argparse.ArgumentParser()
    parser.add_argument('--script'    , default='', help='Path to script that contains a SwichboardScriptBase subclass')
    parser.add_argument('--scriptargs', default='', help='String to pass to SwichboardScriptBase subclass as arguments')
    args, unknown = parser.parse_known_args()
    return args

def launch():
    """
    Main for running standalone or in another application.
    """
    if sys.platform == 'win32':
        # works around some windows quirks so we can show the window icon
        import ctypes
        app_id = u'epicgames.virtualproduction.switchboard.0.1'
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID(app_id)

    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling)
    app = QtWidgets.QApplication(sys.argv)

    # Global variables can be inited after QT has been started (they may show dialogues if there are errors)
    SETTINGS.init()
    CONFIG.init(SETTINGS.CONFIG)
    
    # parse arguments
    args = parse_arguments()

    # script manager
    script_manager = ScriptManager()

    # add script passed from command line
    if args.script:
        try:
            script_manager.add_script_from_path(args.script, args.scriptargs)
        except Exception as e:
            LOGGER.warning(f"Could not initialize '{args.script}': {e}")

    # script pre-init
    script_manager.on_preinit()

    # create main window
    main_window = SwitchboardDialog(script_manager)

    if not main_window.window:
        return

    # closure so we can access main_window and app
    def sigint_handler(*args):
        LOGGER.info("Received SIGINT, exiting...")
        main_window.on_exit()
        app.quit()

    # install handler for SIGINT so it's possible to exit the app when pressing ctrl+c in the terminal.
    signal.signal(signal.SIGINT, sigint_handler)

    main_window.window.show()

    # Enable file logging.
    LOGGER.enable_file_logging()

    # Logging start.
    LOGGER.info('----==== Switchboard ====----')

    # this will pump the event loop every 200ms so we can react faster on a SIGINT.
    # otherwise it will take several seconds before sigint_handler is called.
    timer = QtCore.QTimer()
    timer.start(200)
    timer.timeout.connect(lambda: None)

    # execute the app
    appresult = app.exec_()

    # script exit
    script_manager.on_exit()

    sys.exit(appresult)

if __name__ == "__main__":
    launch()