#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "structs.h"
#include "functions.h"

/*
 * Выводит подробную информацию о сообщении в читаемом формате.
 */
void mailar_message_dump(MailarMessage *msg, MailarArchive* archive)
{
    if (!msg)
        return;

    printf("ID: %u\n", msg->id);
    printf("LENGTH: %u\n", msg->length);

    printf("FLAGS: ");
    if (msg->flags.deleted) { printf("DELETED "); }
    if (msg->flags.has_tags) { printf("TAGS "); }
    if (msg->flags.maildir_owner) { printf("OWNER "); }
    if (msg->flags.has_attachments) { printf("ATTACHMENTS "); }
    if (msg->flags.has_parents) { printf("PARENTS "); }
    if (msg->flags.body_compressed) { printf("COMPRESSED "); }
    if (msg->flags.non_utf8) { printf("NON_UTF8 "); }
    if (msg->flags.encryption) { printf("ENCRYPTION "); }
    if (msg->flags.has_timestamp) { printf("TIMESTAMP "); }
    printf("\n");

    if (msg->flags.has_timestamp) { 
        time_t time = msg->timestamp;
        struct tm *tm_info = localtime(&time);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("Timestamp: %s\n", buffer);
    }

    printf("HEADERS:\n");
    char *header_name;
    for (uint8_t i = 0; i < msg->headers_count; ++i) {
        MailarMessageHeader *h = msg->headers[i];
        if (archive) {
            header_name = mailar_header_get_name_by_id(archive->headers, h->header_id);
        }
        if (archive && header_name) {
            printf("  %s : %.*s\n", header_name, h->data_length, h->data);
        }
        else {
            printf("  %u : %.*s\n", h->header_id, h->data_length, h->data);
        }
    }

    if (msg->flags.has_parents && msg->parent_count) {
        printf("PARENTS:");
        for (uint16_t i = 0; i < msg->parent_count; ++i) {
            printf("  %u", msg->parents[i]);
        }
        printf("\n");
    }


    if (msg->flags.has_tags && msg->tags_count) {
        printf("TAGS:\n");
        for (uint16_t i = 0; i < msg->tags_count; ++i) {
            printf("  %u\n", msg->tags[i]);
        }
    }


    printf("BODY:\n");
    if (msg->body && msg->body_length > 0)
        printf("  %.*s\n", msg->body_length, msg->body);
    else
        printf("  <empty>\n");

    if (msg->flags.has_attachments && msg->attachment_count) {
        printf("ATTACHEMENTS:\n");
        for (uint16_t i = 0; i < msg->attachment_count; ++i) {
            MailarAttachment *a = msg->attachments[i];
            printf("  %.*s %u\n",
                   a->name_length,
                   a->name_data,
                   a->body_length);
        }
    }
}


void mailar_debug_print(char *s) {
    printf("%s\n", s);
}
