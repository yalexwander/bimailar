#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "structs.h"

/*
 * Чтение набора заголовков из файла <archiveFile>.headers.
 * Формат файла:
 *   uint16  encoding (0 – UTF‑8)
 *   последовательно:
 *     uint32  header id
 *     uint16  header name length
 *     string  header name data
 *
 * Возвращает указатель на MailarArchiveHeadersSet, содержащий
 * загруженные заголовки, их количество и кодировку.
 */
static uint16_t read_uint16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_uint32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

MailarArchiveHeadersSet* mailar_header_load_names(char* headersFile)
{
    if (!headersFile) {
        return NULL;
    }

    /* Формируем имя файла с расширением .headers */
    size_t len = strlen(headersFile);
    char *header_file = malloc(len + 10); /* ".headers" + NUL */
    if (!header_file) {
        return NULL;
    }
    strcpy(header_file, headersFile);
    strcat(header_file, ".headers");

    /* Открываем файл */
    FILE *f = fopen(header_file, "rb");
    free(header_file);
    if (!f) {
        /* Файл не существует – возвращаем пустой набор */
        return NULL;
    }

    /* Считываем весь файл в буфер */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }

    uint8_t *buf = malloc(sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);

    /* Парсим содержимое */
    size_t offset = 0;
    MailarArchiveHeadersSet *set = calloc(1, sizeof(MailarArchiveHeadersSet));
    if (!set) {
        free(buf);
        return NULL;
    }

    set->encoding = read_uint16(buf + offset);
    offset += 2;
    (void)set->encoding; /* пока игнорируем, но читаем */

    /* Динамический массив указателей на заголовки */
    size_t cap = 8;
    size_t count = 0;
    set->headers = calloc(cap, sizeof(MailarArchiveHeader*));
    if (!set->headers) {
        free(buf);
        free(set);
        return NULL;
    }

    while (offset + 6 <= (size_t)sz) { /* минимум 4+2 байта для id и длины */
        MailarArchiveHeader *h = malloc(sizeof(MailarArchiveHeader));
        if (!h) {
            /* Очистка и выход */
            for (size_t i = 0; i < count; ++i) {
                free(set->headers[i]->name);
                free(set->headers[i]);
            }
            free(set->headers);
            free(buf);
            free(set);
            return NULL;
        }

        h->header_id = read_uint32(buf + offset);
        offset += 4;
        h->name_length = read_uint16(buf + offset);
        offset += 2;

        if (offset + h->name_length > (size_t)sz) {
            /* Неверный формат – очистка */
            free(h);
            for (size_t i = 0; i < count; ++i) {
                free(set->headers[i]->name);
                free(set->headers[i]);
            }
            free(set->headers);
            free(buf);
            free(set);
            return NULL;
        }

        h->name = malloc(h->name_length + 1);
        if (!h->name) {
            free(h);
            for (size_t i = 0; i < count; ++i) {
                free(set->headers[i]->name);
                free(set->headers[i]);
            }
            free(set->headers);
            free(buf);
            free(set);
            return NULL;
        }
        memcpy(h->name, buf + offset, h->name_length);
        h->name[h->name_length] = '\0';
        offset += h->name_length;

        /* Добавляем заголовок в массив */
        if (count >= cap) {
            cap *= 2;
            MailarArchiveHeader **tmp = realloc(set->headers, cap * sizeof(MailarArchiveHeader*));
            if (!tmp) {
                /* Очистка */
                free(h->name);
                free(h);
                for (size_t i = 0; i < count; ++i) {
                    free(set->headers[i]->name);
                    free(set->headers[i]);
                }
                free(set->headers);
                free(buf);
                free(set);
                return NULL;
            }
            set->headers = tmp;
        }
        set->headers[count++] = h;
    }

    set->headers_count = (uint16_t)count;
    free(buf);
    return set;
}

