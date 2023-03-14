// Copyright Epic Games, Inc. All Rights Reserved.
import * as Sentry from '@sentry/node';
import * as crypto from 'crypto';
import * as fs from 'fs';
import { Arg, readProcessArgs } from '../common/args';
import { ContextualLogger } from '../common/logger';

const LdapAuth: any = require('ldapauth-fork');

export interface Credentials {
	user: string
	password: string
}

const TOKEN_VERSION = 0.6

// dev cookie key - should be overwritten by key from vault in production
const DEV_COOKIE_KEY = 'dev-cookie-key'

const ADMINS = ['<perforce user name>']

export interface AuthData {
	user: string,
	displayName: string,
	tags: Set<string> // tags to specify (e.g. fte, admin) or look up (e.g. fortnite [AD group mapping to set of perms]) access
}

export function getSentryUser(authData : AuthData) : Sentry.User {
	const user : Sentry.User = { username: authData.user }

	let i = 0
	// role information may be helpful for triage
	authData.tags.forEach((tag) => {
		user[`role${i}`] = tag
		i++
	})

	return user
}

const COMMAND_LINE_ARGS: {[param: string]: (Arg<any>)} = {
	vault: {
		match: /^-vault_path=(.+)$/,
		env: 'ROBO_VAULT_PATH',
		dflt: '/vault'
	}
}

const sessionStartupLogger = new ContextualLogger('Session Startup')
const maybeNullArgs = readProcessArgs(COMMAND_LINE_ARGS, sessionStartupLogger)
if (!maybeNullArgs) {
	process.exit(1)
}
const args = maybeNullArgs

export class Session {
	public static VAULT_PATH = args.vault

	public static onLoginAttempt: ((result: string) => void) | null = null

	static init(logger: ContextualLogger) {
		const ldapConfig = JSON.parse(fs.readFileSync('config/ldap.cfg.json', 'utf8'))

		Session.LDAP_CONFIG = ldapConfig['server-config']

		const botGroups = ldapConfig['bot-groups']
		if (botGroups) {
			for (const botGroup of botGroups) {
				Session.BOT_GROUPS.set(botGroup.group, botGroup.tags)
			}
		}

		try {
			const vaultString = fs.readFileSync(Session.VAULT_PATH + '/vault.json', 'ascii')
			const vault = JSON.parse(vaultString)
			Session.LDAP_CONFIG.adminPassword = vault['ldap-password']
			Session.COOKIE_KEY = vault['cookie-key']
			return
		}
		catch (err) {
			logger.warn(`Warning (ok in dev):  ${err.toString()}`)
			Session.COOKIE_KEY = DEV_COOKIE_KEY
		}

		logger.warn('No vault or no LDAP creds in vault (this is ok for testing)')
	}

	static login(creds: Credentials, logger: ContextualLogger) {
		return new Promise<string | null>((done, fail) => {
			// LdapAuth modifies the config, so copy to be clean (probably doesn't matter)
			logger.info(`checking LDAP groups for user "${creds.user}"`)

			const config: any = {}
			Object.assign(config, Session.LDAP_CONFIG)
			const auth = new LdapAuth(config)

			logger.info(`LDAP: auth helper created (${creds.user} log-in)`)

			const startTime = Date.now()
			auth.on('error', fail)
			auth.authenticate(creds.user, creds.password, (err: any | null, userData: any) => {
				const duration = Math.round((Date.now() - startTime) / 1000)
				logger.info(`LDAP for "${creds.user}" took ${duration}s`)

				if (err) {
					if (err.name === 'InvalidCredentialsError' ||
						(typeof(err) === 'string' && err.startsWith('no such user'))) {

						logger.info(`Log-in failed: ${err.name}, ${err} (${creds.user})`)
						if (Session.onLoginAttempt) {
							Session.onLoginAttempt('fail')
						}
						done(null)
					}
					else {
						// unknown error
						logger.printException(err, `LDAP error`)
						if (Session.onLoginAttempt) {
							Session.onLoginAttempt('error')
						}
						fail(err)
					}
					return
				}

				let tags = new Set<string>()

				if (userData._groups) {
					for (const groupInfo of userData._groups) {
						const groupNameMatch = groupInfo.dn.match(/CN=([^,]+),/)
						if (!groupNameMatch) {
							continue
						}
						const tagsForGroup = Session.BOT_GROUPS.get(groupNameMatch[1])
						if (tagsForGroup) {
							tags = new Set<string>([...tagsForGroup, ...tags])
						}
					}
				}

				// Add admin tags to users based on admins array
				if (ADMINS.indexOf(creds.user) > -1) {
					tags.add("admin")
				}

				const authData = {user: creds.user, displayName: userData.displayName || creds.user, tags}
				done(Session.authDataToToken(authData))

				if (Session.onLoginAttempt) {
					Session.onLoginAttempt('success')
				}
			})
		})
	}

