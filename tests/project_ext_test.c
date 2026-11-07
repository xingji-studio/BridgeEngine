// Tests for the extended .bep project API: create / save / add / remove /
// set_engine, plus normalization and malformed input handling.
#include "BridgeEngine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                                           \
	do {                                                                                           \
		if (!(cond)) {                                                                             \
			fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);                                \
			failures++;                                                                            \
		} else {                                                                                   \
			printf("ok:   %s\n", msg);                                                             \
		}                                                                                          \
	} while (0)

static int file_exists(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) return 0;
	fclose(file);
	return 1;
}

static void test_create(void)
{
	bapi_project_t null_name   = bapi_project_create(NULL, "engine");
	bapi_project_t null_engine = bapi_project_create("Name", NULL);
	CHECK(null_name == NULL, "create rejects NULL name");
	CHECK(null_engine == NULL, "create rejects NULL engine");
	bapi_project_destroy(null_name);
	bapi_project_destroy(null_engine);

	bapi_project_t project = bapi_project_create("Demo", "G:/BridgeEngine");
	CHECK(project != NULL, "create returns a handle");
	if (!project) return;
	CHECK(strcmp(bapi_project_get_name(project), "Demo") == 0, "created name readable");
	CHECK(strcmp(bapi_project_get_engine_dir(project), "G:/BridgeEngine") == 0,
		  "created engine readable");
	CHECK(bapi_project_get_document_count(project) == 0, "created project has no documents");
	CHECK(bapi_project_get_document_path(project, 0) == NULL, "created project index 0 empty");
	bapi_project_destroy(project);
}

static void test_add_and_normalize(void)
{
	bapi_project_t project = bapi_project_create("AddDemo", "");
	CHECK(project != NULL, "create for add test");
	if (!project) return;

	CHECK(bapi_project_add_document(project, "./ui\\menu.xml") == 0, "add normalizes path");
	CHECK(bapi_project_get_document_count(project) == 1, "one document after add");
	CHECK(strcmp(bapi_project_get_document_path(project, 0), "ui/menu.xml") == 0,
		  "stored path is normalized");

	// Duplicates, including differently spelled ones, are no-ops.
	CHECK(bapi_project_add_document(project, "ui/menu.xml") == 0, "duplicate add is a no-op");
	CHECK(bapi_project_add_document(project, "./ui//menu.xml") == 0,
		  "duplicate add with ./ and // is a no-op");
	CHECK(bapi_project_get_document_count(project) == 1, "duplicate add does not grow list");

	// Invalid paths are rejected and leave the list untouched.
	CHECK(bapi_project_add_document(project, "") == -1, "reject empty path");
	CHECK(bapi_project_add_document(project, "   ") == -1, "reject whitespace-only path");
	CHECK(bapi_project_add_document(project, "/abs/menu.xml") == -1, "reject absolute path");
	CHECK(bapi_project_add_document(project, "\\\\server\\share\\menu.xml") == -1,
		  "reject UNC path");
	CHECK(bapi_project_add_document(project, "C:/abs/menu.xml") == -1, "reject drive-letter path");
	CHECK(bapi_project_add_document(project, "../escape.xml") == -1, "reject leading ..");
	CHECK(bapi_project_add_document(project, "ui/../../escape.xml") == -1,
		  "reject embedded .. escape");
	CHECK(bapi_project_add_document(project, "..") == -1, "reject bare ..");
	CHECK(bapi_project_get_document_count(project) == 1, "rejected adds leave list untouched");

	CHECK(bapi_project_add_document(NULL, "ui/menu.xml") == -1, "add NULL project rejected");
	CHECK(bapi_project_add_document(project, NULL) == -1, "add NULL path rejected");

	bapi_project_destroy(project);
}

