// Copyright Epic Games, Inc. All Rights Reserved.

import { BranchDefForStatus, BranchStatus } from "../../src/robo/status-types"

class Info {
	graphNodeColor?: string

	forcedEdges = new Set<string>()
	defaultDests = new Set<string>()
	blockAssetDests = new Set<string>()

	importance = 0


	constructor(public id: string, public tooltip: string, public name: string, public sources: BranchStatus[]) {
	}

	update(branch: BranchStatus, getId: (alias: string) => string) {

		const edgeId = (alias: string) => branch.def.name + '->' + getId(alias)
		this.forcedEdges = new Set([...this.forcedEdges, ...branch.def.forceFlowTo.map(edgeId)])
		this.defaultDests = new Set([...this.defaultDests, ...branch.def.defaultFlow.map(edgeId)])
		this.blockAssetDests = new Set([...this.blockAssetDests, ...branch.def.blockAssetTargets.map(edgeId)])
	}
}

type NodeInfo = {
	importance: number
	marginX: string; marginY: string; fontSize: string
	info: Info
}

type Link = {
	src: string
	dst: string
	styles: [string, string][]
}

const EDGE_STYLES: {[key: string]: [string, string][]} = {
	roboshelf: [['color', 'purple'], ['arrowhead', 'diamond']],
	forced: [],
	defaultFlow: [['color', 'blue']],
	onRequest: [['color', 'darkgray'], ['style', 'dashed']],
	blockAssets: [['color', 'darkgray'], ['style', 'dashed'], ['arrowhead', 'odiamond']]
}

function setDefault<K, V>(map: Map<K, V>, key: K, def: V): V {
	const val = map.get(key)
	if (val) {
		return val
	}
	map.set(key, def)
	return def
}

function renderGraph(src: string): Promise<any> {
	const hpccWasm = (window as any)["@hpcc-js/wasm"];
	return hpccWasm.graphviz.layout(src, "svg", "dot");

}

function decoratedAlias(bot: string, alias: string) {
	return bot + ':' + alias
}

function aliasForBranch(branch: BranchStatus) {
	return decoratedAlias(branch.bot, branch.def.upperName)
}


type Options = {
	hideDisconnected?: boolean
	noGroups?: boolean
	showOnlyForced?: boolean
	botsToShow?: string[]
}

function parseOptions(search: string): Options {
	return {
		showOnlyForced: !!search.match(/showOnlyForced/i),
		noGroups: !!search.match(/noGroups/i),
		hideDisconnected: !!search.match(/hideDisconnected/i)
	}
}

class Graph {
	readonly nodeLabels = new Map<string, Info[]>()
	readonly links: Link[] = []

	private readonly aliases = new Map<string, Info>()
	private readonly allInfos: Info[] = []

	private readonly linkOutward = new Set<string>()
	private readonly linkInward = new Set<string>()

	private readonly connectedNodes = new Set<string>()

	private branchList: BranchStatus[] = []

	constructor(
		private allBranches: BranchStatus[],
		private options: Options
	) {

	}

	singleBot(botName: string) {
		this.branchList = this.allBranches.filter(b => b.bot === botName)
		this.addNodesAndEdges(false)
		return this.makeGraph(botName, botName + 'integration paths')
	}

	allBots() {
		this.branchList = this.options.botsToShow
			? this.allBranches.filter(b => this.options.botsToShow!.indexOf(b.bot) >= 0)
			: this.allBranches

		this.findSharedNodes()
		this.addNodesAndEdges(!this.options.noGroups)

		return this.makeGraph('All bots', 'Flow including shared bots')
	}

	private addNodesAndEdges(grouped: boolean) {
		for (const branchStatus of this.branchList) {
			this.addBranch(branchStatus, grouped ? branchStatus.bot : null)
		}

		for (const info of this.allInfos) {
			for (const sourceBranch of info.sources) {
				info.update(sourceBranch, this.getIdForBot(sourceBranch.bot))
			}
		} 

		for (const branchStatus of this.branchList) {
			for (const flow of branchStatus.def.flowsTo) {
				this.addEdge(branchStatus, flow)
			}
		}
	}