	static tokenToAuthData(token: string): AuthData | null {
		const parts = token.replace(/-/g, '+').replace(/_/g, '/').split(':')
		if (parts.length !== 2) {
			return null
		}
		const dataBuf = Buffer.from(parts[0], 'base64')
		if (Session.macForTokenBuffer(dataBuf) != parts[1]) {
			return null
		}
		const obj = JSON.parse(dataBuf.toString('utf8'))
		if (obj.version !== TOKEN_VERSION || !obj.user || !obj.displayName || !Array.isArray(obj.tags)) {
			return null
		}
		obj.tags = new Set([...obj.tags])
		return obj as AuthData
	}

	private static macForTokenBuffer(buf: Buffer) {
		return crypto.createHmac('sha256', Session.COOKIE_KEY!).update(buf).digest('base64')
	}

	private static authDataToToken(authData: AuthData) {
		const authDataForToken: any = {}
		Object.assign(authDataForToken, authData)
		authDataForToken.version = TOKEN_VERSION
		authDataForToken.tags = [...authData.tags]
		const adBuf = Buffer.from(JSON.stringify(authDataForToken), 'utf8')
		const mac = Session.macForTokenBuffer(adBuf)
		return `${adBuf.toString('base64')}:${mac}`.replace(/\+/g, '-').replace(/\//g, '_')
	}

	private static LDAP_CONFIG: any | null = null
	private static COOKIE_KEY: string | null = null

	private static BOT_GROUPS = new Map<string, string[]>()

////////////////////////////
// ALL CODE BELOW IS TESTS

	private static _testSingleToken(testName: string, authData: AuthData, logger: ContextualLogger, other?: AuthData) {
		const token = Session.authDataToToken(authData)
		let tokenToDecode
		if (!other) {
			tokenToDecode = token
		}
		else {
			const otherToken = Session.authDataToToken(other)

			const otherTokenParts = otherToken.replace(/-/g, '+').replace(/_/g, '\\').split(':')
			const tokenParts = token.replace(/-/g, '+').replace(/_/g, '\\').split(':')

			// hack together fake token, to check it's rejected as necessary

			tokenToDecode = `${otherTokenParts[0]}:${tokenParts[1]}`
		}

		const decoded = Session.tokenToAuthData(tokenToDecode)

		// !decoded means mismatch
		if (other) {
			if (decoded) {
				throw new Error(`${testName}: Decoded but shouldn't have!`)
			}

			// ok - correctly rejected mismatch
			logger.info(`Passed '${testName}'!`)
			return
		}

		if (!decoded) {
			throw new Error(`${testName}: Failed to decode!`)
		}

		// should match now
		if (!decoded.user || !(decoded.tags instanceof Set)) {
			throw new Error(`${testName}: Invalid decoded data!`)
		}

		if (decoded.user.toLowerCase() !== authData.user.toLowerCase() ||
			![...decoded.tags].every(x => authData.tags.has(x))
		) {
			throw new Error(`${testName}: Mismatch!`)
		}

		logger.info(`Passed '${testName}'`)
	}

	static _testTokenSigning(logger: ContextualLogger) {
		const TEST_DATA: AuthData = {user: 'marlin.kingsly', displayName: '-', tags: new Set(['admin'])}
		
		Session._testSingleToken('match', TEST_DATA, logger)
		Session._testSingleToken('mismatched user', TEST_DATA, logger, {user: 'marlin.kngsly', displayName: '-', tags: new Set(['admin'])})
		Session._testSingleToken('mismatched tag', TEST_DATA, logger, {user: 'marlin.kingsly', displayName: '-', tags: new Set(['admn'])})
		// actually always going to encode user as lower case
		Session._testSingleToken('mismatched user case', TEST_DATA, logger, {user: 'Marlin.Kingsly', displayName: '-', tags: new Set(['admin'])})
		Session._testSingleToken('mismatched tag, case only', TEST_DATA, logger, {user: 'marlin.kingsly', displayName: '-', tags: new Set(['ADMIN'])})
		Session._testSingleToken('match, no tags', {user: 'marlin.kingsly', displayName: '-', tags: new Set([])}, logger)
		// (~~~~ forces a + in base 64 which gets replaced with a -). Quite a few of the MACs generated also test replacement
		Session._testSingleToken('match, with replaced character', {user: 'marlin.kingsly', displayName: '-', tags: new Set(['~~~~'])}, logger)
	}
}

Session.init(sessionStartupLogger)

if (process.argv.indexOf('__TEST__') !== -1) {
	Session._testTokenSigning(sessionStartupLogger.createChild('Testing'))
}
