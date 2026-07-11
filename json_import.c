#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "functions.h"
#include "structs.h"
#include "json_exchange.h"

/* -------------------------- */
/*  Base64 декодер (простая реализация)                               */
/* -------------------------- */
static const unsigned char base64_table[256] = {
    [0 ... 255] = 0x80, /* 0x80 означает «невалидный символ» */
    ['A'] = 0,  ['B'] = 1,  ['C'] = 2,  ['D'] = 3,
    ['E'] = 4,  ['F'] = 5,  ['G'] = 6,  ['H'] = 7,
    ['I'] = 8,  ['J'] = 9,  ['K'] = 10, ['L'] = 11,
    ['M'] = 12, ['N'] = 13, ['O'] = 14, ['P'] = 15,
    ['Q'] = 16, ['R'] = 17, ['S'] = 18, ['T'] = 19,
    ['U'] = 20, ['V'] = 21, ['W'] = 22, ['X'] = 23,
    ['Y'] = 24, ['Z'] = 25,
    ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
    ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33,
    ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37,
    ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
    ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45,
    ['u'] = 46, ['v'] = 47, ['w'] = 48, ['x'] = 49,
    ['y'] = 50, ['z'] = 51,
    ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
    ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
    ['8'] = 60, ['9'] = 61,
    ['+'] = 62, ['/'] = 63,
    ['='] = 0
};

static int base64_decode(const char *in, size_t in_len, unsigned char **out, size_t *out_len)
{
    if (!in || !out || !out_len) {
        return -1;
    }

    size_t padding = 0;
    for (size_t i = 0; i < in_len; ++i) {
        if (in[i] == '=') {
            padding++;
        }
    }

    size_t decoded_len = (in_len / 4) * 3 - padding;
    unsigned char *buf = malloc(decoded_len);
    if (!buf) {
        return -1;
    }

    size_t idx = 0;
    for (size_t i = 0; i < in_len; i += 4) {
        unsigned int sextet[4];
        for (size_t j = 0; j < 4; ++j) {
            unsigned char c = in[i + j];
            if (c == '=') {
                sextet[j] = 0;
            } else {
                sextet[j] = base64_table[c];
                if (sextet[j] == 0x80) { /* невалидный символ */
                    free(buf);
                    return -1;
                }
            }
        }

        buf[idx++] = (sextet[0] << 2) | (sextet[1] >> 4);
        if (i + 2 < in_len && in[i + 2] != '=') {
            buf[idx++] = (sextet[1] << 4) | (sextet[2] >> 2);
        }
        if (i + 3 < in_len && in[i + 3] != '=') {
            buf[idx++] = (sextet[2] << 6) | sextet[3];
        }
    }

    *out = buf;
    *out_len = decoded_len;
    return 0;
}

