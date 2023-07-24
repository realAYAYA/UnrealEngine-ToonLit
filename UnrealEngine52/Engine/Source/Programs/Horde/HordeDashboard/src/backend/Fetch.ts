// Copyright Epic Games, Inc. All Rights Reserved.

import { ErrorHandler, ErrorInfo } from "../components/ErrorHandler";
//import { setDatadogUser } from './Datadog';

export enum ChallengeStatus {
    Ok,
    Unauthorized,
    Error
}

export interface FetchRequestConfig {
    formData?: boolean;
    responseBlob?: boolean;
    params?: Record<string, string | string[] | number | boolean | undefined>;
    suppress404?: boolean;
}

export type FetchResponse = {
    data: any;
    status: number;
}

function handleError(errorIn: ErrorInfo, force?: boolean) {

    const e = { ...errorIn, hordeError: true } as ErrorInfo;

    ErrorHandler.set(e, force ?? false);

}

/** Fetch convenience wrapper */
export class Fetch {

    get(url: string, config?: FetchRequestConfig) {

        url = this.buildUrl(url, config);

        return new Promise<FetchResponse>(async (resolve, reject) => {

            try {

                const response = await fetch(url, this.buildRequest("GET"));

                if (response.ok) {
                    this.handleResponse(response, url, "GET", resolve, reject, config);
                    return;
                }

                if (response.status === 404) {
                    if (config?.suppress404) {
                        reject(`Received suppressed 404 on ${url}`);
                        return;
                    }
                }

                const challenge = await this.challenge();

                if (challenge === ChallengeStatus.Unauthorized) {

                    this.login(window.location.toString());
                    return;
                }

                if (response.status === 502) {
                    throw new Error(`502 Gateway error connecting to server`);
                }

                throw response.statusText ? response.statusText : response.status;

            } catch (reason) {

                // Note: the fetch request sets error for redirects, this is opaque as per the spec
                // so all we get is an error string and can't detect AccessDenied, etc in redirect :/
                let message = `Error in request, ${reason}`;

                handleError({
                    reason: message,
                    mode: "GET",
                    url: url
                });

                reject(message);
            };
        });
    }

    post(url: string, data?: any, config?: FetchRequestConfig) {

        url = this.buildUrl(url, config);

        return new Promise<FetchResponse>((resolve, reject) => {

            fetch(url, this.buildRequest("POST", data, config)).then(response => {

                this.handleResponse(response, url, "POST", resolve, reject, config);

            }).catch(reason => {

                // Note: the fetch request sets error for redirects, this is opaque as per the spec
                // so all we get is an error string and can't detect AccessDenied, etc in redirect :/
                let message = `Possible permission issue, ${reason}`;

                handleError({
                    reason: message,
                    mode: "POST",
                    url: url
                }, true);

                reject(message);
            });
        });

    }

    put(url: string, data?: any, config?: FetchRequestConfig) {

        url = this.buildUrl(url, config);

        return new Promise<FetchResponse>((resolve, reject) => {

            fetch(url, this.buildRequest("PUT", data)).then(response => {

                this.handleResponse(response, url, "PUT", resolve, reject, config);

            }).catch(reason => {

                handleError({
                    reason: reason,
                    mode: "PUT",
                    url: url
                });

                reject(reason);
            });
        });

    }


    delete(url: string, config?: FetchRequestConfig) {

        url = this.buildUrl(url, config);

        return new Promise<FetchResponse>((resolve, reject) => {

            fetch(url, this.buildRequest("DELETE")).then(response => {

                this.handleResponse(response, url, "DELETE", resolve, reject, config);

            }).catch(reason => {

                handleError({
                    reason: reason,
                    mode: "DELETE",
                    url: url
                });

                reject(reason);
            });
        });
    }

    login(redirect: string) {

        if (this.debugToken || this.logout) {
            return;
        }

        window.location.assign("/api/v2/dashboard/login?redirect=" + btoa(redirect ?? "/index"));
    }

