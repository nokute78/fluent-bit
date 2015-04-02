/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdlib.h>
#include <string.h>

#include <msgpack.h>
#include <jsmn/jsmn.h>
#include <fluent-bit/flb_error.h>
#include <fluent-bit/flb_utils.h>

static jsmntok_t *json_tokenise(char *js, size_t len, int *arr_size)
{
    int ret;
    unsigned int n;
    jsmntok_t *tokens;
    jsmn_parser parser;

    jsmn_init(&parser);
    tokens = malloc(sizeof(jsmntok_t) * n);
    ret = jsmn_parse(&parser, js, len, tokens, n);

    while (ret == JSMN_ERROR_NOMEM) {
        n = n * 2 + 1;
        tokens = realloc(tokens, sizeof(jsmntok_t) * n);
        ret = jsmn_parse(&parser, js, len, tokens, n);
    }

    if (ret == JSMN_ERROR_INVAL) {
        printf("1\n");
        flb_utils_error(FLB_ERR_JSON_INVAL);
        goto error;
    }

    if (ret == JSMN_ERROR_PART) {
        printf("2\n");
        flb_utils_error(FLB_ERR_JSON_PART);
        goto error;
    }

    /* Store the array length */
    *arr_size = n;
    return tokens;

 error:
    free(tokens);
    return NULL;
}

/* It parse a JSON string and convert it to MessagePack format */
char *flb_pack_json(char *tag, char *js, size_t len, int *size)
{
    int i;
    int flen;
    int arr_size;
    int tag_len;
    int expect;
    char *p;
    char *buf;
    jsmntok_t *t;
    jsmntok_t *tokens;
    msgpack_packer pck;
    msgpack_sbuffer sbuf;

    if (!tag || !js) {
        printf("1\n");
        return NULL;
    }

    tokens = json_tokenise(js, len, &arr_size);
    if (!tokens) {
        printf("2\n");
        return NULL;
    }

    tag_len = strlen(tag);

    printf("JSON=\n%s\n", js);

    /* initialize buffers */
    msgpack_sbuffer_init(&sbuf);
    msgpack_packer_init(&pck, &sbuf, msgpack_sbuffer_write);

    for (i = 0; i < arr_size ; i++) {
        t = &tokens[i];

        printf("i=%i start=%i end=%i\n", i, t->start, t->end);
        if (t->start == -1 || t->end == -1 || (t->start == 0 && t->end == 0)) {
            break;
        }
        flen = (t->end - t->start);

        switch (t->type) {
        case JSMN_OBJECT:
            /* Each object must start with a string */
            expect = JSMN_STRING;
            msgpack_pack_map(&pck, t->size);
            break;
        case JSMN_ARRAY:
            msgpack_pack_array(&pck, t->size);
            printf("i=%i is ARRAY\n", i);
            break;
        case JSMN_STRING:
            msgpack_pack_raw(&pck, flen);
            msgpack_pack_raw_body(&pck, js + t->start, flen);
            if (expect == JSMN_STRING) {
                /* String is only mandatory */
            }
            printf("i=%i is STRING\n", i);
            break;
        case JSMN_PRIMITIVE:
            p = js + t->start;
            if (strncmp(p, "false", 5) == 0) {
                msgpack_pack_false(&pck);
            }
            else if (strncmp(p, "true", 4) == 0) {
                msgpack_pack_true(&pck);
            }
            else if (strncmp(p, "null", 4) == 0) {
                msgpack_pack_nil(&pck);
            }
            printf("i=%i is PRIMITIVE\n", i);
            break;
        }
    }

    /* dump data back to a new buffer */
    *size = sbuf.size;
    buf = malloc(sbuf.size);
    memcpy(buf, sbuf.data, sbuf.size);
    msgpack_sbuffer_destroy(&sbuf);

    free(tokens);
    return buf;
}
