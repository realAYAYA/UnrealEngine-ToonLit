AJA Python API
==============

This is a set of python modules for working with AJA Videos Systems, Inc. products.
For now it only supports the REST api for Ki-Pro family of products.

All files are Copyright (C) 2009,2010,2012 AJA Video Systems, Inc.


Requirements
------------

  * Ki-Pro family unity (Ki-Pro, Ki-Pro Mini, Ki-Pro Rack etc.) with firmware version 3.0 or later.
  * Python 2.x >= 2.6 (does not support Python 3.0)


Quickstart
------------

All the cool kids have a quickstart these days. Here's ours::

	$ python
	>>> from aja.embedded.kipro.rest import Client
	>>> client = Client('http:://Your-KiPro-URL-Or-IP')
	>>> client.getFirmwareVersion()
	'4.0.0.9'
	>>> client.record()
	>>> client.play()
	>>> client.getWriteableParameters()

More Information
----------------

The Python source is heavily documented. Use::

	$ python
	>>> from aja.embedded.kipro.rest import Client
	>>> help(Client)

	Help on class Client in module aja.embedded.rest.kipro:

	class Client
	|  This class understands the kipro REST api.
	|  Read the source for examples or for a quickstart:
	|  
	|  $ python
	|  >>> from aja.embedded.kipro.rest import Client
	|  >>> client = Client('http://10.3.95.12')
	|  >>> client.getFirmwareVersion()
	|  '4.0.0.9'
	|  >>> client.record()
	|  >>> client.play()
	|  >>> client.getWriteableParameters()
	|  
	|  Methods defined here:
	|  
	|  __init__(self, url, cacheRawParameters=True)
	|  
	|  asPython(self, response)
	|      Convert a Ki-Pro response into a python object 
	|      (usually an array of dictionaries depending ont he json).
	(etc.)...

The modules also have main and demo() methods so you can see a demo of what
they can do by running for example::

	$ python src/aja/embedded/rest/kipro.py -u http:://Your-KiPro-URL-Or-IP

