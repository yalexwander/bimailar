build_tests:
	gcc debug.c functions.c parser.c tests/dumper_archive.c -o tests/dumper_archive

build_utils:
	gcc --debug vendor/cJSON/cJSON.c debug.c functions.c serializer.c headers.c tags.c json_import.c parser.c archive.c utils/bimailar_import_json.c -o utils/bimailar_import_json
	gcc --debug debug.c functions.c parser.c headers.c tags.c serializer.c archive.c utils/mailar_msg_get.c -o utils/mailar_msg_get
