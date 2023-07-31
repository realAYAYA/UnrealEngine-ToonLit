// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'


export class EdigrateMainRevToRelease extends FunctionalTest {

	private testFilePathMain: string

	private mainClient: P4Client
	private releaseClient: P4Client

	constructor(p4: Perforce) {
		super(p4)

		this.mainClient = this.p4Client('testuser1', 'Main')
		this.releaseClient = this.p4Client('testuser1', 'Release')

		this.testFilePathMain = Path.join(this.mainClient.root, 'a.txt')
	}

	async setup() {
		// Set up depot
		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec('Main', 'mainline'))
		await this.p4.stream(this.streamSpec('Release', 'release', 'Main'))
		
		// Create workspaces
		await this.mainClient.create(P4Util.specForClient(this.mainClient))
		await this.releaseClient.create(P4Util.specForClient(this.releaseClient))

		// Add file 'a.txt'
		await System.writeFile(this.testFilePathMain, 'Simple Unit Test File') 
		await this.mainClient.add('a.txt')
		await this.mainClient.submit("Adding Initial File 'a.txt'")
	
		// Populate Release stream
		await this.p4.populate(this.getStreamPath('Release'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Release (${this.getStreamPath('Release')})`)
	}

	async run() {
		await this.mainClient.edit('a.txt')
		await System.writeFile(this.testFilePathMain, 'Simple Unit Test File Rev. #2') 
		await this.mainClient.submit("Committing revision to 'a.txt'")
	}

	getBranches() {
		return [{
			streamDepot: this.testName,
			name: this.testName + 'Main',
			streamName: 'Main',
			flowsTo: [this.testName + 'Release'],
			forceAll: true,
			integrationMethod: 'convert-to-edit'
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Release',
			streamName: 'Release',
			flowsTo: [this.testName + 'Main']
		}]
	}

	verify() {
		return Promise.all([
			this.checkHeadRevision('Release', 'a.txt', 2),
			(async () => {
				this.info(`Ensuring //${this.testName}/Release/a.txt was edited, not integrated`)
				const changes = await this.releaseClient.changes(1)
				const files = await this.p4.describe(changes[0].change)
				if (files[0].action !== 'edit') {
					throw new Error(`Unexpected action for //${this.testName}/Release/a.txt. (Expected: edit, actual: ${files[0].action})`)
				}
			})()
		])
	}
}

