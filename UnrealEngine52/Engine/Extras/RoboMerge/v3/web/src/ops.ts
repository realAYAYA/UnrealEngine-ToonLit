import { clearErrorText, createErrorMessage, setErrorText } from "./ui"

let opPending = false

export type Op = (botname: string, nodeName: string, op: string,
	successCallback: SuccessCallback, failureCallback: (error: string) => void) => JQuery.jqXHR<any>

type SuccessCallback = (data: any | null) => void

export function nodeAPIOp(botname: string, nodeName: string, op: Op,
		successCallback: SuccessCallback, failureCallback: (error: string) => void) {
	return genericAPIOp(`/api/op/bot/${encodeURIComponent(botname)}/node/${encodeURIComponent(nodeName)}/op${op}`, successCallback, failureCallback)
}
export function edgeAPIOp(botname: string, nodeName: string, edgeName: string, op: Op,
		successCallback: SuccessCallback, failureCallback: (error: string) => void) {
	return genericAPIOp(`/api/op/bot/${encodeURIComponent(botname)}/node/${encodeURIComponent(nodeName)}/edge/${encodeURIComponent(edgeName)}/op${op}`, successCallback, failureCallback)
}

function genericAPIOp(url: string, successCallback: SuccessCallback, failureCallback: (error: string) => void) {
	if (opPending) {
		alert("Operation already pending");
		return;
	}
	opPending = true;
	clearErrorText();

	return $.ajax({
		url,
		type: 'post',
		contentType: 'application/json',
		success: function(data) {
			opPending = false;
			if (successCallback) {
				successCallback(data);
			}
		},
		error: function(xhr, error, status) {
			opPending = false;
			const errMsg = createErrorMessage(xhr, error)
			setErrorText(errMsg);
			if (failureCallback) {
				failureCallback(errMsg);
			} else if (successCallback) {
				// Older behavior when old code doesn't specify a failureCallback
				successCallback(null);
			}
		}
	});
}