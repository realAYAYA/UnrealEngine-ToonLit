// Copyright Epic Games, Inc. All Rights Reserved.

import * as Sentry from '@sentry/node';
import { createTransport } from 'nodemailer';
import * as Mail from 'nodemailer/lib/mailer';
import { Analytics } from './analytics';
import { ContextualLogger } from './logger';

import fs = require('fs')
import path = require('path')

const FROM = "RoboMerge <robomerge@companyname.com>";
const SMTP_SERVER = 'smtp.companyname.net';

const TEMPLATE_PATH : string = path.normalize(`${__dirname}/../../email_templates`)

/**
 * Utility class: build up a list of to, cc, bcc, each recipient on at most one
 * list (earlier more public lists take priority)
 */

interface RecInfo {
	kind: string;
	email?: string;
}

export class Recipients {
	allRecipients = new Map<string, RecInfo>();
	private static readonly recipientsLogger = new ContextualLogger('Recipients')

	constructor(...to: string[]) {
		this.addTo(...to);
	}

	addTo(...recipients: string[]) {
		for (const rec of recipients) {
			this.allRecipients.set(rec, {kind:'to'});
		}
	}

	addCc(...recipients: string[]) {
		for (const rec of recipients) {
			const existing = this.allRecipients.get(rec);
			if (!existing || existing.kind !== 'to') {
				this.allRecipients.set(rec, {kind:'cc'});
			}
		}
	}

	addBcc(...recipients: string[]) {
		for (const rec of recipients) {
			if (!this.allRecipients.has(rec)) {
				this.allRecipients.set(rec, {kind:'bcc'});
			}
		}
	}

	total() {
		return this.allRecipients.size;
	}

	toCommaSeparatedString() {
		return [...this.allRecipients.keys()].join(',');
	}


	async findEmails(findEmail: Function) {
		let emailsFound = 0;

		// could cache emails - should probably timestamp them and refresh say every day
		// possibly worth it due to glabal notifies repeatedly emailing people

		for (const [user, info] of this.allRecipients.entries()) {
			try {
				const email = await findEmail(user);
				if (email) {
					++emailsFound;
					info.email = email;
				}
			}
			catch (err) {
				Recipients.recipientsLogger.printException(err, `Error while resolving email for ${user}`);
			}
		}
		return emailsFound;
	}

	getTo(): string[] { return this._get('to'); }
	getCc(): string[] { return this._get('cc'); }
	getBcc(): string[] { return this._get('bcc'); }

	private _get(kind: string): string[] {
		return Array.from(this.allRecipients)
		.filter(([_k, v]) => v.email && v.kind === kind)
		.map(([_k, v]) => v.email!);
	}
}

export interface MailParamValue {
	value: string
	noEscape?: boolean
}
export type MailParams = {[key: string]: string | MailParamValue}

export class Mailer {
	transport: Mail;
	readonly templates : Map<string, string>
	analytics: Analytics | null;
	private readonly mailerLogger = new ContextualLogger('Mailer')

	constructor(analytics: Analytics, parentLogger: ContextualLogger) {
		this.mailerLogger = parentLogger.createChild('Mail')
		this.templates = Mailer.readTemplates(this.mailerLogger)
		this.mailerLogger.info(`Read email templates: [ ${Array.from(this.templates.keys()).join(', ')} ]`)

		if (this.templates.size > 0) {
			// create reusable transporter object using the default SMTP transport
			this.transport = createTransport({
				host: SMTP_SERVER,
				port: 25, // secure port
				secure: false, // no ssl
				tls: {rejectUnauthorized: false}
			});

			this.analytics = analytics;
		}
		else {
			this.mailerLogger.warn(`No email templates found in "${TEMPLATE_PATH}", not configuring mail transport.`)
		}
	}

	// Read each .html email template in from TEMPLATE_PATH into our template map
	private static readTemplates(logger: ContextualLogger) {
		if (!fs.existsSync(TEMPLATE_PATH)) {
			throw new Error(`Mailer email template directory does not exist: ${TEMPLATE_PATH}`)
		}

		let files
		try {
			files = fs.readdirSync(TEMPLATE_PATH, {encoding: "utf8"})
		}	
		catch (err) {
			logger.printException(err, "Mailer encountered error reading email template directory")
			throw err
		}
		
		const templates : Map<string, string> = new Map()
		files.forEach( (file) => {
			const fullPath = path.join(TEMPLATE_PATH, file)
			// Skip non-html files -- why is it there?
			if (path.extname(file) !== '.html') {
				logger.info(`Skipping non-HTML file in Mailer email templates folder: "${fullPath}"`)
				return
			}

			// Template will be keyed by the basename without the extension. Name your templates intelligently!
			templates.set(path.basename(file, '.html'), fs.readFileSync(fullPath, 'utf8'))
		})

		return templates
	}

	static _escapeForHtml(s: string) {
		return s.replace(/&/g, "&amp;")
			.replace(/</g, "&lt;")
			.replace(/>/g, "&gt;");
	}

	async sendEmail(emailTemplateName: string, recipients: Recipients, subject: string, replacementParams : MailParams, findEmail: Function) {
		const setDebugData = (scope : Sentry.Scope) => {
			scope.setTag('emailTemplateName', emailTemplateName)
			scope.setExtra('recipients', recipients.toCommaSeparatedString())
			scope.setExtra('subject', subject)
			scope.setExtra('replacementTokens', JSON.stringify(replacementParams))
		}

		if (!this.transport) {
			this.mailerLogger.info(`Email not configured. Suppressing email.\n${subject}`);
			return;
		}

		const templateHtml = this.templates.get(emailTemplateName)
		if (!templateHtml) {
			const msg = `Robomerge cannot find a template titled ${emailTemplateName}, not sending email.`
			this.mailerLogger.error(msg)
			Sentry.withScope((scope) => {
				setDebugData(scope)
				Sentry.captureMessage(msg)
			})
			return
		}

		const totalToSend = await recipients.findEmails(findEmail);

		const andFinally = () => {
			this.analytics!.reportEmail(totalToSend, recipients.total());
		};

		if (totalToSend === 0) {
			andFinally();
			return;
		}

		let html = templateHtml
		const createRegex = (key: string) => { return new RegExp(`\\$\\{${key}}`, 'g') }
		for (const pKey in replacementParams) {
			const pValue = replacementParams[pKey]
			html = html.replace(createRegex(pKey), typeof pValue === 'string' ? pValue : pValue.noEscape ? pValue.value : Mailer._escapeForHtml(pValue.value) )
		}

		// Quick sanity check for unreplaced tokens
		const unreplacedTokens = html.match(createRegex('.+?'))
		if (unreplacedTokens) {
			const msg = `Mail msg may have unreplaced tokens after replacement process: ${unreplacedTokens.join(', ')}`
			this.mailerLogger.warn(msg)
			Sentry.withScope((scope) => {
				setDebugData(scope)
				scope.setExtra('html', html)
				Sentry.captureMessage(msg)
			})
		}

		const mail: Mail.Options = {
			subject: '[RoboMerge] ' + subject,
			from: FROM,
			html
		};
		

		const to = recipients.getTo();
		if (to.length !== 0) {
			mail.to = to;
		}

		const cc = recipients.getCc();
		if (cc.length !== 0) {
			mail.cc = cc;
		}

		const bcc = recipients.getBcc();
		if (bcc.length !== 0) {
			mail.bcc = bcc;
		}

		await this.transport.sendMail(mail)
			.catch(err => this.mailerLogger.printException(err, 'Sending email failed'))
			.then(andFinally);
	}
}

