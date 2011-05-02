#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "blf_keygen.h"

blf_keygen *init_keygen(u_int8_t *key, int keylen, u_int8_t *iv) {
    blf_ctx *c = (blf_ctx *) malloc(sizeof(blf_ctx));
    blf_keygen *k = (blf_keygen *) malloc(sizeof(blf_keygen));
    blf_key(c,key,keylen);
    k->context = c;
	memcpy(k->iv,iv,8);
	refill_keygen(k);
    return k;
}


void refill_keygen(blf_keygen *k) {
    k->offset = 0;
    memset(k->key,0,KEYGEN_LEN);
    blf_cbc_encrypt(k->context,k->iv,k->key,KEYGEN_LEN);
    memcpy(k->iv,k->key + KEYGEN_LEN - 8,8);
}

char encrypt_keygen(blf_keygen *k,char c) {
	if (k->offset == KEYGEN_LEN) {
		refill_keygen(k);
	}
	return c ^ k->key[k->offset++];
}
