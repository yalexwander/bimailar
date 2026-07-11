#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../functions.h"
#include "../json_exchange.h"

/*
 * Простейший утилитный файл для импорта сообщений из JSON
 * в архив Bimailar.
 *
 * Параметры командной строки:
 *   -a <archive>   – имя архива, в который будут добавлены сообщения
 *   -j <json_file> – путь к файлу с JSON‑массивом сообщений
 *
 * Функция читает содержимое JSON‑файла в память, вызывает
 * mailar_json_import(archive, jsonString) и выводит результат.
 */
static void print_usage()
{
    fprintf(stderr,
            "Usage: bimailar_import_json -a <archive_file> -j <json_file>\n"
            "  -a <archive_file>  имя архива Bimailar\n"
            "  -j <json_file>     путь к файлу с JSON‑массивом сообщений\n"
            );
}

int main(int argc, char *argv[])
{
    const char *archive_name = NULL;
    const char *json_file = NULL;
    /* ----- Парсим аргументы ----- */
    int opt;
    while ((opt = getopt(argc, argv, "a:j:")) != -1) {
        switch (opt) {
        case 'a':
            archive_name = optarg;
            break;
        case 'j':
            json_file = optarg;
            break;
        default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (!archive_name || !json_file) {
        print_usage();
        return EXIT_FAILURE;
    }

    /* ----- Читаем JSON‑файл в строку ----- */
    FILE *f = fopen(json_file, "rb");
    if (!f) {

        fprintf(stderr, "Failed to open JSON file '%s': %s\n",
                json_file, strerror(errno));
        return EXIT_FAILURE;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek JSON file '%s'\n", json_file);
        fclose(f);
        return EXIT_FAILURE;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fprintf(stderr, "Failed to tell JSON file '%s'\n", json_file);
        fclose(f);
        return EXIT_FAILURE;
    }
    rewind(f);
    char *json_str = malloc(sz + 1);
    if (!json_str) {
        fprintf(stderr, "Memory allocation failed for JSON string\n");
        fclose(f);
        return EXIT_FAILURE;
    }

    size_t read_bytes = fread(json_str, 1, sz, f);
    fclose(f);
    if (read_bytes != (size_t)sz) {
        fprintf(stderr, "Failed to read entire JSON file '%s'\n", json_file);
        free(json_str);
        return EXIT_FAILURE;
    }
    json_str[sz] = '\0'; /* null‑terminate */
    /* ----- Импортируем сообщения ----- */
    int ret = mailar_json_import((char *)archive_name, json_str);
    free(json_str);

    if (ret != 0) {
        fprintf(stderr, "mailar_json_import failed with code %d\n", ret);
        return EXIT_FAILURE;
    }
    printf("Successfully imported messages into '%s'\n", archive_name);
    return EXIT_SUCCESS;
}