static void test_remove(void)
{
	bapi_project_t project = bapi_project_create("RemoveDemo", "");
	CHECK(project != NULL, "create for remove test");
	if (!project) return;

	bapi_project_add_document(project, "ui/menu.xml");
	bapi_project_add_document(project, "ui\\game.xml");
	bapi_project_add_document(project, "ui/settings.xml");
	CHECK(bapi_project_get_document_count(project) == 3, "three documents before remove");

	CHECK(bapi_project_remove_document(project, "./ui/game.xml") == 0,
		  "remove matches normalized path");
	CHECK(bapi_project_get_document_count(project) == 2, "count drops after remove");
	CHECK(strcmp(bapi_project_get_document_path(project, 0), "ui/menu.xml") == 0,
		  "earlier document survives shift");
	CHECK(strcmp(bapi_project_get_document_path(project, 1), "ui/settings.xml") == 0,
		  "later document shifts down");

	CHECK(bapi_project_remove_document(project, "ui/missing.xml") == -1,
		  "remove missing document fails");
	CHECK(bapi_project_get_document_count(project) == 2, "failed remove keeps count");
	CHECK(bapi_project_remove_document(project, NULL) == -1, "remove NULL path rejected");
	CHECK(bapi_project_remove_document(NULL, "ui/menu.xml") == -1, "remove NULL project rejected");

	CHECK(bapi_project_remove_document(project, "ui/menu.xml") == 0, "remove first document");
	CHECK(bapi_project_remove_document(project, "ui/settings.xml") == 0, "remove last document");
	CHECK(bapi_project_get_document_count(project) == 0, "empty after removing all");
	CHECK(bapi_project_get_document_path(project, 0) == NULL, "no documents remain");

	bapi_project_destroy(project);
}

static void test_set_engine(void)
{
	bapi_project_t project = bapi_project_create("EngineDemo", "old/engine");
	CHECK(project != NULL, "create for engine test");
	if (!project) return;

	CHECK(bapi_project_set_engine(project, "new/engine") == 0, "set_engine succeeds");
	CHECK(strcmp(bapi_project_get_engine_dir(project), "new/engine") == 0, "engine replaced");
	CHECK(bapi_project_set_engine(project, "") == 0, "set_engine accepts empty string");
	CHECK(strcmp(bapi_project_get_engine_dir(project), "") == 0, "empty engine stored");
	CHECK(bapi_project_set_engine(project, NULL) == -1, "set_engine NULL path rejected");
	CHECK(bapi_project_set_engine(NULL, "x") == -1, "set_engine NULL project rejected");
	CHECK(bapi_project_get_engine_dir(project) != NULL, "engine still readable after rejection");

	bapi_project_destroy(project);
}

