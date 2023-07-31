// Common mutex variable around displaying error messages
let clearer: NodeJS.Timeout | null = null
export function clearErrorText() {
	const $err = $('#error')
	$err.addClass('_hidden');
	if (!clearer) {
		clearer = setTimeout(() => {
			if (clearer) {
				$err.empty();
			}
		}, 1000);
	}
}
export function setErrorText(text: string) {
	if (clearer) {
		clearTimeout(clearer);
		clearer = null;
	}
	$('#error').text(text).removeClass('_hidden');
}
export function setError(xhr: JQuery.jqXHR, error: JQuery.Ajax.ErrorTextStatus) {
	setErrorText(createErrorMessage(xhr, error))
}
export function createErrorMessage(xhr: JQuery.jqXHR, error: JQuery.Ajax.ErrorTextStatus) {
	if (xhr.responseText)
		return `${xhr.status ? xhr.status + ": " : ""}${xhr.responseText}`
	else if (xhr.status == 0) {
		document.title = "ROBOMERGE (error)";
		return "Connection error";
	}
	else
		return `HTTP ${xhr.status}: ${error}`;
}
