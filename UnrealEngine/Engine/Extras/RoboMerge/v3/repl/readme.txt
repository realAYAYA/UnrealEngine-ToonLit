## setup

Requires Python 3.

```
pip install requests
```

## run

```
python roborepl.py
```

## example

```
RoboMerge REPL v0.1
>>> login('john.wick', <Epic password>)
>>> edge = find_edge('fortnite', 'main', '16.00')
>>> reconsider(edge, 123456) # change list in Main
```

## notes

Strings are not case senstive.

## functions

### login

`login`

### discovery

`find_edge`, `find_node`, `list_bots`, `list_ops`

### status

`list_blockages`

### ops

`reconsider`, `retry`


`ops` contains categorized lists of available functions:

```
>>> ops
{'login': ['login'], 'discovery': ['find_edge', 'find_node', 'list_bots', 'list_ops'], 'status': ['list_blockages'], 'ops': ['reconsider', 'retry']}
```

Use `help` for documentation

```
>>> help(retry)
retry(edge)
    Send a retry request to RoboMerge
    
    :param Edge edge: Blocked edge that should retry integration
 ```
 
### options

 `-nocolors`
	 : disable colored output
