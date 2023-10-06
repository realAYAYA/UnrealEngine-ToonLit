// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util, Stream } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'

const SOURCE_DEPOT = 'CDSI_SourceStream'
const TARGET_DEPOT = 'CDSI_TargetStream'

export class CrossDepotStreamIntegration extends FunctionalTest {


	private placeholderFilePathSource: string
	private placeholderFilePathTarget: string

	private sourceClient: P4Client
	private targetClient: P4Client

	private textFilePathSource: string

	private readonly sourceStream: Stream = {
		name: 'Main',
		streamType: "mainline",
		depotName: SOURCE_DEPOT
	}
	private readonly targetStream: Stream = {
		name: 'Main',
		streamType: "mainline",
		depotName: TARGET_DEPOT
	}

	constructor(p4: Perforce) {
		super(p4)

		this.sourceClient = this.p4Client('testuser1', 'Main', SOURCE_DEPOT)
		this.targetClient = this.p4Client('testuser1', 'Main', TARGET_DEPOT)

		this.placeholderFilePathSource = Path.join(this.sourceClient.root, 'placeholder.txt')
		this.placeholderFilePathTarget = Path.join(this.targetClient.root, 'placeholder.txt')

		this.textFilePathSource = Path.join(this.sourceClient.root, 'textfile.txt')
	}

	async setup() {
		// Set up depot
		await this.p4.depot('stream', this.depotSpec(SOURCE_DEPOT))
		await this.p4.depot('stream', this.depotSpec(TARGET_DEPOT))

		// Set up streams
		await this.p4.stream(this.streamSpec(this.sourceStream))
		await this.p4.stream(this.streamSpec(this.targetStream))

		// Set up workspaces
		await this.sourceClient.create(P4Util.specForClient(this.sourceClient))
		await this.targetClient.create(P4Util.specForClient(this.targetClient))

		// Add placeholder files so Robomerge can see the streams
		await System.writeFile(this.placeholderFilePathSource, 'Stream placeholder file')
		await this.sourceClient.add('placeholder.txt')
		await this.sourceClient.submit("Adding initial placeholder file 'placeholder.txt'")

		await System.writeFile(this.placeholderFilePathTarget, 'Stream placeholder file')
		await this.targetClient.add('placeholder.txt')
		await this.targetClient.submit("Adding initial placeholder file 'placeholder.txt'")
	}

	async run() {
		await System.writeFile(this.textFilePathSource, 'Simple functional test file') 
		await this.sourceClient.add('textfile.txt')
		await this.sourceClient.submit("Adding initial file 'textfile.txt'")
	}

	getBranches() {
		return [{
			streamDepot: SOURCE_DEPOT,
			name: SOURCE_DEPOT + 'Main',
			streamName: 'Main',
			flowsTo: [TARGET_DEPOT + 'Main'],
			forceAll: true
		}, {
			streamDepot: TARGET_DEPOT,
			name: TARGET_DEPOT + 'Main',
			streamName: 'Main',
			flowsTo: [SOURCE_DEPOT + 'Main'],
		}]
	}

	async verify() {
		await this.checkHeadRevision('Main', 'textfile.txt', 1, TARGET_DEPOT)
	}
}

