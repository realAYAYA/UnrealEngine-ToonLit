// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsPlatformSslCertificateManager.h"

#include "Ssl.h"
#include "SslError.h"

#include "Misc/ConfigCacheIni.h"

#if WITH_SSL

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"

#include <wincrypt.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#include "Windows/HideWindowsPlatformTypes.h"

void FWindowsPlatformSslCertificateManager::BuildRootCertificateArray()
{
	FSslCertificateManager::BuildRootCertificateArray();

	bool bUsePlatformProvidedCertificates = true;
	if (GConfig->GetBool(TEXT("SSL"), TEXT("bUsePlatformProvidedCertificates"), bUsePlatformProvidedCertificates, GEngineIni) && !bUsePlatformProvidedCertificates)
	{
		return;
	}

	// Add certificates from the windows root certificate store
	HCERTSTORE SystemRootStore = CertOpenSystemStoreW(0, L"ROOT");
	if (SystemRootStore)
	{
		PCCERT_CONTEXT CertContext = nullptr;
		while (nullptr != (CertContext = CertEnumCertificatesInStore(SystemRootStore, CertContext)))
		{
			if ((CertContext->dwCertEncodingType & X509_ASN_ENCODING) != 0)
			{
				if (X509* Certificate = d2i_X509(nullptr, (const unsigned char**)&CertContext->pbCertEncoded, CertContext->cbCertEncoded))
				{
					AddCertificateToRootCertificateArray(Certificate);
				}
				else
				{
					TCHAR Name[128];
					CertGetNameString(CertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, Name, UE_ARRAY_COUNT(Name));
					UE_LOG(LogSsl, Log, TEXT("Unable to convert certificate: name:%s error:%s"), Name, *GetSslErrorString());
				}
			}
			else
			{
				TCHAR Name[128];
				CertGetNameString(CertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, Name, UE_ARRAY_COUNT(Name));
				UE_LOG(LogSsl, Log, TEXT("Unhandled certificate encoding: name:%s encodingType:0x%08x"), Name, CertContext->dwCertEncodingType);
			}
		}
		CertCloseStore(SystemRootStore, 0);
	}
	else if (RootCertificateArray.Num() > 0)
	{
		UE_LOG(LogSsl, Warning, TEXT("Unable to open ROOT certificate store. Platform provided certificates will not be used"));
	}
	else
	{
		UE_LOG(LogSsl, Warning, TEXT("Unable to open ROOT certificate store. TLS may not work."));
	}
}

#endif