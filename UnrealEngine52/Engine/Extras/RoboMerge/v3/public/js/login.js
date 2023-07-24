// Copyright Epic Games, Inc. All Rights Reserved.
"use strict"

const COOKIE_REGEX = /\s*(.*?)=(.*)/;
function getCookie(name) {
	for (const cookieKV of decodeURIComponent(document.cookie).split(';')) {
		const match = cookieKV.match(COOKIE_REGEX);
		if (match && match[1].toLowerCase() === name.toLowerCase()) {
			return match[2];
		}
	}
	return null;
}

$(() => {

	const $form = $('.login-form form');

	const clearError = () => $('#login-error').hide();

	const $nameInput = $('input[name=user]', $form).on('input', clearError);
	const $passwordInput = $('input[name=password]', $form).on('input', clearError);

	$form.on('submit', () => {
		const $submitButton = $('button', $form).prop('disabled', true).text('Signing in ...');

		// clear auth on attempt to log in
		document.cookie = 'auth=;';

		const postData = {
			user: $nameInput.val(),
			password: $passwordInput.val()
		};

		$.post('/dologin', postData)
		.then((token) => {
			// win!
			clearError();

			// Check to see if we have a redirect request from the website
			var urlParams = new URLSearchParams(window.location.search);
			var redirectString = "/"
			if (urlParams.has("redirect")) {
				redirectString = decodeURIComponent(urlParams.get("redirect"))
			}

			document.cookie = `auth=${token}; secure=true`;
			document.cookie = 'redirect_to=;';

			// @todo remove redirect_to cookie on server!
			window.location = window.origin + redirectString

		}, (xhr, status, error) => {
			// bad luck
			$submitButton.prop('disabled', false).text('Sign in');
			$passwordInput.focus();

			$('#login-error')
			.text(error === 'Unauthorized' ? 'Invalid username or password!' : 'Failed to authenticate, please try later')
			.show();
		});

		return false;
	});
});
