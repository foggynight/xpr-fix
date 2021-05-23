// Copyright (C) 2021 Robert Coffey
// All rights reserved

#include <stdio.h>

#define RTB_DEFINE
#include "rtb_buffer.h"

#include "lexer.h"
#include "token.h"

int main(void)
{
	char expr[] = "1 + -1";

	rtb_buffer_t *buffer = expr_to_tokens(expr);

	for (size_t i = 0; i < buffer->len; ++i) {
		if (i) printf("\n");
		token_t *token = (token_t *)rtb_buffer_at(buffer, i);
		printf("STRING: %s\nFLAGS: %x\n", token->str, token->flags);
	}

	return 0;
}