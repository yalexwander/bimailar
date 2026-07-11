#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "structs.h"
#include "functions.h"

/*
 * Чтение 32‑битного целого из последовательности байт
 * (предполагается little‑endian).
 */
static inline uint32_t read_uint32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/*
 * Чтение 16‑битного целого из последовательности байт
 * (предполагается little‑endian).
 */
static inline uint16_t read_uint16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*
 * Чтение 40‑битного целого из последовательности байт
 * (предполагается little‑endian).
 */
static inline uint64_t read_uint64t(const uint8_t *p)
{
    return (uint64_t)p[0] |
           ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

/*
 * Чтение 8‑битного целого из последовательности байт
 */
static inline uint8_t read_uint8(const uint8_t *p)
{
    return p[0];
}

/*
 * Парсинг одного сообщения из сырого буфера rawMessage.
 * Функция выделяет память для MailarMessage и всех вложенных
 * структур. Caller обязан освободить результат с помощью
 * mailar_message_free().
 */
MailarMessage* mailar_parse_message(char* rawMessage)
{
    if (!rawMessage) {
        return NULL;
    }

    const uint8_t *p = (const uint8_t*)rawMessage;
    size_t offset = 0;

    /* Allocate message structure */
    MailarMessage *msg = calloc(1, sizeof(MailarMessage));
    if (!msg) {
        return NULL;
    }

    /* Length of the whole entry */
    msg->length = read_uint32(p + offset);
    offset += 4;

    /* Flags */
    uint16_t flags_raw = read_uint16(p + offset);
    offset += 2;
    msg->flags.deleted          = (flags_raw >> 0) & 0x1;
    msg->flags.has_tags         = (flags_raw >> 1) & 0x1;
    msg->flags.maildir_owner    = (flags_raw >> 2) & 0x1;
    msg->flags.has_attachments  = (flags_raw >> 3) & 0x1;
    msg->flags.has_parents      = (flags_raw >> 4) & 0x1;
    msg->flags.body_compressed  = (flags_raw >> 5) & 0x1;
    msg->flags.non_utf8         = (flags_raw >> 6) & 0x1;
    msg->flags.encryption       = (flags_raw >> 7) & 0x1;
    msg->flags.updated          = (flags_raw >> 8) & 0x1;
    msg->flags.has_timestamp    = (flags_raw >> 9) & 0x1;
    /* reserved bits are ignored */

    /* Timestamp */
    if (msg->flags.has_timestamp) {
        msg->timestamp = read_uint64t(p + offset);
        offset += 8;
    }

    /* Parents */
    if (msg->flags.has_parents) {
        msg->parent_count = read_uint16(p + offset);
        offset += 2;
        if (msg->parent_count > 0) {
            msg->parents = calloc(msg->parent_count, sizeof(uint32_t));
            if (!msg->parents) {
                free(msg);
                return NULL;
            }
            for (uint16_t i = 0; i < msg->parent_count; i++) {
                msg->parents[i] = read_uint32(p + offset);
                offset += 4;
            }
        }
    }

    /* Encoding id */
    if (msg->flags.non_utf8) {
        msg->encoding_id = read_uint16(p + offset);
        offset += 2;
    }

    if (msg->flags.has_attachments) {
        msg->attachment_count = read_uint16(p + offset);
        offset += 2;
    }

    /* Encryption type id (present if encryption flag set) */
    if (msg->flags.encryption) {
        msg->encryption_type_id = p[offset];
        offset += 1;
    }

    /* Body compression type (present if body_compressed flag set) */
    if (msg->flags.body_compressed) {
        msg->body_compression_type = p[offset];
        offset += 1;
    }

    /* Headers */
    msg->headers_count = read_uint8(p + offset);
    offset += 1;
    if (msg->headers_count > 0) {
        msg->headers = calloc(msg->headers_count, sizeof(MailarMessageHeader*));
        if (!msg->headers) {
            free(msg);
            return NULL;
        }
        for (uint16_t i = 0; i < msg->headers_count; ++i) {
            MailarMessageHeader *h = calloc(1, sizeof(MailarMessageHeader));
            if (!h) {
                free(msg);
                return NULL;
            }
            h->header_id = read_uint32(p + offset);
            offset += 4;
            h->data_length = read_uint16(p + offset);
            offset += 2;
            if (h->data_length > 0) {
                h->data = malloc(h->data_length + 1);
                if (!h->data) {
                    free(h);
                    free(msg);
                    return NULL;
                }
                memcpy(h->data, p + offset, h->data_length);
                h->data[h->data_length] = '\0';
                offset += h->data_length;
            } else {
                h->data = NULL;
            }
            msg->headers[i] = h;
        }
    }

    msg->tags_count = 0;
    msg->tags = NULL;

    /* Body */
    msg->body_length = read_uint32(p + offset);
    offset += 4;
    if (msg->body_length > 0) {
        msg->body = malloc(msg->body_length);
        if (!msg->body) {
            free(msg);
            return NULL;
        }
        memcpy(msg->body, p + offset, msg->body_length);
        offset += msg->body_length;
    } else {
        msg->body = NULL;
    }

    /* Attachments */
    if (msg->flags.has_attachments) {
        if (msg->attachment_count > 0) {
            msg->attachments = calloc(msg->attachment_count, sizeof(MailarAttachment*));
            if (!msg->attachments) {
                free(msg);
                return NULL;
            }
        }

        /* For each attachment read its flags, mime type, name, body */
        for (uint16_t i = 0; i < msg->attachment_count; ++i) {
            MailarAttachment *a = calloc(1, sizeof(MailarAttachment));
            if (!a) {
                free(msg);
                return NULL;
            }
            /* attachment flags (1 byte) */
            a->attachment_flags.compressed = (p[offset] >> 0) & 0x1;
            a->attachment_flags.encrypted  = (p[offset] >> 1) & 0x1;
            a->attachment_flags.external   = (p[offset] >> 2) & 0x1;
            a->attachment_flags.inline_    = (p[offset] >> 3) & 0x1;
            /* reserved bits ignored */
            offset += 1;

            a->mime_type_id = read_uint16(p + offset);
            offset += 2;

            a->name_length = read_uint16(p + offset);
            offset += 2;
            if (a->name_length > 0) {
                a->name_data = malloc(a->name_length);
                if (!a->name_data) {
                    free(a);
                    free(msg);
                    return NULL;
                }
                memcpy(a->name_data, p + offset, a->name_length);
                offset += a->name_length;
            } else {
                a->name_data = NULL;
            }

            a->body_length = read_uint32(p + offset);
            offset += 4;
            if (a->body_length > 0) {
                a->body_data = malloc(a->body_length);
                if (!a->body_data) {
                    free(a->name_data);
                    free(a);
                    free(msg);
                    return NULL;
                }
                memcpy(a->body_data, p + offset, a->body_length);
                offset += a->body_length;
            } else {
                a->body_data = NULL;
            }

            msg->attachments[i] = a;
        }
    }


    /* Message ID (появляется в конце формата) */
    msg->id = read_uint32(p + offset);
    offset += 4;

    return msg;
}
