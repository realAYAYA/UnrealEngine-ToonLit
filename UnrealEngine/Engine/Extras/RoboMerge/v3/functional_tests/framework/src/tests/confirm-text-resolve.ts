// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'

type PerClient = {
	client: P4Client
	textFilePath: string
}

export class ConfirmTextResolve extends FunctionalTest {


	private mainUser1: PerClient
	private developmentUser1: PerClient
	private mainUser2: PerClient

	private perClient(user: string, stream: string) {
		const client = this.p4Client(user, stream)
		return {
			client,
			textFilePath: Path.join(client.root, 'textfile.txt')
		}
	}

	constructor(p4: Perforce) {
		super(p4)

		this.mainUser1 = this.perClient('testuser1', 'Main')
		this.developmentUser1 = this.perClient('testuser1', 'Development')
		this.mainUser2 = this.perClient('testuser2', 'Main')
	}

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec('Main', 'mainline'))
		await this.p4.stream(this.streamSpec('Development', 'development', 'Main'))

		await this.mainUser1.client.create(P4Util.specForClient(this.mainUser1.client))

		// Add text file 'textfile.txt'
		await System.writeFile(this.mainUser1.textFilePath, 'Simple functional test text file') 
		await this.mainUser1.client.add('textfile.txt')
		await this.mainUser1.client.submit("Adding initial file 'textfile.txt'")

		// Populate Development stream
		await this.p4.populate(this.getStreamPath('Development'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Development (${this.getStreamPath('Development')})`)
	

		// Create Main workspace for testuser2 to create binary conflict
		await this.mainUser2.client.create(P4Util.specForClient(this.mainUser2.client))
		await this.mainUser2.client.sync()

		// Create Development workspace for testuser1
		await this.developmentUser1.client.create(P4Util.specForClient(this.developmentUser1.client))
		await this.developmentUser1.client.sync()
	}

	async run() {
		await this.developmentUser1.client.edit('textfile.txt')
		await System.appendToFile(this.developmentUser1.textFilePath, '\n\nAdding simple, mergable addition to text file')

		// This should merge automatically
		const command = `#robomerge ${this.fullBranchName('Main')}`
		await P4Util.submit(this.developmentUser1.client, 'Creating second revision in Development for text file for merging\n' + command)
	}

	getBranches() {
		return [{
			streamDepot: this.testName,
			name: this.testName + 'Main',
			streamName: 'Main',
			flowsTo: [this.testName + 'Development'],
			forceAll: false
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Development',
			streamName: 'Development',
			flowsTo: [this.testName + 'Main'],
		}]
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Main', 'textfile.txt', 2),
			this.checkDescriptionContainsEdit('Main', ['second revision'])
		])
	}
}
