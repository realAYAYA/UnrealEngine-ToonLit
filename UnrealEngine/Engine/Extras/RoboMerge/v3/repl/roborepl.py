import code

import json, requests, sys

if sys.platform == 'darwin':
	import gnureadline

USE_COLORS = '-nocolors' not in sys.argv

# colours
if USE_COLORS:
	ENDC = '\033[37m'
	color = lambda n, bright=False:	lambda s: f'\033[{3 + 6*bright}{n}m{s}{ENDC}'
else:
	ENDC = ''
	color = lambda n, bright=False:	lambda s: s

warn = color(3)
error = color(1)
name_color = color(2)

#########



ROBOMERGE_URL = 'https://robomerge.epicgames.net'



context = dict(
	_bots = {},
	ops = {}
)

def get_bot(name):
	try:
		return context['_bots'][bot.lower()]
	except KeyError:
		raise f"Unknown bot '{name}'"

def request(url, method, data=None):

	auth_token = context['_auth_token']
	if method == 'GET':
		return requests.get(url, cookies = dict(auth=auth_token))

	return requests.post(url, cookies = dict(auth=auth_token), data=data)

def get(url): return request(url, 'GET')
def post(url, data=None): return request(url, 'POST', data)

def export(category):
	def impl(f):
		context[f.__name__] = f
		ops = context['ops'].setdefault(category, [])
		ops.append(f.__name__)
		ops.sort()
		return f
	return impl

class Bot(object):
	def __init__(self, name):
		self.name = name

		self.nodes = {}
		self.edges = {}

class Branch(object):
	def __init__(self, bot, json_data):
		self.bot = bot

		def_obj = json_data['def']
		self.name = def_obj['name']
		self.flows_to = def_obj.get('flowsTo', [])
		self.force_flow = def_obj.get('forceFlowTo', [])

		self.aliases = def_obj.get('aliases', [])

class Blockage(object):
	def __init__(self, blockage_json):
		self.cl = blockage_json.get('change')
		self.owner = blockage_json.get('owner')

class Node(object):
	def __init__(self, bot, name):
		self.bot = bot
		self.name = name

	def __repr__(self):
		return f"Node({name_color(self.name)})"

class Edge(object):
	def __init__(self, bot, source, target, merge_mode):
		self.bot = bot
		self.source = source
		self.target = target
		self.merge_mode = merge_mode

		self.is_paused, self.blockage = False, None

	def __repr__(self):
		return f'Edge({self.source}, {self.target}, {self.merge_mode})'

	def update_status(self, status_json):
		self.is_paused = bool(status_json.get('is_paused'))
		self.blockage = bool(status_json.get('is_blocked')) and Blockage(status_json.get('blockage'))

		if self.is_paused:
			manual_pause_json = status_json.get('manual_pause')
			self.paused_since = manual_pause_json and manual_pause_json.get('startedAt')

def fetch_branch_data():

	response = get(f'{ROBOMERGE_URL}/api/branches')

	bots = context['_bots']
	bots.clear()

	branches = []
	edge_statuses = []
	for branch_json in json.loads(response.text)['branches']:
		bot_name = branch_json['bot']
		bot = bots.setdefault(bot_name.lower(), Bot(bot_name))

		branch = Branch(bot, branch_json)
		branches.append(branch)

		edges = branch_json.get('edges')
		if edges:
			edge_statuses += [(bot, branch.name, target, status) for target, status in edges.items()]

		node = Node(bot, branch.name)

		bot.nodes[branch.name.lower()] = node
		for alias in branch.aliases:
			bot.nodes[alias.lower()] = node

	for branch in branches:
		for target in branch.flows_to:
			source_node = branch.bot.nodes[branch.name.lower()]
			target_node = branch.bot.nodes[target.lower()]
			branch.bot.edges[(source_node, target_node)] \
				= Edge(branch.bot, source_node, target_node,
						'auto' if target in branch.force_flow else 'normal'
								# not quite right if alias used for force list
				)

	for bot, branch_name, target, status_json in edge_statuses:
		find_edge(bot.name, branch_name, target).update_status(status_json)


@export('login')
def login(user, password):

	r = requests.post(f'{ROBOMERGE_URL}/dologin', data = dict(
		user = user,
		password = password
	))

	if r.status_code == 200:
		print(user + ' logged in')
		context['_auth_token'] = r.text
		fetch_branch_data()
	else:
		print(r.text)

# discoverability

# query branches once for now - maybe provide way to refresh status



	# print([0]['def']['upperName'])
@export('discovery')
def list_bots():
	return list(context['_bots'].keys())

@export('discovery')
def find_edge(bot, source, target):
	'''Find an edge in a specific bot
	:param str bot: Name of bot to look in (not case-sensitive)
	:param str source: Name or part of name of source node (not case-sensitive)
	:param str source: Name or part of name of target node (not case-sensitive)
	'''
	source_node = find_node(bot, source)
	target_node = find_node(bot, target)

	bot = context['_bots'][bot.lower()]
	return source_node and target_node and bot.edges.get((source_node, target_node))

@export('discovery')
def find_node(bot, name):
	'''Find an edge in a specific bot
	:param str bot: Name of bot to look in (not case-sensitive)
	:param str name: Name or part of name of node to find (not case-sensitive)
	'''
	name = name.lower()
	bot = context['_bots'][bot.lower()]
	from_alias = bot.nodes.get(name)
	if from_alias:
		return from_alias

	# fuzzier match
	for alias, node in bot.nodes.items():
		if name in alias:
			return node

@export('discovery')
def list_ops():
	return context['_ops']


# status
@export('status')
def list_blockages(bot):
	bot = context['_bots'][bot.lower()]
	for edge in bot.edges.values():
		# next, store CL and owner
		if edge.is_paused:
			print(f"{edge} {warn('paused')} since {edge.paused_since}")

		if edge.blockage:
			print(f"{edge} {error('blocked')} at CL {edge.blockage.cl} (owner {edge.blockage.owner})")

# operations

def op_url(node_or_edge, op):
	prefix = f'{ROBOMERGE_URL}/api/op/bot/{node_or_edge.bot.name}/'
	suffix = '/op/' + op

	if isinstance(node_or_edge, Node):
		return prefix + f'node/{node_or_edge.name()}' + suffix

	edge = node_or_edge
	return prefix + f'node/{edge.source.name.upper()}/edge/{edge.target.name.upper()}' + suffix

@export('ops')
def retry(edge):
	'''Send a retry request to RoboMerge

	:param Edge edge: Blocked edge that should retry integration
	'''
	result = post(op_url(edge, 'retry'))
	if result.status_code != 200:
		raise Exception(result.text)

@export('ops')
def reconsider(node_or_edge, cl):
	'''Send a reconsider request to RoboMerge

	:param Node|Edge target: Edge to integrate along, or source node to integrate to all outgoing edges
	:param int cl: Changelist to integrate
	'''
	result = post(op_url(node_or_edge, 'reconsider') + f'?cl={cl}')
	if result.status_code != 200:
		raise Exception(result.text)

if __name__ == '__main__':
	code.interact('RoboMerge REPL v0.1' + ENDC, None, context)
