// Copyright Epic Games, Inc. All Rights Reserved.

import { action, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import { Checkbox, Dialog, DialogType, PrimaryButton, Stack, Text } from '@fluentui/react';
import React from 'react';
import { getSiteConfig } from '../backend/Config';
import dashboard from '../backend/Dashboard';


// Explicit error message
export type ErrorFormatted = {
   time: Date | string;
   level: "Error" | "Warning";
   message: string;
   id: number;
   format: string;
   properties: Record<string, string | number | boolean>
}

export type ErrorInfo = {

   // general project rejection
   reason?: any;

   // fetch response object
   response?: Response;

   // request url (should match url in fetch response if any)
   url?: string;

   // REST mode
   mode?: "GET" | "PUT" | "DELETE" | "POST";

   // custom title override
   title?: string;

   // custom message override
   message?: string;

   format?: ErrorFormatted;

}

export class ErrorHandler {

   @observable
   static error?: ErrorInfo;

   @action
   static set(infoIn: ErrorInfo, update?: boolean): void {

      this.print(infoIn);

      const config = getSiteConfig();

      if (!update) {// && !this.unauthorized(infoIn)) {
         if (config.environment === "production") {
            return;
         }
      }


      if (infoIn.response?.status === 502) {
         return;
      }

      const hash = this.hash(infoIn);
      if (hash && this.filter.has(hash)) {
         return;
      }

      if (this.error && !update) {
         return;
      }

      const info = { ...infoIn };
      this.error = info;
   }

   static get message(): string {

      const error = this.error;

      if (!error) {
         return "";
      }

      if (error.format?.message) {
         return error.format?.message;
      }

      if (error.message) {
         return error.message;
      }

      if (error.reason) {
         return error.reason.toString();
      }

      if (error.response) {
         return `${error.response.status}: ${error.response.statusText}`;
      }

      return "";

   }

   static unauthorized(infoIn?: ErrorInfo): boolean {

      let error = infoIn;

      if (!error) {
         error = this.error;
      }

      if (!error) {
         return false;
      }

      if (!error.response) {
         return false;
      }

      if (error.response.status === 401 || error.response.status === 403 || error.response.url.toLowerCase().indexOf("accessdenied") !== -1) {
         return true;
      }

      return false;

   }

   static get reject(): string {

      return this.message;

   }

   static hash(error: ErrorInfo): string {

      const url = error.url ?? error?.response?.url;

      // filter on end point, without query strings
      if (url) {
         return url.split(/[?#]/)[0];
      }

      // filter on reason
      if (error.reason) {
         return error.reason;
      }

      return "";
   }

   static print(info: ErrorInfo) {

      // this will also go out to datadog if configured
      const message = `${info.response ? JSON.stringify(info.response) : ""} ${info.reason ? info.reason : ""}`;

      if (!message) {
         return;
      }
      console.error(message);
   }

   @action
   static clear(): void {

      if (!this.error) {
         return;
      }

      if (this.filterError) {
         const hash = this.hash(this.error);
         if (hash) {
            this.filter.add(hash);
         }
      }

      this.filterError = false;

      this.error = undefined;
   }


   static filter = new Set<string>();

   static filterError = false;

}

export const ErrorDialog: React.FC = observer(() => {

   const error = ErrorHandler.error;

   if (!error) {
      return <div />;
   }

   const message = ErrorHandler.message;
   const unauthorized = ErrorHandler.unauthorized();

   let title = error.title;
   if (!title) {
      title = unauthorized ? "Permission Error" : "Oops, there was a problem...";
   }

   const dialogContentProps = {
      type: DialogType.normal,
      title: title
   };

   const helpEmail = dashboard.helpEmail ?? "Not Configured";
   const helpSlack = dashboard.helpSlack ?? "Not Configured";

   return (
      <Dialog
         minWidth={800}
         hidden={!error}
         onDismiss={() => ErrorHandler.clear()}
         modalProps={{
            isBlocking: true
         }}
         dialogContentProps={dialogContentProps}>

         <Stack tokens={{ childrenGap: 12 }}>
            <Text styles={{ root: { whiteSpace: "pre-wrap", fontSize: 16, color: "#EC4C47" } }}>{message}</Text>
            {!!error.url && <Text variant="medium">{`URL: ${error.url}`}</Text>}
            <Stack>
               <Stack tokens={{ childrenGap: 18 }} style={{ paddingTop: 12 }}>
                  <Text style={{ fontSize: 15 }}>If the issue persists, please contact us for assistance.</Text>
                  <Stack tokens={{ childrenGap: 14 }} style={{ paddingLeft: 12, paddingTop: 2 }}>
                     <Stack horizontal tokens={{ childrenGap: 12 }}><Text style={{ fontSize: 15 }}>Email:</Text><Text style={{ fontSize: 15, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>{helpEmail}</Text></Stack>
                     <Stack horizontal tokens={{ childrenGap: 12 }}><Text style={{ fontSize: 15 }}>Slack:</Text><Text style={{ fontSize: 15, fontWeight: 400, fontFamily: "Horde Open Sans SemiBold" }}>{helpSlack}</Text></Stack>
                  </Stack>
               </Stack>

               <Stack horizontal style={{ paddingTop: 24 }}>
                  <Stack grow />
                  <Stack styles={{ root: { paddingTop: 6, paddingRight: 24 } }}>
                     <Checkbox label="Don't show errors like this again" defaultChecked={false} onChange={(e, b) => { ErrorHandler.filterError = !!b; }} />
                  </Stack>
                  <Stack>
                     <PrimaryButton text="Dismiss" onClick={() => { ErrorHandler.clear(); }} />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Dialog>
   );
});