void mailar_header_save_names(MailarArchiveHeadersSet* headers_set, char *archiveFile)
{
    /* Формируем имя файла с расширением .headers */
    size_t len = strlen(archiveFile);
    char *header_file = malloc(len + 10); /* ".headers" + NUL */
    if (!header_file) {
        return;
    }
    strcpy(header_file, archiveFile);
    strcat(header_file, ".headers");

    /* Открываем файл для записи */
    FILE *f = fopen(header_file, "wb");
    free(header_file);
    if (!f) {
        return;
    }

    /* Записываем кодировку */
    fwrite(&headers_set->encoding, 1, 2, f);

    /* Записываем все заголовки */
    for (size_t i = 0; i < headers_set->headers_count; i++) {
        MailarArchiveHeader *h = headers_set->headers[i];
        /* header id */
        fwrite(&h->header_id, 1, 4, f);
        /* name length */
        fwrite(&h->name_length, 1, 2, f);
        /* name data */
        if (h->name_length > 0) {
            fwrite(h->name, 1, h->name_length, f);
        }
    }

    fclose(f);
}


void free_MailarArchiveHeadersSet(MailarArchiveHeadersSet *headers_set) {
    for (size_t i = 0; i < headers_set->headers_count; i++) {
        free(headers_set->headers[i]->name);
        free(headers_set->headers[i]);
    }

    free(headers_set);
}


/*
 * Возвращает имя тега по его id.
 * Если тег не найден – возвращает NULL.
 */
char* mailar_header_get_name_by_id(MailarArchiveHeadersSet* headerset, uint32_t header_id)
{
    if (!headerset)
        return NULL;

    for (uint16_t i = 0; i < headerset->headers_count; ++i) {
        if (headerset->headers[i]->header_id == header_id) {
            return headerset->headers[i]->name;
        }
    }
    return NULL;
}


uint32_t mailar_header_get_id_by_name(MailarArchiveHeadersSet* headerset, char* header_name, uint16_t headername_len)
{
    if (!headerset || !header_name)
        return 0;

    for (uint16_t i = 0; i < headerset->headers_count; ++i) {
        MailarArchiveHeader *t = headerset->headers[i];
        if (t->name_length == headername_len && memcmp(t->name, header_name, headername_len) == 0) {
            return t->header_id;
        }
    }
    return 0;
}

/**
 * Returns next header id for creating new header
 */
uint32_t mailar_headerset_get_new_header_id(MailarArchiveHeadersSet *headerset) {
    if (! headerset || !headerset->headers_count) {
        return 1;
    }

    uint32_t max_id = 0;
    for (uint32_t i = 0; i < headerset->headers_count; i++) {
        if (headerset->headers[i]->header_id > max_id) {
            max_id = headerset->headers[i]->header_id;
        }
    }
    max_id++;

    return max_id;
}

uint32_t mailar_headerset_add_header(MailarArchiveHeadersSet *headers_set, const char* header_name) {

    MailarArchiveHeader *new_header = malloc(sizeof(MailarArchiveHeader));
    new_header->header_id = mailar_headerset_get_new_header_id(headers_set);
    new_header->name_length = (uint16_t)strlen(header_name);
    new_header->name = malloc(new_header->name_length + 1);
    memcpy(new_header->name, header_name, new_header->name_length);
    new_header->name[new_header->name_length] = '\0';


    /* Перераспределяем массив заголовков */
    if (!headers_set->headers_count) {
        headers_set->headers = malloc(sizeof(MailarArchiveHeader*));
    }
    else {
        headers_set->headers = realloc(headers_set->headers, (headers_set->headers_count + 1) * sizeof(MailarArchiveHeader*));
    }
    headers_set->headers[headers_set->headers_count] = new_header;
    headers_set->headers_count++;

    return new_header->header_id;
}

MailarArchiveHeadersSet* mailar_init_headerset() {
    MailarArchiveHeadersSet *set = malloc(sizeof(MailarArchiveHeadersSet));
    set->headers_count = 0;
    return set;
}
