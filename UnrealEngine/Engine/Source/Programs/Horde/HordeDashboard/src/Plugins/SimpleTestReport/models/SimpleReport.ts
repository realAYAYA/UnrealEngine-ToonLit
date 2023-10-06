// Copyright Epic Games, Inc. All Rights Reserved.

export enum LogLevel {
    Info = "Info",
    Error = "Error",
    Warning = "Warning",
}

export type SimpleReport = {
    Description: string;
    HasSucceeded: boolean;
    Status: string;
    ReportCreatedOn: string;
    TotalDurationSeconds: number;
    URLLink: string;
    Logs: string[];
    Errors: string[];
    Warnings: string[];
}

export type HistoryItem = {
    TestdataId: string;
    Url: string;
    Change: number;
    HasSucceeded: boolean;
    Date: string;
    TotalDurationSeconds: number;
    ErrorCount: number;
}