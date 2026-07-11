#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "structs.h"


/*
 * Сериализация структуры MailarMessage в последовательность байт,
 * соответствующую обновлённому формату.
 *
 * Функция выделяет память для буфера, заполняет его, обновляет поле
 * msg->length и возвращает указатель на буфер. Caller обязан
 * освободить память вызовом free().
 */
char* mailar_serialize_message(MailarMessage *msg)
{
    /* ---------- Вычисляем итоговую длину ---------- */
    uint32_t total_len = 0;

    /* 4 байта длины + 2 байта флагов */
    total_len += 4 + 2;

    /* Timestamp */
    if (msg->flags.has_timestamp) {
        total_len += 8;
    }

    /* Parents */
    if (msg->flags.has_parents) {
        total_len += 2; /* count */
        total_len += 4 * msg->parent_count;
    }

    /* Encoding id */
    if (msg->flags.non_utf8) {
        total_len += 2;
    }

    /* Attachments */
    if (msg->flags.has_attachments) {
        total_len += 2; /* count */
        total_len += 1; /* attachment_flags */
        if (msg->flags.body_compressed) {
            total_len += 1; /* body_compression_type */
        }
    }

    /* Encryption type id (present if encryption flag set) */
    if (msg->flags.encryption) {
        total_len += 1;
    }

    /* Headers */
    total_len += 1; /* header count */
    for (uint8_t i = 0; i < msg->headers_count; ++i) {
        // header id
        total_len += sizeof(uint32_t);
        // header data length
        total_len += sizeof(uint16_t);
        // header data
        total_len += msg->headers[i]->data_length;
    }

    /* Body */
    total_len += 4; /* body length field */
    total_len += msg->body_length;

    /* Attachments data */
    for (uint16_t i = 0; i < msg->attachment_count; ++i) {
        const MailarAttachment *a = msg->attachments[i];
        total_len += 2; /* mime type id */
        total_len += 2; /* name length */
        total_len += a->name_length;
        total_len += 4; /* body length */
        total_len += a->body_length;
    }

    /* Message ID (появляется в конце формата) */
    total_len += 4;

    /* ---------- Выделяем буфер ---------- */
    char *buf = malloc(total_len);
    if (!buf) {
        return NULL;
    }

    /* ---------- Заполняем буфер ---------- */
    uint32_t offset = 0;

    /* Длина */
    memcpy(buf + offset, &total_len, 4);
    offset += 4;

    /* Flags */
    uint16_t flags_raw = 0;
    flags_raw |= (msg->flags.deleted          & 0x1) << 0;
    flags_raw |= (msg->flags.has_tags         & 0x1) << 1;
    flags_raw |= (msg->flags.maildir_owner    & 0x1) << 2;
    flags_raw |= (msg->flags.has_attachments  & 0x1) << 3;
    flags_raw |= (msg->flags.has_parents      & 0x1) << 4;
    flags_raw |= (msg->flags.body_compressed  & 0x1) << 5;
    flags_raw |= (msg->flags.non_utf8         & 0x1) << 6;
    flags_raw |= (msg->flags.encryption       & 0x1) << 7;
    flags_raw |= (msg->flags.updated          & 0x1) << 8;
    flags_raw |= (msg->flags.has_timestamp    & 0x1) << 9;

    /* reserved bits remain 0 */
    memcpy(buf + offset, &flags_raw, 2);
    offset += 2;

    /* Timestamp */
    if (msg->flags.has_timestamp) {
        memcpy(buf + offset, &msg->timestamp, 8);
        offset += 8;
    }

    /* Parents */
    if (msg->flags.has_parents) {
        memcpy(buf + offset, &msg->parent_count, 2);
        offset += 2;
        for (uint16_t i = 0; i < msg->parent_count; i++) {
            memcpy(buf + offset, &msg->parents[i], 4);
            offset += 4;
        }
    }

    /* Encoding id */
    if (msg->flags.non_utf8) {
        memcpy(buf + offset, &msg->encoding_id, 2);
        offset += 2;
    }

    /* Attachments */
    if (msg->flags.has_attachments) {
        memcpy(buf + offset, &msg->attachment_count, 2);
        offset += 2;
        /* pack attachment flags into one byte */
        uint8_t att_flags = 0;
        att_flags |= (msg->attachments[0]->attachment_flags.compressed & 0x1) << 0;
        att_flags |= (msg->attachments[0]->attachment_flags.encrypted  & 0x1) << 1;
        att_flags |= (msg->attachments[0]->attachment_flags.external   & 0x1) << 2;
        att_flags |= (msg->attachments[0]->attachment_flags.inline_    & 0x1) << 3;
        /* reserved bits are 0 */
        buf[offset++] = att_flags;
        if (msg->flags.body_compressed) {
            buf[offset++] = msg->body_compression_type;
        }
    }

    /* Encryption type id */
    if (msg->flags.encryption) {
        buf[offset++] = msg->encryption_type_id;
    }

    /* Headers */
    buf[offset++] = msg->headers_count;
    for (uint8_t i = 0; i < msg->headers_count; ++i) {
        memcpy(buf + offset, &msg->headers[i]->header_id, 4);
        offset += 4;

        memcpy(buf + offset, &msg->headers[i]->data_length, 2);
        offset += 2;

        memcpy(buf + offset, msg->headers[i]->data, msg->headers[i]->data_length);
        offset += msg->headers[i]->data_length;
    }

    /* Body */
    memcpy(buf + offset, &msg->body_length, 4);
    offset += 4;
    if (msg->body_length > 0) {
        memcpy(buf + offset, msg->body, msg->body_length);
        offset += msg->body_length;
    }

    /* Attachments data */
    for (uint16_t i = 0; i < msg->attachment_count; ++i) {
        const MailarAttachment *a = msg->attachments[i];
        memcpy(buf + offset, &a->mime_type_id, 2);
        offset += 2;
        memcpy(buf + offset, &a->name_length, 2);
        offset += 2;
        if (a->name_length > 0) {
            memcpy(buf + offset, a->name_data, a->name_length);
            offset += a->name_length;
        }
        memcpy(buf + offset, &a->body_length, 4);
        offset += 4;
        if (a->body_length > 0) {
            memcpy(buf + offset, a->body_data, a->body_length);
            offset += a->body_length;
        }
    }

    /* Message ID (в конце формата) */
    memcpy(buf + offset, &msg->id, 4);
    offset += 4;

    /* ---------- Обновляем поле длины ---------- */
    msg->length = total_len;

    return buf;
}
