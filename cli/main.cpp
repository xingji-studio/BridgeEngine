#include "cli.h"

#include <iostream>
#include <string>
#include <vector>

#ifndef BCLI_VERSION
#define BCLI_VERSION "0.0.0"
#endif

namespace
{

void print_help()
{
	std::cout << "BridgeEngine CLI (bcli) " << BCLI_VERSION << "\n\n";
	std::cout << "Project management for BridgeEngine projects.\n\n";
	std::cout << "Commands:\n";
	std::cout << "  new <name> [--dir DIR] [--engine PATH]   create a project from the template\n";
	std::cout << "  init [--name NAME] [--engine PATH]      initialize the current directory\n";
	std::cout << "  info [--json]                           show project information\n";
	std::cout << "  build [--debug|--release] [--clean] [--reconfigure]\n";
	std::cout << "  run [--build] [--debug|--release] [-- ARGS...]\n";
	std::cout << "  clean                                   remove build/\n";
	std::cout << "  add-doc <relative-path>                 add a UI document\n";
	std::cout << "  remove-doc <relative-path>              remove a UI document\n";
	std::cout << "  list-docs                               list UI documents\n";
	std::cout << "  pack [--assets DIR] [--out FILE] [--atlas DIR] [--bctex bc1|bc3] [--no-rz]\n";
	std::cout << "                                          bake assets, atlases or BC textures\n";
	std::cout
		<< "  ui validate [FILE|DIR]                  validate UI XML and referenced assets\n";
	std::cout << "  watch [--build] [--run] [--interval MS]\n";
	std::cout << "                                          rebuild on file changes\n";
	std::cout << "  set-engine <path>                       update the engine source path\n";
	std::cout << "  doctor                                  check build dependencies\n";
	std::cout << "  self-test                               run built-in tests\n";
	std::cout << "  version                                 print version\n";
	std::cout << "  help                                    show this help\n";
}

void print_version()
{
	std::cout << "bcli " << BCLI_VERSION << "\n";
}

} // namespace

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++) args.emplace_back(argv[i]);

	if (args.empty()) {
		print_help();
		return 0;
	}
	const std::string			   command = args.front();
	const std::vector<std::string> rest(args.begin() + 1, args.end());

	if (command == "help" || command == "--help" || command == "-h") {
		print_help();
		return 0;
	}
	if (command == "version" || command == "--version" || command == "-V") {
		print_version();
		return 0;
	}
	if (command == "new") return bcli::cmd_new(rest);
	if (command == "init") return bcli::cmd_init(rest);
	if (command == "info") return bcli::cmd_info(rest);
	if (command == "build") return bcli::cmd_build(rest);
	if (command == "run") return bcli::cmd_run(rest);
	if (command == "clean") return bcli::cmd_clean(rest);
	if (command == "add-doc") return bcli::cmd_add_document(rest);
	if (command == "remove-doc") return bcli::cmd_remove_document(rest);
	if (command == "list-docs") return bcli::cmd_list_documents(rest);
	if (command == "pack") return bcli::cmd_pack(rest);
	if (command == "ui") return bcli::cmd_ui(rest);
	if (command == "watch") return bcli::cmd_watch(rest);
	if (command == "set-engine") return bcli::cmd_set_engine(rest);
	if (command == "doctor") return bcli::cmd_doctor(rest);
	if (command == "self-test") return bcli::cmd_self_test(rest);

	std::cerr << "bcli: unknown command: " << command << "\n";
	print_help();
	return 1;
}