	private makeGraph(title: string, tooltip: string) {
		const lines: string[] = [
			'digraph robomerge {',
			'fontname="sans-serif"; labelloc=top; fontsize=16;',
			'edge [penwidth=2]; nodesep=.7; ranksep=1.2;',
			`label = "${title}";`,
			`tooltip="${tooltip}";`,
			`node [shape=box, style=filled, fontname="sans-serif", fillcolor=moccasin];`,
		]

		const nodeGroups: [string, NodeInfo[]][] = []
		for (const [groupName, v] of this.nodeLabels) {
			const nodeInfo: NodeInfo[] = []
			nodeGroups.push([groupName, nodeInfo])

			for (const info of v) {
				const {id, tooltip, name, importance} = info
				let factor = (Math.min(importance, 10) - 1) / 9
				nodeInfo.push({
					importance: factor,
					marginX: (.2 * (1 - factor) + .4 * factor).toPrecision(1),
					marginY: (.1 * (1 - factor) + .25 * factor).toPrecision(1),
					fontSize: (14 * (1 - factor) + 20 * factor).toPrecision(2),
					info: info
				})
			}
		}

		for (const [groupName, nodeInfo] of nodeGroups) {
			if (groupName !== 'nogroup') {
				lines.push(`subgraph cluster_${groupName} {
	label="${groupName}";
`)
			}

			for (const info of nodeInfo) {
				if (this.options.hideDisconnected && !this.connectedNodes.has(info.info.id)) {
					continue
				}
				const attrs: [string, string][] = [
					['label', `"${info.info.name}"`],
					['tooltip', `"${info.info.tooltip}"`],
					['margin', `"${info.marginX},${info.marginY}"`],
					['fontsize', info.fontSize],
				]

				if (info.importance > .5) {
					attrs.push(['style', '"filled,bold"'])
				}

				if (info.info.graphNodeColor) {
					attrs.push(['fillcolor', `"${info.info.graphNodeColor}"`])
				}

				const attrStrs = attrs.map(([key, value]) => `${key}=${value}`)
				lines.push(`${info.info.id} [${attrStrs.join(', ')}];`)
			}

			if (groupName !== 'nogroup') {
				lines.push('}')
			}
		}

		for (const link of this.links) {
			// when there's forced/unforced pair, only the former is a constrait
			// (this makes flow )
			const combo = link.dst + '->' + link.src

			if (combo && this.linkOutward.has(combo) && this.linkInward.has(combo)) {
				link.styles.push(['constraint', 'false'])
			}

			const styleStrs = link.styles.map(([key, value]) => `${key}=${value}`)
			const suffix = styleStrs.length === 0 ? '' : ` [${styleStrs.join(', ')}]`
			lines.push(`${link.src} -> ${link.dst}${suffix};`)
		}
		lines.push('}')
		return lines
	}

	private addBranch(branchStatus: BranchStatus, group: string | null = null) {
		const branch = branchStatus.def
		if (this.aliases.has(decoratedAlias(branchStatus.bot, branch.upperName)))
			return

		let tooltip = branch.rootPath
		if (branch.aliases && branch.aliases.length !== 0) {
			tooltip += ` (${branch.aliases.join(', ')})`
		}
		const info = new Info(
			`_${branchStatus.bot}_${branch.upperName.replace(/[^\w]+/g, '_')}`,
			tooltip, branch.name,
			[branchStatus]
		)
		this.allInfos.push(info)
		if (branch.config.graphNodeColor) {
			info.graphNodeColor = branch.config.graphNodeColor
		}

		if (branch.upperName === 'MAIN') {
			// special case branches named Main
			info.importance = 10
		}

		// note: branch.upperName is always in branch.aliases
		for (const alias of branch.aliases) {
			this.aliases.set(decoratedAlias(branchStatus.bot, alias), info)
		}
		setDefault(this.nodeLabels, group || 'nogroup', []).push(info)
	}