    private async handleResponse(response: Response, url: string, mode: string, resolve: (value: FetchResponse | PromiseLike<FetchResponse>) => void, reject: (reason?: any) => void, config?: FetchRequestConfig) {


        if (response.status === 401) {

            return reject(response.statusText);
        }

        if (!response.ok) {

            let message = response.statusText;

            if (response.url?.indexOf("AccessDenied") !== -1) {

                handleError({
                    response: response,
                    url: url,
                    title: "Access Denied"
                });

            } else {

                if (response.status !== 404 || !config?.suppress404) {

                    let errorInfo: ErrorInfo = {
                        response: response,
                        url: url
                    }

                    let json: any = undefined;

                    try {
                        json = await response.json();
                    } catch {

                    }

                    // dynamic detection of horde formatted error
                    if (json && json.time && json.message && json.level) {
                        errorInfo.format = json;
                        message = json.message;
                        if (json.id) {
                            message = `(Error ${json.id}) - ${message}`;
                        }
                    }

                    handleError(errorInfo);

                }
            }

            reject(message);
            return;
        }

        if (config?.responseBlob) {
            response.blob().then(blob => {
                resolve({
                    data: blob,
                    status: response.status
                });
            }).catch(reason => {

                handleError({
                    response: response,
                    url: url,
                    reason: reason
                });

                reject(reason);
            });
        }
        else {
            response.clone().json().then(data => {
                resolve({
                    data: data,
                    status: response.status
                });
            }).catch((reason) => {
                response.clone().text().then(text => {
                    resolve({
                        data: text,
                        status: response.status
                    });
                }).catch(reason => {

                    handleError({
                        response: response,
                        url: url,
                        reason: reason
                    });

                    reject(reason);
                });
            });
        }

    }

    private buildRequest(method: "GET" | "POST" | "PUT" | "DELETE", data?: any, config?: FetchRequestConfig): RequestInit {

        const headers = this.buildHeaders();

        let body: any = undefined;

        if (method === "POST" || method === "PUT") {
            if (data) {
                if (config?.formData) {
                    body = data;
                } else {
                    headers['Content-Type'] = 'application/json';
                    body = JSON.stringify(data);
                }
            }
        }

        // no-cors doesn't allow authorization header

        const requestInit: RequestInit = {
            method: method,
            mode: "cors",
            cache: "no-cache",
            credentials: this.debugToken ? "same-origin" : 'include',
            redirect: "error",
            headers: headers,
            body: body
        };

        return requestInit;
    }

    private get withCredentials(): boolean {
        return !!this.authorization && !!this.authorization.length;
    }

    private buildHeaders(): Record<string, string> {

        const headers: Record<string, string> = {};

        if (this.withCredentials) {
            headers["Authorization"] = this.authorization!;
        }

        return headers;
    }


    private buildUrl(url: string, config?: FetchRequestConfig): string {

        while (url.startsWith("/")) {
            url = url.slice(1);
        }

        while (url.endsWith("/")) {
            url = url.slice(0, url.length);
        }

        url = `${this.baseUrl}/${url}`;

        if (!config?.params) {
            return url;
        }

        const keys = Object.keys(config.params).filter((key) => {
            return config.params![key] !== undefined;
        });

        const query = keys.map((key) => {
            const value = config.params![key]!;
            if (Array.isArray(value)) {
                return (value as string[]).map(v => {
                    return encodeURIComponent(key) + '=' + encodeURIComponent(v);
                });
            } else {
                return encodeURIComponent(key) + '=' + encodeURIComponent(value as any);
            }

        }).flat().join('&');

        return query.length ? `${url}?${query}` : url;

    }

    async challenge(): Promise<ChallengeStatus> {

        try {
            const url = this.buildUrl("/api/v1/dashboard/challenge");

            const result = await fetch(url, this.buildRequest("GET"));

            if (result.ok) {
                return ChallengeStatus.Ok;
            }

            if (result.status === 401 || result.status === 404) {

                if (this.debugToken) {
                    // raise error to check debug token expired
                    handleError({
                        reason: "Unauthorized challenge with debug token, it may have expired",
                        mode: "GET",
                        url: url
                    });
                }

                return ChallengeStatus.Unauthorized;
            }
        } catch (reason) {
            console.error(reason);
        }

        return ChallengeStatus.Error;

    }

    // debug token
    private setAuthorization(authorization: string | undefined) {
        this.authorization = authorization;
    }

    setDebugToken(token?: string) {

        if (!token) {

            this.debugToken = undefined;
            this.setAuthorization(undefined);
            return;
        }

        this.debugToken = token;
        this.setAuthorization(`Bearer ${this.debugToken}`);

    }

    setBaseUrl(url?: string) {

        if (!url) {
            this.baseUrl = "";
            return;
        }

        this.baseUrl = url;
    }

    logout: boolean = false;

    private baseUrl = "";
    private debugToken?: string;
    private authorization?: string;

}