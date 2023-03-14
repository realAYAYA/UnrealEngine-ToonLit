// Copyright Epic Games, Inc. All Rights Reserved.
import * as Path from 'path'
import { FunctionalTest, P4Client, P4Util } from '../framework'
import * as System from '../system'
import { Perforce } from '../test-perforce'

export class RejectDeleteResolveStomp extends FunctionalTest {

	private binaryFilePathMain: string[]

	private mainClient: P4Client[]
	private developmentClient: P4Client

	constructor(p4: Perforce) {
		super(p4)

		this.mainClient = [
			this.p4Client('testuser1', 'Main'),
			this.p4Client('testuser2', 'Main')
		]

		this.developmentClient = this.p4Client('testuser1', 'Development')

		this.binaryFilePathMain = this.mainClient.map(client => Path.join(client.root, 'fake.uasset'))
	}

	async setup() {
		// Set up depot

		await this.p4.depot('stream', this.depotSpec())

		// Setup streams
		await this.p4.stream(this.streamSpec('Main', 'mainline'))
		await this.p4.stream(this.streamSpec('Development', 'development', 'Main'))

		// Create workspaces
		await this.mainClient[0].create(P4Util.specForClient(this.mainClient[0]))
		await this.mainClient[1].create(P4Util.specForClient(this.mainClient[1]))
		await this.developmentClient.create(P4Util.specForClient(this.developmentClient))

		await System.writeFile(this.binaryFilePathMain[0], 'Simple functional test binary file') 
		await this.mainClient[0].add('fake.uasset', true)
		await this.mainClient[0].submit("Adding initial files 'fake.uasset'")

		// Populate Development stream
		await this.p4.populate(this.getStreamPath('Development'), `Initial branch of files from Main (${this.getStreamPath('Main')}) to Development (${this.getStreamPath('Development')})`)
	
		// Create second revision in main streqm to cause conflict
		await this.mainClient[1].sync()
		await this.mainClient[1].edit('fake.uasset')
 		await System.writeFile(this.binaryFilePathMain[1], 'Create second revision in Main for binary file')
		await this.mainClient[1].submit('Create second revision in Main for binary file')
	}

	async run() {

		// Create future binary file conflict
		await this.developmentClient.sync()
		await this.developmentClient.delete('fake.uasset')

		// This should result in a conflict on Main 'fake.uasset' so we can test stomp verification and execution!
		const command = `#robomerge ${this.testName}Main`
		await P4Util.submit(this.developmentClient, 'Deleting binary file in Development\n' + command)
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

	async verify() {
		const edgeState = await this.getEdgeState('Development', 'Main')
		const verifyResult = await this.verifyStompRequest('Development', 'Main', edgeState)

		if (!verifyResult.validRequest && verifyResult.branchOrDeleteResolveRequired && verifyResult.files[0].resolveResult.resolveType === 'delete') {
			this.info('Success! Stomp verify detected a delete resolve requirement after the automatic merge, which is correct for this functional test.')
		}
		else {
			throw new Error(`Stomp verify returned unexpected values. Erroring...\n${JSON.stringify(verifyResult)}`)
		}

		const stompResult = await this.performStompRequest('Development', 'Main', edgeState)

		if (stompResult.statusCode === 500) {
			this.info('Robomerge successfully rejected our stomp request')
			return
		}

		throw new Error(`FAILURE!!! Robomerge did not reject our stomp request, which should have failed due to delete resolve requirement. Response was: \n${JSON.stringify(stompResult)}`)
	}
}
