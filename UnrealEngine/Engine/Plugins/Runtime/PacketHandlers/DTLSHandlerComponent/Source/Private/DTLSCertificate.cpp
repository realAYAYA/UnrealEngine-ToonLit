// Copyright Epic Games, Inc. All Rights Reserved.

#include "DTLSCertificate.h"
#include "Misc/ScopeExit.h"

#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include <openssl/x509.h>
THIRD_PARTY_INCLUDES_END
#undef UI


constexpr uint32 FDTLSFingerprint::Length;


FDTLSCertificate::FDTLSCertificate()
	: PKey(nullptr)
	, Certificate(nullptr)
{
}

FDTLSCertificate::~FDTLSCertificate()
{
	FreeCertificate();
}

void FDTLSCertificate::FreeCertificate()
{
	if (Certificate)
	{
		X509_free(Certificate);
		Certificate = nullptr;
	}

	if (PKey)
	{
		EVP_PKEY_free(PKey);
		PKey = nullptr;
	}

	Fingerprint.Reset();
}

bool FDTLSCertificate::GenerateCertificate(const FTimespan& Lifetime)
{
	bool bSuccess = false;

	ON_SCOPE_EXIT
	{
		if (!bSuccess)
		{
			FreeCertificate();
		}
	};

	check(!PKey && !Certificate);

	PKey = EVP_PKEY_new();
	Certificate = X509_new();

	// will be freed when the pkey is destroyed
	RSA* RSAKey = RSA_new();
	
	{
		BIGNUM* BigNum = BN_new();
		BN_set_word(BigNum, RSA_F4);
		RSA_generate_key_ex(RSAKey, 2048, BigNum, nullptr);
		BN_free(BigNum);
	}

	EVP_PKEY_assign_RSA(PKey, RSAKey);

	X509_set_version(Certificate, 2);

	ASN1_INTEGER* SerialNumber = ASN1_INTEGER_new();
	
	{
		BIGNUM* BigNumTemp = BN_new();
		BN_rand(BigNumTemp, 159, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY);
		BN_to_ASN1_INTEGER(BigNumTemp, SerialNumber);
		BN_free(BigNumTemp);
	}

	int32 ResultCode = X509_set_serialNumber(Certificate, SerialNumber);
	if (ResultCode == 0)
	{
		UE_LOG(LogDTLSHandler, Error, TEXT("Failed to set serial number on cert: %d"), ResultCode);
		return bSuccess;
	}

	ASN1_INTEGER_free(SerialNumber);

	X509_gmtime_adj(X509_get_notBefore(Certificate), 0);
	X509_gmtime_adj(X509_get_notAfter(Certificate), (long)Lifetime.GetTotalSeconds());

	X509_set_pubkey(Certificate, PKey);

	X509_NAME* SubjectName = X509_get_subject_name(Certificate);

	X509_NAME_add_entry_by_txt(SubjectName, "C",  MBSTRING_ASC, (unsigned char *)"US", -1, -1, 0);
	X509_NAME_add_entry_by_txt(SubjectName, "CN", MBSTRING_ASC, (unsigned char *)"Unreal Engine", -1, -1, 0);

	ResultCode = X509_set_issuer_name(Certificate, SubjectName);
	if (ResultCode == 0)
	{
		UE_LOG(LogDTLSHandler, Error, TEXT("Failed to set issuer name"));
		return bSuccess;
	}

	ResultCode = X509_sign(Certificate, PKey, EVP_sha256());
	if (ResultCode <= 0)
	{
		UE_LOG(LogDTLSHandler, Error, TEXT("Failed to sign cert: %d"), ResultCode);
		return bSuccess;
	}

	Fingerprint.Reset();

	uint32 HashLen = FDTLSFingerprint::Length;

	ResultCode = X509_digest(Certificate, EVP_sha256(), Fingerprint.Data, &HashLen);
	if (ResultCode <= 0)
	{
		UE_LOG(LogDTLSHandler, Error, TEXT("Failed to hash cert: %d"), ResultCode);
		return bSuccess;
	}

	bSuccess = true;

	return bSuccess;
}
