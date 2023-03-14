# Copyright Epic Games, Inc. All Rights Reserved.
import os
import sys
import time
import logging
import tempfile
import calendar
import datetime

from PySide2 import QtCore

logging.MESSAGE_LEVEL_NUM = logging.DEBUG - 2
logging.OSC_LEVEL_NUM = logging.DEBUG - 1
logging.SUCCESS_LEVEL_NUM = logging.INFO + 2

class QtHandler(logging.Handler):
    def __init__(self):
        logging.Handler.__init__(self)
        self.record = None

        html = """<span style="margin: 0px; display: block"><font color="grey">[{}][{}]:</font> 
        <font color="{}">{}</font></span>"""
        self.html_format = html

    def emit(self, record):
        self.record = self.format(record)

        if record:
            ConsoleStream.stdout().write('{}'.format(self.record))
            ConsoleStream.stderr().write('{}'.format(self.record))

    def format(self, record):
        levelName = record.levelname

        if levelName == 'DEBUG':
            initial = 'D'
            color = '#66D9EF'
        elif levelName == 'INFO':
            initial = 'I'
            color = 'white'
        elif levelName == 'WARNING':
            initial = 'W'
            color = 'yellow' #E6DB74
        elif levelName == 'CRITICAL':
            initial = 'C'
            color = '#FD971F'
        elif levelName == 'ERROR':
            initial = 'D'
            color = '#F92672'
        elif levelName == 'OSC':
            initial = 'O'
            color = '#4F86C6'
        elif levelName == 'MESSAGE':
            initial = 'M'
            color = '#7b92ad'
        elif levelName == 'SUCCESS':
            initial = 'S'
            color = '#A6E22E'

        return self.html_format.format(datetime.datetime.now().strftime("%H:%M:%S"), initial, color, record.msg)

    def get_record(self):
        return self.record


class ConsoleStream(QtCore.QObject):
    _stdout = None
    _stderr = None
    message_written = QtCore.Signal(str)

    def write(self, message):
        if not self.signalsBlocked():
            self.message_written.emit(message)

    @staticmethod
    def stdout():
        if not ConsoleStream._stdout:
            ConsoleStream._stdout = ConsoleStream()
            sys.stdout = ConsoleStream._stdout
        return ConsoleStream._stdout

    @staticmethod
    def stderr():
        if not ConsoleStream._stderr:
            ConsoleStream._stderr = ConsoleStream()
            sys.stdout = ConsoleStream._stderr
        return ConsoleStream._stderr

    def flush(self):
        pass


class HTMLogger(object):
    """This overrides the logging verbosity levels with an HTML layer for console coloring."""

    def __init__(self, logger):
        super(HTMLogger, self).__init__()
        self.logger = logger
        self.file_handler = None
        self.log_path = None

        self.add_custom_levels()

    def add_custom_levels(self):
        logging.addLevelName(logging.OSC_LEVEL_NUM, "OSC")

        def __log_osc(s, message, *args, **kwargs):
            if s.isEnabledFor(logging.OSC_LEVEL_NUM):
                s._log(logging.OSC_LEVEL_NUM, message, args, **kwargs)
        logging.Logger.osc = __log_osc


        logging.addLevelName(logging.SUCCESS_LEVEL_NUM, "SUCCESS")

        def __log_success(s, message, *args, **kwargs):
            if s.isEnabledFor(logging.SUCCESS_LEVEL_NUM):
                s._log(logging.SUCCESS_LEVEL_NUM, message, args, **kwargs)
        logging.Logger.success = __log_success


        logging.addLevelName(logging.MESSAGE_LEVEL_NUM, "MESSAGE")

        def __log_message(s, message, *args, **kwargs):
            if s.isEnabledFor(logging.MESSAGE_LEVEL_NUM):
                s._log(logging.MESSAGE_LEVEL_NUM, message, args, **kwargs)
        logging.Logger.message = __log_message

    def debug(self, message, exc_info=False, current_time_only=True):
        self.logger.debug(message, exc_info=exc_info)

    def success(self, message, exc_info=False, current_time_only=True):
        self.logger.success(message, exc_info=exc_info)

    def info(self, message, exc_info=False, current_time_only=True):
        self.logger.info(message, exc_info=exc_info)

    def warning(self, message, exc_info=False, current_time_only=True):
        self.logger.warning(message, exc_info=exc_info)

    def critical(self, message, exc_info=False, current_time_only=True):
        self.logger.critical(message, exc_info=exc_info)

    def error(self, message, exc_info=False, current_time_only=True):
        self.logger.error(message, exc_info=exc_info)

    def osc(self, message, exc_info=False, current_time_only=True):
        self.logger.osc(message, exc_info=exc_info)

    def message(self, message, exc_info=False, current_time_only=True):
        self.logger.message(message, exc_info=exc_info)

    def get_timestamp(self, current_time_only=True):
        current_time = self.get_current_time()
        if current_time_only:
            return current_time

        weekday = list(calendar.day_abbr)[int(datetime.date.today().weekday())]
        day = str(datetime.date.today()).replace("-", "/")
        return " ".join([weekday, day, current_time])

    @staticmethod
    def get_current_time():
        return datetime.datetime.now().strftime("%H:%M:%S")

    def enable_file_logging(self, log_path=None):
        if not log_path:
            if not os.path.isdir(DEFAULT_LOG_PATH):
                os.makedirs(DEFAULT_LOG_PATH)
            self.log_path = os.path.join(DEFAULT_LOG_PATH, "switchboard_{}.html".format(str(time.time())))
        else:
            self.log_path = log_path
        self.file_handler = logging.FileHandler(self.log_path)
        self.logger.addHandler(self.file_handler)
        self.logger.setLevel(logging.DEBUG)  # Log everything.

    def setLevel(self, value):
        self.logger.setLevel(value)

    def disable_file_logging(self):
        self.log_path = None
        if not self.file_handler:
            return self.logger.warning("No file handler found!")
        self.logger.removeHandler(self.file_handler)
        self.file_handler.close()
        
    def save_log_file(self) -> str:
        if self.log_path is not None:
            replaced_path_name = self.log_path.replace(".html", ".log")
            with open(replaced_path_name, "w") as file:
                file.write(_LOG_STREAM.getvalue())
            return replaced_path_name

from io import StringIO
_LOG_STREAM = StringIO()
logging.basicConfig(stream=_LOG_STREAM)
_LOGGER = logging.getLogger("switchboard")
DEFAULT_LOG_PATH = os.path.join(tempfile.gettempdir(), "switchboard")

QT_HANDLER = QtHandler()
PYTHON_HANDLER = logging.StreamHandler()

formatter = logging.Formatter('[%(levelname)s]: %(message)s')
PYTHON_HANDLER.setFormatter(formatter)

_LOGGER.addHandler(QT_HANDLER)
_LOGGER.addHandler(PYTHON_HANDLER)
_LOGGER.setLevel(logging.DEBUG)
PYTHON_HANDLER.setLevel(logging.DEBUG)

LOGGER = HTMLogger(_LOGGER)