/* -------------------------- */
/*  Основная функция импорта из JSON                                   */
/* -------------------------- */
int mailar_json_import(char* archiveName, char* jsonString)
{
    if (!archiveName || !jsonString) {
        return -1;
    }

    /* ---------- Загружаем существующие имена заголовков ---------- */
    MailarArchiveHeadersSet* headers_set = mailar_header_load_names(archiveName);
    if (! headers_set) {
        headers_set = mailar_init_headerset();
    }

    uint32_t max_header_id = 0;
    int new_headers_added = 0;

    if (headers_set->headers_count) {
        for (int i = 0; i < headers_set->headers_count; i++) {
            max_header_id = headers_set->headers[i]->header_id;
        }
    } else {
        headers_set->headers_count = 0;
        max_header_id = 1;
    }

    /* ---------- Загружаем существующие имена тегов ---------- */
    MailarArchiveTagsSet* tags_set = mailar_tag_load_names(archiveName);
    if (! tags_set) {
        tags_set = mailar_init_tagset();
    }


    uint32_t max_tag_id = 1;
    int new_tags_added = 0;

    if (tags_set->tags_count) {
        for (int i = 0; i < tags_set->tags_count; i++) {
            max_tag_id = tags_set->tags[i]->tag_id;
        }
    } else {
        tags_set->tags_count = 0;
        max_tag_id = 1;
    }

    cJSON *root = cJSON_Parse(jsonString);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);

        free_MailarArchiveHeadersSet(headers_set);
        free_MailarArchiveTagsSet(tags_set);
        return -1;
    }

    int msg_count = cJSON_GetArraySize(root);
    if (msg_count <= 0) {
        cJSON_Delete(root);

        free_MailarArchiveHeadersSet(headers_set);
        free_MailarArchiveTagsSet(tags_set);

        return 0; /* ничего не импортируем */
    }

    /* Массив указателей на сообщения */
    MailarMessage **msgs = calloc(msg_count, sizeof(MailarMessage*));
    if (!msgs) {
        cJSON_Delete(root);

        free_MailarArchiveHeadersSet(headers_set);
        free_MailarArchiveTagsSet(tags_set);

        return -1;
    }

    int i;
    for (i = 0; i < msg_count; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item || !cJSON_IsObject(item)) {
            continue;
        }

        MailarMessage *msg = calloc(1, sizeof(MailarMessage));
        if (!msg) {
            continue;
        }

        cJSON *external_id = cJSON_GetObjectItem(item, "id");
        if (external_id && cJSON_IsNumber(external_id)) {
            msg->id = (uint32_t)external_id->valueint;
        }
        else {
            msg->id = 0;
        }


        /* ---------- Флаги сообщения ---------- */
        cJSON *flags = cJSON_GetObjectItem(item, "flags");
        if (flags && cJSON_IsObject(flags)) {
            msg->flags.deleted          = cJSON_IsTrue(cJSON_GetObjectItem(flags, "deleted"));
            msg->flags.has_tags         = cJSON_IsTrue(cJSON_GetObjectItem(flags, "has_tags"));
            msg->flags.maildir_owner    = cJSON_IsTrue(cJSON_GetObjectItem(flags, "maildir_owner"));
            msg->flags.has_attachments  = cJSON_IsTrue(cJSON_GetObjectItem(flags, "has_attachments"));
            msg->flags.has_parents      = cJSON_IsTrue(cJSON_GetObjectItem(flags, "has_parents"));
            msg->flags.body_compressed  = cJSON_IsTrue(cJSON_GetObjectItem(flags, "body_compressed"));
            msg->flags.non_utf8         = cJSON_IsTrue(cJSON_GetObjectItem(flags, "non_utf-8"));
            msg->flags.encryption       = cJSON_IsTrue(cJSON_GetObjectItem(flags, "is_encrypted"));
            msg->flags.has_timestamp    = cJSON_IsTrue(cJSON_GetObjectItem(flags, "has_timestamp"));
        }

        /* timestamp */
        if (msg->flags.has_timestamp) {
            cJSON *ts = cJSON_GetObjectItem(item, "timestamp");
            if (ts && cJSON_IsNumber(ts)) {
                // TODO: check if it actually return 64 bit
                msg->timestamp = (uint64_t)ts->valueint;
            }
        }

        /* ---------- Родители ---------- */
        cJSON *parents = cJSON_GetObjectItem(item, "parents");
        if (parents && cJSON_IsArray(parents)) {
            msg->parent_count = cJSON_GetArraySize(parents);
            if (msg->parent_count > 0) {
                msg->parents = calloc(msg->parent_count, sizeof(uint32_t));
                for (uint16_t j = 0; j < msg->parent_count; ++j) {
                    cJSON *p = cJSON_GetArrayItem(parents, j);
                    if (p && cJSON_IsNumber(p)) {
                        msg->parents[j] = (uint32_t)p->valueint;
                    }
                }
                msg->flags.has_parents = 1;
            }
            else {
                msg->flags.has_parents = 0;
            }
        }
        

        /* ---------- Кодировка ---------- */
        if (msg->flags.non_utf8) {
            cJSON *enc = cJSON_GetObjectItem(item, "encoding_id");
            if (enc && cJSON_IsNumber(enc)) {
                msg->encoding_id = (uint16_t)enc->valueint;
            }
        }

        /* ---------- Вложения ---------- */
        if (msg->flags.has_attachments) {
            cJSON *atts = cJSON_GetObjectItem(item, "attachements");
            if (atts && cJSON_IsArray(atts)) {
                msg->attachment_count = cJSON_GetArraySize(atts);
                if (msg->attachment_count > 0) {
                    msg->attachments = calloc(msg->attachment_count, sizeof(MailarAttachment));
                }
                for (uint16_t j = 0; j < msg->attachment_count; ++j) {
                    cJSON *att = cJSON_GetArrayItem(atts, j);
                    if (!att || !cJSON_IsObject(att)) {
                        continue;
                    }
                    /* Флаги вложения */
                    cJSON *aflags = cJSON_GetObjectItem(att, "flags");
                    if (aflags && cJSON_IsObject(aflags)) {
                        msg->attachments[j]->attachment_flags.compressed = cJSON_IsTrue(cJSON_GetObjectItem(aflags, "compressed"));
                        msg->attachments[j]->attachment_flags.encrypted  = cJSON_IsTrue(cJSON_GetObjectItem(aflags, "encrypted"));
                        msg->attachments[j]->attachment_flags.external   = cJSON_IsTrue(cJSON_GetObjectItem(aflags, "external"));
                        msg->attachments[j]->attachment_flags.inline_    = cJSON_IsTrue(cJSON_GetObjectItem(aflags, "inline"));
                    }
                    /* MIME‑тип */
                    cJSON *mime = cJSON_GetObjectItem(att, "mime_type");
                    if (mime && cJSON_IsNumber(mime)) {
                        msg->attachments[j]->mime_type_id = (uint16_t)mime->valueint;
                    }
                    /* Имя */
                    cJSON *name = cJSON_GetObjectItem(att, "name");
                    if (name && cJSON_IsString(name)) {
                        msg->attachments[j]->name_length = (uint16_t)strlen(name->valuestring);
                        msg->attachments[j]->name_data = malloc(msg->attachments[j]->name_length);
                        memcpy(msg->attachments[j]->name_data, name->valuestring, msg->attachments[j]->name_length);
                    }
                    /* Тело */
                    cJSON *body = cJSON_GetObjectItem(att, "body_data");
                    if (body && cJSON_IsString(body)) {
                        unsigned char *decoded = NULL;
                        size_t decoded_len = 0;
                        if (base64_decode(body->valuestring, strlen(body->valuestring), &decoded, &decoded_len) == 0) {
                            msg->attachments[j]->body_length = (uint32_t)decoded_len;
                            msg->attachments[j]->body_data = decoded;
                        }
                    }
                }
            }
        }

        /* ---------- Тип шифрования ---------- */
        if (msg->flags.encryption) {
            cJSON *enc_type = cJSON_GetObjectItem(item, "encryption_type");
            if (enc_type && cJSON_IsNumber(enc_type)) {
                msg->encryption_type_id = (uint8_t)enc_type->valueint;
            }
        }

        /* ---------- Заголовки ---------- */
        cJSON *hdrs = cJSON_GetObjectItem(item, "headers");
        if (hdrs && cJSON_IsObject(hdrs)) {
            /* Считаем количество заголовков */
            int hdrs_count = 0;
            cJSON *key = NULL;
            cJSON_ArrayForEach(key, hdrs) {
                hdrs_count++;
            }
            if (hdrs_count > 0) {
                msg->headers_count = (uint8_t)hdrs_count;
                msg->headers = calloc(msg->headers_count, sizeof(MailarMessageHeader));
                int idx = 0;
                cJSON_ArrayForEach(key, hdrs) {
                    const char *key_name = key->string;
                    const char *val = key->valuestring;
                    if (!val) {
                        val = "";
                    }

                    /* Ищем существующий заголовок */
                    uint32_t found_id = mailar_header_get_id_by_name(headers_set, key_name, strlen(key_name));

                    if (!found_id) {
                        found_id = mailar_headerset_add_header(headers_set, key_name);
                        new_headers_added = 1;
                    }

                    MailarMessageHeader* new_header = malloc(sizeof(MailarMessageHeader));
                    new_header->header_id = found_id;
                    new_header->data_length = strlen(val);
                    new_header->data = (char*)val;

                    msg->headers[idx] = new_header;
                    idx++;
                }
            }
        }

        /* ---------- Теги ---------- */
        cJSON *tags_obj = cJSON_GetObjectItem(item, "tags");
        if (tags_obj && cJSON_IsArray(tags_obj)) {
            /* Считаем количество тегов */
            int tags_count = 0;
            cJSON *tkey = NULL;
            cJSON_ArrayForEach(tkey, tags_obj) {
                tags_count++;
            }

            if (tags_count > 0) {
                /* Сначала выделяем массив для тегов сообщения */
                msg->tags = calloc(tags_count, sizeof(uint32_t));
                msg->tags_count = tags_count;
                int idx = 0;

                cJSON_ArrayForEach(tkey, tags_obj) {
                    const char *tag_name = tkey->valuestring;
                    uint32_t found_id = mailar_tag_get_id_by_tagname(tags_set, tag_name, strlen(tag_name));

                    if (!found_id) {
                        /* Добавляем новый тег */
                        found_id = mailar_tagset_add_tag(tags_set, tag_name);
                        new_tags_added = 1;
                    }

                    /* Заполняем тег в сообщении */
                    msg->tags[idx] = found_id;
                    idx++;
                }
            }
        }

        if (!msg->tags_count && msg->flags.has_tags) {
            mailar_debug_print("Warning: unsetting gas_tags flag because no actual tags");
            msg->flags.has_tags = 0;
        }


        /* ---------- Тело сообщения ---------- */
        cJSON *body = cJSON_GetObjectItem(item, "body");
        if (body && cJSON_IsString(body)) {
            msg->body_length = (uint32_t)strlen(body->valuestring);
            msg->body = malloc(msg->body_length);
            memcpy(msg->body, body->valuestring, msg->body_length);
        }

        /* ---------- Тип сжатия тела ---------- */
        if (msg->flags.body_compressed) {
            cJSON *comp = cJSON_GetObjectItem(item, "compression_type");
            if (comp && cJSON_IsNumber(comp)) {
                msg->body_compression_type = (uint8_t)comp->valueint;
            }
        }

        /* ---------- Сохраняем сообщение в массив ---------- */
        msgs[i] = msg;
    }

    /* ---------- Записываем все сообщения в архив ---------- */
    mailar_message_append_to_archive(msgs, msg_count, archiveName);

    /* ---------- Очистка ---------- */
    for (i = 0; i < msg_count; ++i) {
        if (msgs[i]) {
            mailar_message_free(msgs[i]);
        }
    }
    free(msgs);
    cJSON_Delete(root);

    int headers_encoding = 0;

    /* Сохраняем новые заголовки, если они появились */
    if (new_headers_added) {
        mailar_header_save_names(headers_set,archiveName);
    }

    /* Сохраняем новые теги, если они появились */
    if (new_tags_added) {
        mailar_tag_save_names(tags_set, archiveName);
    }

    free_MailarArchiveHeadersSet(headers_set);
    free_MailarArchiveTagsSet(tags_set);

    printf("Added %d messages", msg_count);
    return 0;
}
