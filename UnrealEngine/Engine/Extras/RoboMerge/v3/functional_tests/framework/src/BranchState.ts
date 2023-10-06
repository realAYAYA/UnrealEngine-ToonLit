// Copyright Epic Games, Inc. All Rights Reserved.

export type Blockage = {
	owner: string
	change?: number
}

export class EdgeState {
	constructor(private rawEdgeData: any) {
		//console.log(JSON.stringify(rawEdgeData, null, '  '))
	}

	get displayName() {
		return this.rawEdgeData.display_name
	}

	isBlocked() {
		return !!this.rawEdgeData.blockage
	}

	get conflict() {
		return this.rawEdgeData.blockage ? {
			owner: this.rawEdgeData.blockage.owner || '<no owner>',
			change: this.rawEdgeData.blockage.change
		} : null
	}

	getLastCL() {
		return this.rawEdgeData.last_cl
	}

	getLastGoodCL() {
		return this.rawEdgeData.lastGoodCL
	}

	getGateClosedMessage() {
		return this.rawEdgeData.gateClosedMessage
	}

	dump() {
		console.log(this.rawEdgeData)
	}
}

export class BranchState {
	constructor(private rawBranchData: any) {
//		console.log(JSON.stringify(rawBranchData, null, '  '))
	}

	get name() {
		return this.rawBranchData.def.name as string
	}

	hasEdge(targetBranchName: string) {
		return !!this.rawBranchData.edges[targetBranchName.toUpperCase()]
	}

	getEdges(): EdgeState[] {
		return Object.entries(this.rawBranchData.edges).map(([_, edgeData]) => new EdgeState(edgeData))
	}

	getLastCL() {
		return this.rawBranchData.last_cl
	}

	getQueue() {
		return this.rawBranchData.queue || []
	}

	isBlocked() {
		return this.rawBranchData.is_blocked
	}

	isActive() {
		return this.rawBranchData.is_active
	}

	getStatusMessage() {
		return this.rawBranchData.status_msg
	}

	getTickCount() {
		return this.rawBranchData.tick_count
	}

	getEdgeState(targetBranchName: string): EdgeState {
		const edgeJson = this.rawBranchData.edges[targetBranchName.toUpperCase()]

		if (!edgeJson) {
			throw new Error(`Edge ${this.name} -> ${targetBranchName} not found`)
		}

		return new EdgeState(edgeJson)
	}
}
