/*
 *  makefst - CDN FST packer and packager for Wii U homebrew
 *
 *  This code is licensed to you under the terms of the MIT License;
 *  see file LICENSE for details
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tik.h"
#include "wup.h"
#include "utils.h"

void tik_init(tik_context* ctx)
{
	memset(ctx, 0, sizeof(tik_context));
}

void tik_set_file(tik_context* ctx, FILE* file)
{
	ctx->file = file;
}

void tik_set_offset(tik_context* ctx, u64 offset)
{
	ctx->offset = offset;
}

void tik_set_size(tik_context* ctx, u32 size)
{
	ctx->size = size;
}

void tik_get_decrypted_titlekey(tik_context* ctx, u8 decryptedkey[0x10])
{
	memcpy(decryptedkey, ctx->titlekey, 16);
}

void tik_get_titleid(tik_context* ctx, u8 titleid[8])
{
	memcpy(titleid, ctx->tik.title_id, 8);
}

void tik_get_iv(tik_context* ctx, u8 iv[16])
{
	memset(iv, 0, 16);
	memcpy(iv, ctx->tik.title_id, 8);
}

void tik_decrypt_titlekey(tik_context* ctx, u8 decryptedkey[0x10]) 
{
	u8 iv[16];
	u8* key = 0x0; //TODO

	memset(decryptedkey, 0, 0x10);

	if (!key)
	{
		fprintf(stdout, "Warning, could not read common key.\n");
	}
	else
	{
		memset(iv, 0, 0x10);
		memcpy(iv, ctx->tik.title_id, 8);

		wup_init_cbc_decrypt(&ctx->aes, key, iv);
		wup_decrypt_cbc(&ctx->aes, ctx->tik.encrypted_title_key, decryptedkey, 0x10);
	}
}

void tik_process(tik_context* ctx, u32 actions)
{
	if (ctx->size < sizeof(eticket))
	{
		fprintf(stderr, "Error, ticket size too small\n");
		goto clean;
	}

	fseeko64(ctx->file, ctx->offset, SEEK_SET);
	fread((u8*)&ctx->tik, 1, sizeof(eticket), ctx->file);

	tik_decrypt_titlekey(ctx, ctx->titlekey);

	if (actions & InfoFlag)
	{
		tik_print(ctx); 
	}

clean:
	return;
}

void tik_print(tik_context* ctx)
{
	int i;
	eticket* tik = &ctx->tik;

	fprintf(stdout, "\nTicket content:\n");
	fprintf(stdout,
		"Signature Type:         %08x\n"
		"Issuer:                 %s\n",
		getle32(tik->sig_type), tik->issuer
	);

	fprintf(stdout, "Signature:\n");
	hexdump(tik->signature, 0x100);
	fprintf(stdout, "\n");

	fprintf(stdout, "Version:                %d\n", tik->version);
	fprintf(stdout, "CA CLR Version:         %d\n", tik->ca_clr_version);
	fprintf(stdout, "Signer CRL Version:     %d\n", tik->signer_crl_version);

	memdump(stdout, "Encrypted Titlekey:     ", tik->encrypted_title_key, 0x10);

	//TODO
	//if (settings_get_common_key(ctx->usersettings))
	//	memdump(stdout, "Decrypted Titlekey:     ", ctx->titlekey, 0x10);

	memdump(stdout,	"Ticket ID:              ", tik->ticket_id, 0x08);
	memdump(stdout,	"Device ID:              ", tik->device_id, 0x04);
	memdump(stdout,	"Title ID:               ", tik->title_id, 0x08);
	fprintf(stdout, "Ticket Version:         %d\n", getbe16(tik->ticket_version));
	fprintf(stdout, "License Type:           %d\n", tik->license_type);
	fprintf(stdout, "Common Key Index:       %d\n", tik->ckey_index);
	memdump(stdout,	"Account ID:             ", tik->account_id, 0x04);
	fprintf(stdout, "Audit:                  %d\n", tik->audit);

	fprintf(stdout, "Limit entry map:\n");
	for(i = 0; i < 0x40; i++) {
		printf(" %02x", tik->limit_entries[i]);

		if ((i+1) % 8 == 0)
			printf("\n");
	}
	printf("\n");
}
