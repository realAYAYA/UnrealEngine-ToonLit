// Copyright Epic Games, Inc. All Rights Reserved.

import * as CryptoJS from "crypto-js";
import { openDB, DBSchema, IDBPDatabase } from 'idb';

interface CacheSchema extends DBSchema {
    'cache-item': {
        key: string;
        value: {
            key: string;
            encodedData: string;
            storeTime: number;
        };
        indexes: { 'by-key': string };
    };
}

const DB_VERSION = 8;
// @todo: make this tuneable per cache type
const DB_MAX_ITEMS = 2048;

export abstract class LocalCache<ValueType> {

    async getItem(hash: string): Promise<ValueType | undefined> {

        const cached = this.cache.get(hash);

        if (cached) {
            return cached;
        }

        if (!this.initialized) {
            return undefined;
        }

        try {

            const cipherHash = CryptoJS.SHA256(hash).toString();

            const item = await this.db!.get("cache-item", cipherHash);

            if (!item) {
                //console.log("cache miss", cipherHash);
                return undefined;
            }

            const bytes = CryptoJS.AES.decrypt(item.encodedData, hash);
            const value = JSON.parse((bytes.toString(CryptoJS.enc.Utf8))) as ValueType;

            this.cache.set(hash, value);

            //console.log("cache hit");

            return value;

        } catch (err) {
            console.error(err);
        }

        return undefined;

    }

    async storeItem(hash: string, value: ValueType) {

        if (!this.initialized) {
            return undefined;
        }

        try {

            this.cache.set(hash, value);

            const cipherHash = CryptoJS.SHA256(hash).toString();
            const encodedData = CryptoJS.AES.encrypt(JSON.stringify(value), hash).toString();

            //console.log("cache store", cipherHash);

            await this.db!.put("cache-item", { key: cipherHash, storeTime: Date.now(), encodedData: encodedData }, cipherHash)

        } catch (err) {
            console.error(err);
        }

    }

    async evict(): Promise<boolean> {

        try {

            if (this.verbose) {
                console.log(`Cache ${this.name} : ${this.storeName}, checking for eviction`);
            }

            const count = await this.db!.count("cache-item");

            if (count >= DB_MAX_ITEMS) {

                console.log(`Cache: ${this.storeName} evicting data`)

                await this.db!.clear("cache-item");

            }

        } catch (err) {
            console.error(err)
            return false;
        }

        return true;

    }

    initialize() {

        if (this.failure || this.attempted) {
            return false;
        }

        this.attempted = true;

        console.log(`Initializing local cache, ${this.name + " : " + this.storeName}`);

        const innerInit = async () => {

            try {
                let success = true;

                this.db = await openDB<CacheSchema>(this.storeName, DB_VERSION, {

                    upgrade: (db) => {

                        console.log("upgrade");
                        try {
                            db.deleteObjectStore('cache-item');
                        } catch (reason) {
                            console.log("Did not delete existing cache", reason);
                        }
                        
                        const store = db.createObjectStore('cache-item');
                        store.createIndex('by-key', 'key');

                    },
                    blocked: () => {
                        success = false;
                        console.log("blocked");
                    },
                    blocking: () => {
                        this.initialized = false;
                        this.db?.close();
                        console.log("blocking");
                    },
                    terminated: () => {
                        this.initialized = false;
                        console.log("terminated");                                                
                    }
                });

                if (success) {
                    success = await this.evict();
                }
                
                if (!success) {
                    this.failure = true;
                    return;
                }
            } catch (reason) {

                this.failure = true;
                console.error(reason);
                return;
            }

            this.initialized = true;

        };

        innerInit();

    }

    constructor(name: string, storeName: string, description: string) {

        this.name = name;
        this.storeName = storeName;
        this.description = description;

    }

    name: string;
    storeName: string;
    description: string;

    initialized = false;

    attempted = false;

    verbose = true;

    failure = false;

    db?: IDBPDatabase<CacheSchema>;

    cache: Map<string, ValueType> = new Map();

}