static void test_save_roundtrip(const char *out_dir)
{
	char bep_path[1024];
	char temp_path[1024];
	snprintf(bep_path, sizeof(bep_path), "%s/project_ext_roundtrip.bep", out_dir);
	snprintf(temp_path, sizeof(temp_path), "%s.tmp", bep_path);
	remove(bep_path);
	remove(temp_path);

	bapi_project_t project = bapi_project_create("Roundtrip", "./engine\\dir/");
	CHECK(project != NULL, "create for roundtrip test");
	if (!project) return;
	bapi_project_add_document(project, "ui\\menu.xml");
	bapi_project_add_document(project, "./ui/game.xml");
	bapi_project_add_document(project, "ui/menu.xml"); // duplicate no-op
	CHECK(bapi_project_get_document_count(project) == 2, "roundtrip source has two documents");

	CHECK(bapi_project_save(project, bep_path) == 0, "save succeeds");
	CHECK(file_exists(bep_path), "save created the project file");
	CHECK(!file_exists(temp_path), "save leaves no temporary file behind");
	bapi_project_destroy(project);

	// Canonical header / layout.
	FILE *file = fopen(bep_path, "rb");
	CHECK(file != NULL, "saved file can be reopened");
	if (file) {
		char   buffer[1024];
		size_t n  = fread(buffer, 1, sizeof(buffer) - 1, file);
		buffer[n] = '\0';
		fclose(file);
		CHECK(strncmp(buffer, "# BridgeEngine Project\n", 23) == 0, "canonical header written");
		CHECK(strstr(buffer, "name = Roundtrip\n") != NULL, "name line written");
		CHECK(strstr(buffer, "engine = ./engine\\dir/\n") != NULL, "engine line written");
		CHECK(strstr(buffer, "[documents]\n") != NULL, "documents section written");
		CHECK(strstr(buffer, "ui/menu.xml\n") != NULL, "normalized document written");
		CHECK(strstr(buffer, "ui/game.xml\n") != NULL, "second document written");
	}

	bapi_project_t loaded = bapi_project_load_from_bep(bep_path);
	CHECK(loaded != NULL, "saved file loads back");
	if (loaded) {
		CHECK(strcmp(bapi_project_get_name(loaded), "Roundtrip") == 0, "roundtrip name");
		CHECK(strcmp(bapi_project_get_engine_dir(loaded), "./engine\\dir/") == 0,
			  "roundtrip engine");
		CHECK(bapi_project_get_document_count(loaded) == 2, "roundtrip document count");
		CHECK(strcmp(bapi_project_get_document_path(loaded, 0), "ui/menu.xml") == 0,
			  "roundtrip document 0");
		CHECK(strcmp(bapi_project_get_document_path(loaded, 1), "ui/game.xml") == 0,
			  "roundtrip document 1");
		bapi_project_destroy(loaded);
	}

	// Re-saving a loaded, then mutated project keeps working.
	bapi_project_t mutated = bapi_project_create("Mutated", "");
	if (mutated) {
		bapi_project_add_document(mutated, "only.xml");
		CHECK(bapi_project_save(mutated, bep_path) == 0, "overwrite existing file succeeds");
		bapi_project_destroy(mutated);

		bapi_project_t reloaded = bapi_project_load_from_bep(bep_path);
		CHECK(reloaded != NULL, "overwritten file loads");
		if (reloaded) {
			CHECK(strcmp(bapi_project_get_name(reloaded), "Mutated") == 0, "overwrite name");
			CHECK(strcmp(bapi_project_get_engine_dir(reloaded), "") == 0, "empty engine reloads");
			CHECK(bapi_project_get_document_count(reloaded) == 1, "overwrite document count");
			CHECK(strcmp(bapi_project_get_document_path(reloaded, 0), "only.xml") == 0,
				  "overwrite document path");
			bapi_project_destroy(reloaded);
		}
	}

	// A project with no documents still roundtrips.
	bapi_project_t empty = bapi_project_create("Empty", "");
	if (empty) {
		CHECK(bapi_project_save(empty, bep_path) == 0, "save project without documents");
		bapi_project_destroy(empty);
		bapi_project_t empty_loaded = bapi_project_load_from_bep(bep_path);
		CHECK(empty_loaded != NULL, "empty project file loads");
		if (empty_loaded) {
			CHECK(bapi_project_get_document_count(empty_loaded) == 0, "empty document count");
			bapi_project_destroy(empty_loaded);
		}
	}

	remove(bep_path);
}

static void test_malformed(void)
{
	char missing_path[1024];
	snprintf(missing_path, sizeof(missing_path), "%s/project_ext_missing_%d.bep", ".", 1);
	remove(missing_path);

	CHECK(bapi_project_load_from_bep(NULL) == NULL, "load NULL path rejected");
	CHECK(bapi_project_load_from_bep("") == NULL, "load empty path rejected");
	CHECK(bapi_project_load_from_bep(missing_path) == NULL, "load missing file rejected");

	CHECK(bapi_project_save(NULL, "x.bep") == -1, "save NULL project rejected");
	bapi_project_t project = bapi_project_create("Malformed", "");
	if (project) {
		CHECK(bapi_project_save(project, NULL) == -1, "save NULL path rejected");
		CHECK(bapi_project_save(project, "") == -1, "save empty path rejected");
		CHECK(bapi_project_save(project, "/nonexistent-dir/out.bep") == -1,
			  "save into missing directory fails");
		bapi_project_destroy(project);
	}

	// Getters on NULL handles are safe.
	CHECK(bapi_project_get_document_count(NULL) == 0, "count on NULL is 0");
	CHECK(bapi_project_get_name(NULL) == NULL, "name on NULL is NULL");
	CHECK(bapi_project_get_engine_dir(NULL) == NULL, "engine on NULL is NULL");
	CHECK(bapi_project_get_document_path(NULL, 0) == NULL, "path on NULL is NULL");
	bapi_project_destroy(NULL);
}

int main(int argc, char *argv[])
{
	const char *out_dir = argc > 1 ? argv[1] : ".";

	test_create();
	test_add_and_normalize();
	test_remove();
	test_set_engine();
	test_save_roundtrip(out_dir);
	test_malformed();

	printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
	return failures ? 1 : 0;
}
