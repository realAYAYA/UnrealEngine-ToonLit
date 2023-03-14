// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDTLSHandler, Log, All);

/** Forward some OpenSSL types  */

struct evp_pkey_st;
typedef struct evp_pkey_st EVP_PKEY;

struct x509_st;
typedef struct x509_st X509;

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

struct ssl_st;
typedef struct ssl_st SSL;

struct bio_st;
typedef struct bio_st BIO;

typedef struct bio_method_st BIO_METHOD;

enum class EDTLSContextType
{
	Server,
	Client
};

const TCHAR* LexToString(EDTLSContextType ContextType);