/*
 *  makefst - CDN FST packer and packager for Wii U homebrew
 *
 *  This code is licensed to you under the terms of the MIT License;
 *  see file LICENSE for details
 */

#include "lib.h"
#include "crypto.h"

const u8 RSA_PUB_EXP[0x3] = {0x01,0x00,0x01};
const int HASH_MAX_LEN = 0x20;

bool VerifySha256(const void *data, u64 size, u8 hash[32])
{
	u8 calchash[32];
	ShaCalc(data, size, calchash, WUP_SHA_256);
	return memcmp(hash,calchash,32) == 0;
}

void ShaCalc(const void *data, u64 size, u8 *hash, int mode)
{
	switch(mode){
		case(WUP_SHA_1): sha1((u8*)data, size, hash); break;
		case(WUP_SHA_256): sha2((u8*)data, size, hash, 0); break;
	}
}

void SetAesCtrOffset(u8 *ctr, u64 offset)
{
	u64_to_u8(ctr+8,u8_to_u64(ctr+8,BE)|align(offset,16)/16,BE);
}

void AesCtrCrypt(const u8 *key, u8 *ctr, u8 *input, u8 *output, u64 length, u64 offset)
{
	u8 stream[16];
	aes_context aes;
	size_t nc_off = 0;
	
	clrmem(&aes,sizeof(aes_context));
	aes_setkey_enc(&aes, key, 128);
	SetAesCtrOffset(ctr,offset);
	
	aes_crypt_ctr(&aes, length, &nc_off, ctr, stream, input, output);
	
	
	return;
}

void AesCbcCrypt(const u8 *key, u8 *iv, u8 *input, u8 *output, u64 length, u8 mode)
{
	aes_context aes;
	clrmem(&aes,sizeof(aes_context));
	
	switch(mode){
		case(ENC): 
			aes_setkey_enc(&aes, key, 128);
			aes_crypt_cbc(&aes, AES_ENCRYPT, length, iv, input, output);
			return;
		case(DEC):
			aes_setkey_dec(&aes, key, 128);
			aes_crypt_cbc(&aes, AES_DECRYPT, length, iv, input, output); 
			return;
		default:
			return;
	}
}

bool RsaKeyInit(rsa_context* ctx, const u8 *modulus, const u8 *private_exp, const u8 *exponent, u8 rsa_type)
{
	// Sanity Check
	if(!ctx)
		return false;
	
	rsa_init(ctx, RSA_PKCS_V15, 0);
	
	u16 n_size = 0;
	u16 d_size = 0;
	u16 e_size = 0;
	
	switch(rsa_type){
		case RSA_2048:
			ctx->len = 0x100;
			n_size = 0x100;
			d_size = 0x100;
			e_size = 3;
			break;
		case RSA_4096:
			ctx->len = 0x200;
			n_size = 0x200;
			d_size = 0x200;
			e_size = 3;
			break;
		default: return false;
	}
	
	if (modulus && mpi_read_binary(&ctx->N, modulus, n_size))
		goto clean;
	if (exponent && mpi_read_binary(&ctx->E, exponent, e_size))
		goto clean;
	if (private_exp && mpi_read_binary(&ctx->D, private_exp, d_size))
		goto clean;
	

	return true;
clean:
	rsa_free(ctx);
	return false;
}

u8 GetRsaType(u32 sig_type)
{
	switch(sig_type){
		case RSA_4096_SHA1:
		case RSA_4096_SHA256:
			return RSA_4096;
		case RSA_2048_SHA1:
		case RSA_2048_SHA256:
			return RSA_2048;			
	}
	return INVALID_SIG_TYPE;
}

u32 GetSigHashType(u32 sig_type)
{
	switch(sig_type){
		case RSA_4096_SHA1:
		case RSA_2048_SHA1:
		case ECC_SHA1:
			return WUP_SHA_1;
		case RSA_4096_SHA256:
		case RSA_2048_SHA256:
		case ECC_SHA256:
			return WUP_SHA_256;
	}
	return 0;
}

int GetRsaHashType(u32 sig_type)
{
	switch(sig_type){
		case RSA_4096_SHA1:
		case RSA_2048_SHA1:
			return SIG_RSA_SHA1;
		case RSA_4096_SHA256:
		case RSA_2048_SHA256:
			return SIG_RSA_SHA256;
	}
	return 0;
}

u32 GetSigHashLen(u32 sig_type)
{
	switch(sig_type){
		case RSA_4096_SHA1:
		case RSA_2048_SHA1:
		case ECC_SHA1:
			return SHA_1_LEN;
		case RSA_4096_SHA256:
		case RSA_2048_SHA256:
		case ECC_SHA256:
			return SHA_256_LEN;
	}
	return 0;
}

bool CalcHashForSign(void *data, u64 len, u8 *hash, u32 sig_type)
{
	if(GetSigHashType(sig_type) == 0)
		return false;

	ShaCalc(data, len, hash, GetSigHashType(sig_type));
	
	return true;
}

int RsaSignVerify(void *data, u64 len, u8 *sign, const u8 *mod, const u8 *priv_exp, u32 sig_type, u8 rsa_mode)
{
	int rsa_result = 0;
	rsa_context ctx;
	u8 hash[HASH_MAX_LEN];
		
	if(!RsaKeyInit(&ctx, mod, priv_exp, (u8*)RSA_PUB_EXP, GetRsaType(sig_type)))
		return -1;
		
	if(!CalcHashForSign(data, len, hash, sig_type))
		return -1;		

	if(rsa_mode == WUP_RSA_VERIFY)
		rsa_result = rsa_pkcs1_verify(&ctx, RSA_PUBLIC, GetRsaHashType(sig_type), 0, hash, sign);
	else // WUP_RSA_SIGN
		rsa_result = rsa_rsassa_pkcs1_v15_sign(&ctx, RSA_PRIVATE, GetRsaHashType(sig_type), 0, hash, sign);
	
	rsa_free(&ctx);
	return rsa_result;
}

void decrypt_titlekey(u8 titlekey[16], const u8 title_id[8], const u8 ckey[16])
{
    aes_context ctx;
    u8 iv[16] = {0};
    
    memcpy(iv, title_id, 8);
    aes_setkey_dec(&ctx, ckey, 128);
    aes_crypt_cbc(&ctx, AES_DECRYPT, 16, iv, titlekey, titlekey);
}

int get_tikfile_titlekey(char* fname_tik, u8 titlekey[16], const u8 ckey[16])
{
    // todo: sanity-check tik
    u8 tid[8];
    FILE* f_tik = fopen(fname_tik, "rb");
    if(!f_tik)
    {
        return -1;
    }
    fseek(f_tik, 0x1BF, SEEK_SET);
    fread(titlekey, 1, 16, f_tik);
    fseek(f_tik, 0x1DC, SEEK_SET);
    fread(tid, 1, sizeof(tid), f_tik);
    fclose(f_tik);
    decrypt_titlekey(titlekey, tid, ckey);
    return 0;
}
