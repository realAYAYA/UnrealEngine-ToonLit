// Copyright Epic Games, Inc. All Rights Reserved.
/* tls_unreal.c
** strophe XMPP client library -- TLS abstraction unreal ssl impl.
**
** Copyright (C) 2005-008 Collecta, Inc. 
**
**  This software is provided AS-IS with no warranty, either express
**  or implied.
**
**  This program is dual licensed under the MIT and GPLv3 licenses.
*/

/** @file
 *  TLS implementation with Unreal SSL.
 */

#include <errno.h>   /* EINTR */
#include <string.h>

#include "common.h"
#include "tls.h"
#include "sock.h"
#include "unreal_ssl.h"

#if defined(USE_SOCKETAPI_DISPATCH)
#include <sys/types.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include "unreal_socketapi.h"
#endif

struct _tls {
    xmpp_ctx_t *ctx;
    sock_t sock;
    void *ssl_ctx;
    void *ssl;
    int lasterror;
};

enum {
    TLS_SHUTDOWN_MAX_RETRIES = 10,
    TLS_TIMEOUT_SEC = 0,
    TLS_TIMEOUT_USEC = 100000,
};

static void _tls_sock_wait(tls_t *tls);
static void _tls_set_error(tls_t *tls, int error);
static void _tls_log_error(xmpp_ctx_t *ctx);

void tls_initialize(void)
{
}

void tls_shutdown(void)
{
    return;
}

int tls_error(tls_t *tls)
{
    return tls->lasterror;
}

tls_t *tls_new(xmpp_conn_t *conn)
{
    tls_t *tls = xmpp_alloc(conn->ctx, sizeof(*tls));

    if (tls) {
        int ret;
        memset(tls, 0, sizeof(*tls));

        tls->ctx = conn->ctx;
        tls->sock = conn->sock;
        tls->ssl_ctx = SSL_CTX_new(NULL);
        if (tls->ssl_ctx == NULL)
            goto err;

        tls->ssl = SSL_new(tls->ssl_ctx);
        if (tls->ssl == NULL)
            goto err_free_ctx;

        ret = SSL_set_socketfd(tls->ssl, conn->sock);
        if (ret < 0)
            goto err_free_ssl;
    }

    return tls;

err_free_ssl:
    SSL_shutdown(tls->ssl);
    SSL_free(tls->ssl);
err_free_ctx:
    SSL_CTX_free(tls->ssl_ctx);
err:
    xmpp_free(conn->ctx, tls);
    _tls_log_error(conn->ctx);
    return NULL;
}

void tls_free(tls_t *tls)
{
    SSL_shutdown(tls->ssl);
    SSL_free(tls->ssl);
    SSL_CTX_free(tls->ssl_ctx);
    xmpp_free(tls->ctx, tls);
}

int tls_set_credentials(tls_t *tls, const char *cafilename)
{
    return -1;
}

int tls_set_hostname(tls_t *tls, const char *hostname)
{
    int ret;
    ret = SSL_set_hostname(tls->ssl, hostname, strlen(hostname));
    return ret < 0 ? 0 : 1;
}

int tls_start(tls_t *tls)
{
    int ret;

    /* Since we're non-blocking, loop the connect call until it
       succeeds or fails */
    while (1) {
        ret = SSL_connect(tls->ssl);

        if (ret == UNREAL_SSL_ERROR_WOULDBLOCK) {
            /* wait for something to happen on the sock before looping back */
            _tls_sock_wait(tls);
            continue;
        }

        /* success or fatal error */
        break;
    }

	_tls_set_error(tls, ret);
	
    return ret <= 0 ? 0 : 1;
}

int tls_stop(tls_t *tls)
{
    int retries = 0;
    int error;
    int ret;

    while (1) {
        ++retries;
        ret = SSL_shutdown(tls->ssl);
        if (ret == 0 || retries >= TLS_SHUTDOWN_MAX_RETRIES) {
            break;
        }
        _tls_sock_wait(tls);
    }
	
	_tls_set_error(tls, ret);

    return ret < 0 ? 0 : 1;
}

int tls_is_recoverable(int error)
{
    return (error == 0 || error == UNREAL_SSL_ERROR_WOULDBLOCK);
}

int tls_pending(tls_t *tls)
{
    return SSL_pending(tls->ssl);
}

int tls_read(tls_t *tls, void * const buff, const size_t len)
{
    int ret;

    ret = SSL_read(tls->ssl, buff, len);
    _tls_set_error(tls, ret <= 0 ? SSL_get_error(tls->ssl, ret) : 0);

    return ret;
}

int tls_write(tls_t *tls, const void * const buff, const size_t len)
{
    int ret;

    ret = SSL_write(tls->ssl, buff, len);
    _tls_set_error(tls, ret <= 0 ? SSL_get_error(tls->ssl, ret) : 0);

    return ret;
}

int tls_clear_pending_write(tls_t *tls)
{
    return 0;
}

static void _tls_sock_wait(tls_t *tls)
{
	// TODO - It might be better to just set the socket to blocking as per the gnutls implementation.
    struct timeval tv;
    fd_set rfds;
    fd_set wfds;
    int ret;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(tls->sock, &rfds);
    FD_SET(tls->sock, &wfds);
	
    do {
        tv.tv_sec = TLS_TIMEOUT_SEC;
        tv.tv_usec = TLS_TIMEOUT_USEC;
        ret = select(tls->sock + 1, &rfds, &wfds, NULL, &tv);
    } while (ret == -1 && errno == EINTR);
}

static void _tls_set_error(tls_t *tls, int error)
{
    if (error != 0 && !tls_is_recoverable(error)) {
        _tls_log_error(tls->ctx);
    }
    tls->lasterror = error;
}

static void _tls_log_error(xmpp_ctx_t *ctx)
{
}
