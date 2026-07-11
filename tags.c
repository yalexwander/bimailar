#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "structs.h"
#include "functions.h"

static uint16_t read_uint16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_uint32(const uint8_t *p)
{
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/*
 * Загрузка списка тегов из файла архива.
 *
 * Формат файла:
 *   2 байта – кодировка (uint16_t)
 *   2 байта – количество тегов (uint16_t)
 *   Для каждого тега:
 *     4 байта – id тега (uint32_t)
 *     2 байта – длина имени (uint16_t)
 *     N байт  – имя (не-нулевой терминатор)
 */
MailarArchiveTagsSet* mailar_tag_load_names(char* archiveFile)
{
    FILE *f = fopen(archiveFile, "rb");
    if (!f)
        return NULL;

    uint8_t buf[4];
    /* читаем кодировку */
    if (fread(buf, 1, 2, f) != 2) {
        fclose(f);
        return NULL;
    }
    uint16_t encoding = read_uint16(buf);

    /* читаем количество тегов */
    if (fread(buf, 1, 2, f) != 2) {
        fclose(f);
        return NULL;
    }
    uint16_t tags_count = read_uint16(buf);

    MailarArchiveTagsSet *set = malloc(sizeof(MailarArchiveTagsSet));
    if (!set) {
        fclose(f);
        return NULL;
    }
    set->encoding = encoding;
    set->tags_count = tags_count;
    set->tags = malloc(tags_count * sizeof(MailarMessageTag*));
    if (!set->tags) {
        free(set);
        fclose(f);
        return NULL;
    }

    for (uint16_t i = 0; i < tags_count; ++i) {
        /* читаем id тега */
        if (fread(buf, 1, 4, f) != 4) {
            /* в случае ошибки освобождаем уже прочитанные данные */
            for (uint16_t j = 0; j < i; ++j) {
                free(set->tags[j]->name);
                free(set->tags[j]);
            }
            free(set->tags);
            free(set);
            fclose(f);
            return NULL;
        }
        uint32_t tag_id = read_uint32(buf);

        /* читаем длину имени */
        if (fread(buf, 1, 2, f) != 2) {
            for (uint16_t j = 0; j < i; ++j) {
                free(set->tags[j]->name);
                free(set->tags[j]);
            }
            free(set->tags);
            free(set);
            fclose(f);
            return NULL;
        }
        uint16_t name_length = read_uint16(buf);

        /* читаем имя */
        MailarMessageTag *tag = malloc(sizeof(MailarMessageTag));
        if (!tag) {
            for (uint16_t j = 0; j < i; ++j) {
                free(set->tags[j]->name);
                free(set->tags[j]);
            }
            free(set->tags);
            free(set);
            fclose(f);
            return NULL;
        }
        tag->tag_id = tag_id;
        tag->name_length = name_length;
        tag->name = malloc(name_length + 1);
        if (!tag->name) {
            free(tag);
            for (uint16_t j = 0; j < i; ++j) {
                free(set->tags[j]->name);
                free(set->tags[j]);
            }
            free(set->tags);
            free(set);
            fclose(f);
            return NULL;
        }
        if (fread(tag->name, 1, name_length, f) != name_length) {
            free(tag->name);
            free(tag);
            for (uint16_t j = 0; j < i; ++j) {
                free(set->tags[j]->name);
                free(set->tags[j]);
            }
            free(set->tags);
            free(set);
            fclose(f);
            return NULL;
        }
        tag->name[name_length] = '\0';
        set->tags[i] = tag;
    }

    fclose(f);
    return set;
}

/*
 * Сохраняет массив тегов в файл <archiveFile>.tagnames.
 * Формат записи:
 *   uint16  encoding, 0 – UTF‑8
 *   последовательность записей тегов:
 *     uint32 – tag id
 *     uint16 – tag name length
 *     string – tag name data
 *
 * Функция принимает указатель на MailarArchiveTagsSet и имя архива.
 */
void mailar_tag_save_names(MailarArchiveTagsSet* set, char *archive)
{
    /* Формируем имя файла с расширением .tagnames */
    size_t len = strlen(archive);
    char *tag_file = malloc(len + 10); /* ".tagnames" + NUL */
    if (!tag_file)
        return;
    strcpy(tag_file, archive);
    strcat(tag_file, ".tagnames");

    FILE *f = fopen(tag_file, "wb");
    free(tag_file);
    if (!f)
        return;

    /* Записываем кодировку */
    fwrite(&set->encoding, 1, 2, f);

    /* Записываем все теги */
    for (size_t i = 0; i < set->tags_count; ++i) {
        MailarMessageTag *t = set->tags[i];
        /* tag id */
        fwrite(&t->tag_id, 1, 4, f);
        /* name length */
        fwrite(&t->name_length, 1, 2, f);
        /* name data */
        if (t->name_length > 0)
            fwrite(t->name, 1, t->name_length, f);
    }

    fclose(f);
}

void free_MailarArchiveTagsSet(MailarArchiveTagsSet *tags_set) {
    for (size_t i = 0; i < tags_set->tags_count; i++) {
        free(tags_set->tags[i]->name);
        free(tags_set->tags[i]);
    }

    free(tags_set);
}

/*
 * Возвращает имя тега по его id.
 * Если тег не найден – возвращает NULL.
 */
char* mailar_tag_get_tagname_by_id(MailarArchiveTagsSet* tagset, uint32_t tag_id)
{
    if (!tagset)
        return NULL;

    for (uint16_t i = 0; i < tagset->tags_count; ++i) {
        if (tagset->tags[i]->tag_id == tag_id) {
            return tagset->tags[i]->name;
        }
    }
    return NULL;
}

/*
 * Возвращает id тега по его имени.
 * Если тег не найден – возвращает 0.
 */
uint32_t mailar_tag_get_id_by_tagname(MailarArchiveTagsSet* tagset, const char* tag_name, uint16_t tagname_len)
{
    if (!tagset || !tag_name)
        return 0;

    for (uint16_t i = 0; i < tagset->tags_count; ++i) {
        MailarMessageTag *t = tagset->tags[i];
        if (t->name_length == tagname_len && memcmp(t->name, tag_name, tagname_len) == 0) {
            return t->tag_id;
        }
    }
    return 0;
}

/**
 * Returns next tag id for creating new tag
 */
uint32_t mailar_tagset_get_new_tag_id(MailarArchiveTagsSet *tagset) {
    if (! tagset || !tagset->tags_count) {
        return 1;
    }

    uint32_t max_id = 0;
    for (uint32_t i = 0; i < tagset->tags_count; i++) {
        if (tagset->tags[i]->tag_id > max_id) {
            max_id = tagset->tags[i]->tag_id;
        }
    }
    max_id++;

    return max_id;
}

uint32_t mailar_tagset_add_tag(MailarArchiveTagsSet *tags_set, const char* tag_name) {

    MailarMessageTag *new_tag = malloc(sizeof(MailarMessageTag));
    new_tag->tag_id = mailar_tagset_get_new_tag_id(tags_set);
    new_tag->name_length = (uint16_t)strlen(tag_name);
    new_tag->name = malloc(new_tag->name_length + 1);
    memcpy(new_tag->name, tag_name, new_tag->name_length);
    new_tag->name[new_tag->name_length] = '\0';


    /* Перераспределяем массив тегов */
    if (!tags_set->tags_count) {
        tags_set->tags = malloc(sizeof(MailarMessageTag*));
    } else {
        tags_set->tags = realloc(tags_set->tags, (tags_set->tags_count + 1) * sizeof(MailarMessageTag*));
    }
    tags_set->tags[tags_set->tags_count] = new_tag;
    tags_set->tags_count++;

    return new_tag->tag_id;
}


MailarArchiveTagsSet* mailar_init_tagset() {
    MailarArchiveTagsSet *set = calloc(1, sizeof(MailarArchiveTagsSet));
    set->tags_count = 0;
    return set;
}