	// dstName 
	private addEdge(srcBranchStatus: BranchStatus, dst: string) {
		const src = aliasForBranch(srcBranchStatus)
		const srcInfo = this.aliases.get(src)!

		const dstInfo = this.aliases.get(decoratedAlias(srcBranchStatus.bot, dst))!

		++srcInfo.importance
		++dstInfo.importance

		const hasEdge = (edges: Set<string>, target: Info) => 
			edges.has(srcBranchStatus.def.name + '->' + target.id)

		const isForced = hasEdge(srcInfo.forcedEdges, dstInfo)
		if (!isForced && this.options.showOnlyForced) {
			return
		}

		this.connectedNodes.add(srcInfo.id)
		this.connectedNodes.add(dstInfo.id)

		const edgeStyle =
			srcBranchStatus.def.convertIntegratesToEdits ?	'roboshelf' :
			isForced ?										'forced' :
			hasEdge(srcInfo.defaultDests, dstInfo) ?		'defaultFlow' :
			hasEdge(srcInfo.blockAssetDests, dstInfo) ?		'blockAssets' : 'onRequest'

		const styles = [...EDGE_STYLES[edgeStyle]]

		const link: Link = {src: srcInfo.id, dst: dstInfo.id, styles}
		this.links.push(link)
		if (isForced) {
			this.linkOutward.add(link.src + '->' + link.dst)
		}
		else if (edgeStyle === 'blockAssets' || edgeStyle === 'onRequest') {
			this.linkInward.add(link.dst + '->' + link.src)
		}
	}

	private findSharedNodes() {
		const streamMap = new Map<string, [string | null, BranchStatus[]]>()
		for (const branchStatus of this.branchList) {
			const [color, streams] = setDefault(streamMap, branchStatus.def.rootPath, [null, []])
			streams.push(branchStatus)
			const nodeColor = branchStatus.def.config.graphNodeColor
			if (!color && nodeColor) {
				streamMap.set(branchStatus.def.rootPath, [nodeColor, streams])
			}
		}

		// create a shared info for each set of nodes monitoring the same stream
		for (const [k, v] of streamMap) {
			const [color, streams] = v
			if (streams.length > 1) {
				const info = new Info(k.replace(/[^\w]+/g, '_'), streams.map(s => s.bot).join(', '), k, streams)
				this.allInfos.push(info)
				if (color) {
					info.graphNodeColor = color
				}

				for (const branchStatus of streams) {
					for (const alias of branchStatus.def.aliases) {
						this.aliases.set(decoratedAlias(branchStatus.bot, alias), info)
					}
				}

				setDefault(this.nodeLabels, 'nogroup', []).push(info)
			}
		}
	}

	private getIdForBot(bot: string) {
		return (alias: string) => {
			const info = this.aliases.get(decoratedAlias(bot, alias))
			return info ? info.id : alias
		}
	}
}

type Args = ({ botNames: string[] } | { singleBotName: string }) & Options

function makeGraph(data: BranchStatus[], args: Args) {

	let botsToShow: string[] | null = null

	const options: Options = args

	const singleBotName = (args as { singleBotName: string }).singleBotName
	if (singleBotName) {
		return (new Graph(data, options)).singleBot(singleBotName)
	}

	const botNames = (args as { botNames: string[] }).botNames
	if (botNames.length > 0) {
		options.botsToShow = botNames.map(s => s.toUpperCase())
	}
	return (new Graph(data, options)).allBots()
}

function showFlowGraph(data: BranchStatus[], args: Args) {
	const lines = makeGraph(data, args)

	const graphContainer = $('<div class="clearfix">')
	const flowGraph = $('<div class="flow-graph" style="display:inline-block;">').appendTo(graphContainer)
	flowGraph.append($('<div>').css('text-align', 'center').text("Building graph..."))

	renderGraph(lines.join('\n'))
	.then(svg => {
		$('#graph-key-template')
			.clone()
			.removeAttr('id')
			.css('display', 'inline-block')
			.appendTo(graphContainer)

		$('#graph-loading-text').hide()
		const span = $('<span>').css('margin','auto').html(svg)
		const svgEl = $('svg', span).addClass('branch-graph').removeAttr('width')
		// scale graph to 70% of default size
		const height = Math.round(parseInt(svgEl.attr('height')!) * .7)
		svgEl.attr('height', height + 'px').css('vertical-align', 'top')

		flowGraph.empty()
		flowGraph.append(span)
	})

	return graphContainer
}
