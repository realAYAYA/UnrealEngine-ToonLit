// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'


export class MergeMainRevToMultipleRelease extends FunctionalTest {
	private testFilePathMain: string

	private mainClient: P4Client
	private releaseClient: P4Client[]

	constructor(p4: Perforce) {
		super(p4)

		this.mainClient = this.p4Client('testuser1', 'Main')
		this.releaseClient = [
			this.p4Client('testuser1', 'Release0'),
			this.p4Client('testuser1', 'Release1')
		]

		this.testFilePathMain = Path.join(this.mainClient.root, 'a.txt')
	}

	async setup() {
		// Set up depot
		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec('Main', 'mainline'))
		await this.p4.stream(this.streamSpec('Release0', 'release', 'Main'))
		await this.p4.stream(this.streamSpec('Release1', 'release', 'Main'))
		
		// Setup mainline workspace 'testuser1_MergeMainRevToRelease_Main' with test data
		// Create workspace
		await this.mainClient.create(P4Util.specForClient(this.mainClient))
		await this.releaseClient[0].create(P4Util.specForClient(this.releaseClient[0]))
		await this.releaseClient[1].create(P4Util.specForClient(this.releaseClient[1]))

		// Add file 'a.txt'
		await System.writeFile(this.testFilePathMain, 'Simple Unit Test File') 
		await this.mainClient.add('a.txt')
		await this.mainClient.submit("Adding initial file 'a.txt'")

		// Populate Release stream
		await this.p4.populate(this.getStreamPath('Release0'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Release0 (${this.getStreamPath('Release0')})`)
		await this.p4.populate(this.getStreamPath('Release1'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Release1 (${this.getStreamPath('Release1')})`)
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
			flowsTo: [this.testName + 'Release0', this.testName + 'Release1'],
			forceAll: true
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Release0',
			streamName: 'Release0',
			flowsTo: [this.testName + 'Main'],
		}, {
			streamDepot: this.testName,
			name: this.testName + 'Release1',
			streamName: 'Release1',
			flowsTo: [this.testName + 'Main'],
		}]
	}

	async verify() {
		await Promise.all([
			this.checkHeadRevision('Release0', 'a.txt', 2),
			this.checkHeadRevision('Release1', 'a.txt', 2)
		])
	}
}

