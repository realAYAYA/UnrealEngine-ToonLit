import logging
import sys

FORMAT = '[%(asctime)s][%(levelname)8s] DatasmithExporter: %(message)s'
DATE_FORMAT = '%H:%M:%S'
logging.basicConfig(stream=sys.stdout, level=logging.INFO, format=FORMAT, datefmt=DATE_FORMAT)

#logging.DEBUG logging.INFO