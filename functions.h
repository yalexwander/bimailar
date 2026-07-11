#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "structs.h"

void mailar_debug_print(char *s);
void mailar_message_dump(MailarMessage *msg, MailarArchive* archive);
void mailar_message_free(MailarMessage *msg);
char* mailar_serialize_message(MailarMessage *msg);
MailarMessage* mailar_parse_message(char* rawMessage);
MailarMessage* mailar_archive_file_read_message(FILE *f);

MailarArchive* mailar_archive_full_load(char *fileName);
void mailar_message_append_to_archive(MailarMessage **msgs, size_t count, const char *archive);

MailarArchiveHeadersSet* mailar_header_load_names(char* archiveFile);
void mailar_header_save_names(MailarArchiveHeadersSet* headers_set, char *archiveFile);

MailarArchiveTagsSet* mailar_tag_load_names(char* archiveFile);

char* mailar_tag_get_tagname_by_id(MailarArchiveTagsSet* tagset, uint32_t tag_id);
uint32_t mailar_tag_get_id_by_tagname(MailarArchiveTagsSet* tagset, const char* tag_name, uint16_t tagname_len);

void free_MailarMessageTag(MailarMessageTag *tag);
void mailar_tag_save_names(MailarArchiveTagsSet* set, char *archive);

char* mailar_header_get_name_by_id(MailarArchiveHeadersSet* headerset, uint32_t header_id);
uint32_t mailar_header_get_id_by_name(MailarArchiveHeadersSet* headerset, const char* header_name, uint16_t headername_len);

void free_MailarArchiveTagsSet(MailarArchiveTagsSet *tags_set);
void free_MailarArchiveHeadersSet(MailarArchiveHeadersSet *headers_set);
void free_MailarAttachment(MailarAttachment* a);

uint32_t mailar_tagset_get_new_tag_id(MailarArchiveTagsSet *tagset);
uint32_t mailar_tagset_add_tag(MailarArchiveTagsSet *tags_set, const char* tag_name);
uint32_t mailar_headerset_get_new_header_id(MailarArchiveHeadersSet *headerset);
uint32_t mailar_headerset_add_header(MailarArchiveHeadersSet *headers_set, const char* header_name);

MailarArchiveTagsSet* mailar_init_tagset();
MailarArchiveHeadersSet* mailar_init_headerset();

uint32_t mailar_archive_get_last_id(const char* archive);

#endif /* FUNCTIONS_H */
