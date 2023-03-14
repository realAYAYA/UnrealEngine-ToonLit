// Copyright Epic Games, Inc. All Rights Reserved.

import semver = require('semver')
import { ContextualLogger } from './logger';

export interface BuildVersion {
	build: string
	cl: string
}

export class VersionReader {
    private static buildVersion: BuildVersion
    private static packageVersion: semver.SemVer

    static init(logger: ContextualLogger) {
        if (!VersionReader.buildVersion) {
            VersionReader.buildVersion = VersionReader.readBuildVersion();
            logger.info(`Build version initialized to ${VersionReader.toString()}`)
        }
        if (!VersionReader.packageVersion) {
            VersionReader.packageVersion = VersionReader.readPackageVersion();
            logger.info(`Package version initialized to ${VersionReader.getPackageVersionString()}`)
        }
    }

    private static readBuildVersion() : BuildVersion {
        try {
            const versionStr = require('fs').readFileSync('./version.json');
            return JSON.parse(versionStr);
        }
        catch (err) {
            throw new Error(`Please check Robomerge files. Error processing version.json: ${err.toString()}`)
        }
    }

    static readPackageVersion(): semver.SemVer {
        try {
            const versionStr = require('fs').readFileSync('./package.json');
            const packageJson = JSON.parse(versionStr);
            const packageVersionString : string | undefined = packageJson["version"]
            if (!packageVersionString) {
                // No version found!
                return new semver.SemVer('0.0.0')
            }

            const packageSemVer = semver.coerce(packageVersionString)
            if (!packageSemVer) {
                throw new Error('Could not parse semantic version in package.json')
            }

            return packageSemVer
        }
        catch (err) {
            throw new Error(`Please check Robomerge files. Error processing package.json: ${err.toString()}`)
        }
    }

    static getBuildVersionObj() : BuildVersion {
        if (!VersionReader.buildVersion) {
            VersionReader.buildVersion = VersionReader.readBuildVersion();
        }
        return VersionReader.buildVersion
    }

    static getPackageVersion() : semver.SemVer {
        if (!VersionReader.packageVersion) {
            VersionReader.packageVersion = VersionReader.readPackageVersion()
        }
        return VersionReader.packageVersion
    }

    static getPackageVersionString() : string {
        return VersionReader.getPackageVersion().raw
    }

    /**
     * Outputs Build Version object to a string suitable for display to the end user.
     */
    static toString() : string {
        const ver = this.getBuildVersionObj()
        return `build ${ver.build} (cl ${ver.cl})`
    }

    /**
     * Outputs Build Version object to a string suitable for small UI.
     */
    static getShortVersion() : string {
        const ver = this.getBuildVersionObj()
        return `${ver.build}-${ver.cl}`
    }

    static getBuildNumber(): string {
        const ver =  this.getBuildVersionObj()
        return ver.build
    }
}