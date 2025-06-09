// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <cstdlib>
#include <iostream>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
// #include <boost/program_options.hpp>
#include <getopt.h>

#include "srdp.h"
#include "utils.h"

// namespace po = boost::program_options;

struct options {
  enum class command_e {
    none = 0,
    init = 1
  };

 command_e cmd = command_e::none;
 std::string dir;
 std::string project;
 std::string experiment;
};


namespace srdp {
  std::string basename;

  void print_help_init(){
    std::cout << "Usage: dp init [options] <project name>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h:   Show help.\n";
    std::cout << "  --store, -s:  Locatation of data store (optional).\n";
    std::cout << "                If not given, store will be created in project directory.\n";
    std::cout << "\n";
    std::cout << "Possitional options:\n";
    std::cout << "  project name:   name of project\n";
  }

  void command_init(int argc, char *argv[], const options& cmdopts){
    const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"store", required_argument, 0, 's'},
      {0, 0, 0, 0}
    };

    std::string store_dir;
    int opt = 0;

    while ((opt = getopt_long(argc, argv, "hs:", long_options, 0)) != -1) {
      switch (opt) {
        case 'h':
          srdp::print_help_init();
          return;
        case 's':
          store_dir = optarg;
          break;
        default:
          throw std::invalid_argument("Unknown option");
      }
    }

    if (argc > optind) {
      std::string target_dir = "./";
      if (!cmdopts.dir.empty()) target_dir = cmdopts.dir;
      Srdp::init(target_dir, store_dir);
      Srdp srdp(target_dir);
      Project prj = srdp.create_project(argv[optind]);
      prj.ctime = get_timestamp_now();
      prj.owner = Srdp::get_user_name();
      prj.update();
      std::cout << "Created project " << prj.name << " (" << uuids::to_string(prj.uuid) << ")\n";
    } else {
      print_help_init();
      std::cout << "\n";
      throw std::invalid_argument("init: no project name given.");
    }
  }

  void print_help_project(){
    std::cout << "Usage: dp project [options] <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h:     Show help.\n";
    std::cout << "  --message, -m:  Message for abstract/edit/append commands (optional).\n";
    std::cout << "                  If not given, $EDITOR will be opended.\n";
    std::cout << "\n";
    std::cout << "Commands:\n";
    std::cout << "  list, l:             List all projects in DB\n";
    std::cout << "  create <name>, l:    Create new project in DB\n";
    std::cout << "  info, i:             Show info about active project\n";
    std::cout << "  set <name|uuid>, s:  Set the current active projectl\n";
    std::cout << "  abstract, m:         Set short description for project\n";
    std::cout << "  show, j:             Print project journal\n";
    std::cout << "  edit, e:             Edit project journal\n";
    std::cout << "  append, a:           Append project journal\n";
    std::cout << "  remove, r:           Remove project\n";
    std::cout << "  assets, b:           List all project assets\n";
  }

  void command_project(int argc, char *argv[], const options& cmdopts){
    const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"message", required_argument, 0, 'm'},
      {"all", required_argument, 0, 'a'},
      {0, 0, 0, 0}
    };

    std::string message;
    int opt = 0;

    bool print_all = false;
    while ((opt = getopt_long(argc, argv, "ham:", long_options, 0)) != -1) {
      switch (opt) {
        case 'h':
          srdp::print_help_project();
          return;
        case 'm':
          message = optarg;
          break;
        case 'a':
          print_all = true;
          break;
        default:
          throw std::invalid_argument("Unknown option");
      }
    }

    if (argc > optind) {
      const std::string cmd(argv[optind]);

      std::string target_dir = "./";
      if (!cmdopts.dir.empty()) target_dir = cmdopts.dir;
      Srdp srdp(target_dir, true);

      if (cmd == "list" || cmd == "l") { // List all projects
        std::vector<Project> plist = srdp.get_project().list();

        for (auto prj : plist){
          print_project(prj);
        }

      } else if (cmd == "create" || cmd == "l") { // Create new project
        if (argc <= optind+1)
          throw std::runtime_error("No name given");

        optind++;
        auto prj = srdp.create_project(argv[optind]);
        prj.ctime = get_timestamp_now();
        prj.owner = Srdp::get_user_name();

        if (!message.empty())
          prj.metadata = message;

        prj.update();
        srdp.config.set_project(prj.uuid);

        std::cout << "Created new project " << prj.name << " (" << uuids::to_string(prj.uuid) << ")\n";
      } else if (cmd == "info" || cmd == "i") { // Show current project
          print_project(srdp.open_project(cmdopts.project));
                                  //
      } else if (cmd == "abstract" || cmd == "m") { // Edit abstract/metadata
        Project prj = srdp.open_project(cmdopts.project);

        if (message.empty()){
          if ( prj.metadata ) message = *prj.metadata;
          srdp.edit_text(message);
        }

        prj.metadata = message;
        prj.update();

      } else if (cmd == "show" || cmd == "j"){ // Show the journal
        Project prj = srdp.open_project(cmdopts.project);
        std::cout << "project: " << prj.name << "\n";
        std::cout << prj.get_journal() << "\n";

        if (print_all){
          for (auto e : srdp.get_experiment().list()) {
            std::cout << "experiment: " << e.name << "\n";
            std::cout << e.get_journal() << "\n";
          }
        }

      } else if (cmd == "edit" || cmd == "e"){ // Edit the journal
        Project prj = srdp.open_project(cmdopts.project);

        if (message.empty()){
          if ( prj.metadata ) message = prj.get_journal();
          srdp.edit_text(message);
        }

        prj.set_journal(message);

      } else if (cmd == "append" || cmd == "a"){ // Add journal entry
        Project prj = srdp.open_project(cmdopts.project);

        std::string composed_text("### " + Srdp::get_time_stamp_fmt() + "\n\n" + message);
        if (message.empty()){
          srdp.edit_text(composed_text);
        }

        prj.append_journal("\n" + composed_text);

      } else if (cmd == "set" || cmd == "s"){ // set active project
        if (argc <= optind+1)
          throw std::runtime_error("No name/uuid given");

        optind++;
        std::string project_id(argv[optind]);

        Project prj = srdp.open_project(argv[optind]);
        srdp.config.set_project(prj.uuid);

        std::cout << "Changed active project to " << prj.name << " (" << uuids::to_string(prj.uuid) << ")\n";

      } else if (cmd == "remove" || cmd == "r"){ // remove project
        srdp.remove_project(cmdopts.project);

      } else if (cmd == "assets" || cmd == "b"){
        Project prj = srdp.open_project(cmdopts.project);
        Experiment exp = srdp.get_experiment(cmdopts.project);

        std::cout << "Project:\n";
        print_project(prj);

        for (auto e : exp.list()){
          std::cout << "=> Experiment:\n";
          print_experiment(e);

          for (auto f: srdp.get_file(cmdopts.project, cmdopts.experiment).list()){
          std::cout << "=> File:\n";
            print_file_info(f);
          }
        }

      } else {
        srdp::print_help_project();
        std::cout << "\n";
        throw std::invalid_argument("Invalid command specified");
      }
    } else {
      srdp::print_help_project();
      std::cout << "\n";
      throw std::invalid_argument("No command specified");
    }
  }

  void print_help_experiment(){
    std::cout << "Usage: dp experiment [options] <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h:     Show help.\n";
    std::cout << "  --message, -m:  Message for abstract/edit/append commands (optional).\n";
    std::cout << "                  If not given, $EDITOR will be opended.\n";
    std::cout << "\n";
    std::cout << "Commands:\n";
    std::cout << "  list, l :             List all experiments in active project\n";
    std::cout << "  create <name>, c:    Create a new experiment in active project\n";
    std::cout << "  info, i:             Show info about active experiment\n";
    std::cout << "  set <name|uuid>, s:  Set the current active experiment\n";
    std::cout << "  abstract, m:         Set short description for experiment\n";
    std::cout << "  show, j:             Print experiment journal\n";
    std::cout << "  edit, e:             Edit experiment journal\n";
    std::cout << "  append, a:           Append experiment journal\n";
    std::cout << "  remove, r:           Remove experiment\n";
  }

  void command_experiment(int argc, char *argv[], const options& cmdopts){
    const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"message", required_argument, 0, 'm'},
      {0, 0, 0, 0}
    };

    std::string message;
    int opt = 0;

    while ((opt = getopt_long(argc, argv, "hm:", long_options, 0)) != -1) {
      switch (opt) {
        case 'h':
          srdp::print_help_experiment();
          return;
        case 'm':
          message = optarg;
          break;
        default:
          throw std::invalid_argument("Unknown option");
      }
    }

    if (argc > optind) {
      const std::string cmd(argv[optind]);

      std::string target_dir = "./";
      if (!cmdopts.dir.empty()) target_dir = cmdopts.dir;
      Srdp srdp(target_dir, true);

      if (cmd == "list" || cmd == "l") { // List all experiments in projects
        auto elist = srdp.get_experiment(cmdopts.project).list();
        auto prj = srdp.open_project(cmdopts.project);

        std::cout << "project: "  << prj.name << " (" << uuids::to_string(prj.uuid) << ")" << "\n\n";

        for (auto exp : elist){
          print_experiment(exp);
        }

      } else if (cmd == "create" || cmd == "c") { // Create new experiment
        if (argc <= optind+1)
          throw std::runtime_error("No name given");

        optind++;
        auto exp = srdp.create_experiment(argv[optind], cmdopts.project);
        exp.ctime = get_timestamp_now();
        exp.owner = Srdp::get_user_name();

        if (!message.empty())
          exp.metadata = message;

        exp.update();
        srdp.config.set_experiment(exp.uuid);

        std::cout << "Created new experiment " << exp.name << " (" << uuids::to_string(exp.uuid) << ")\n";

      } else if (cmd == "info" || cmd == "i") { // Show current project
        auto prj = srdp.open_project(cmdopts.project);
        std::cout << "project: "  << prj.name << " (" << uuids::to_string(prj.uuid) << ")" << "\n\n";
        print_experiment(srdp.open_experiment(cmdopts.project, cmdopts.experiment));

      } else if (cmd == "remove" || cmd == "r") { // remove experiment
        srdp.remove_experiment(cmdopts.experiment, cmdopts.project);

      } else if (cmd == "abstract" || cmd == "m") { // Edit abstract/metadata
        Experiment exp = srdp.open_experiment(cmdopts.project, cmdopts.experiment);

        if (message.empty()){
          if ( exp.metadata ) message = *exp.metadata;
          srdp.edit_text(message);
        }

        exp.metadata = message;
        exp.update();

      } else if (cmd == "show" || cmd == "j"){ // Show the journal
        Experiment exp = srdp.open_experiment(cmdopts.project, cmdopts.experiment);
        std::cout << exp.get_journal() << "\n";

      } else if (cmd == "edit" || cmd == "e"){ // Edit the journal
        Experiment exp = srdp.open_experiment(cmdopts.project, cmdopts.experiment);

        if (message.empty()){
          if ( exp.metadata ) message = exp.get_journal();
          srdp.edit_text(message);
        }

        exp.set_journal(message);

      } else if (cmd == "append" || cmd == "a"){ // Add journal entry
        Experiment exp = srdp.open_experiment(cmdopts.project, cmdopts.experiment);

        std::string composed_text("### " + Srdp::get_time_stamp_fmt() + "\n\n" + message);
        if (message.empty()){
          srdp.edit_text(composed_text);
        }

        exp.append_journal("\n" + composed_text);

      } else if (cmd == "set" || cmd == "s"){ // set active experiment
        if (argc <= optind+1)
          throw std::runtime_error("No name/uuid given");

        optind++;
        std::string experiment_id(argv[optind]);

        auto exp = srdp.open_experiment(argv[optind], cmdopts.project);

        srdp.config.set_experiment(exp.uuid);

        std::cout << "Changed active experiment to " << exp.name << " (" << uuids::to_string(exp.uuid) << ")\n";
      } else {
        srdp::print_help_experiment();
        std::cout << "\n";
        throw std::invalid_argument("Invalid command specified");
      }
    } else {
      srdp::print_help_experiment();
      std::cout << "\n";
      throw std::invalid_argument("No command specified");
    }
  }

  void print_help_file(){
    std::cout << "Usage: dp experiment [options] <command>\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h:     Show help.\n";
    std::cout << "  --message, -m:  Message for abstract/edit/append commands (optional).\n";
    std::cout << "                  If not given, $EDITOR will be opended.\n";
    std::cout << "\n";
    std::cout << "Commands:\n";
    std::cout << "  list, l:               List all files in active experiment\n";
    std::cout << "  add <role> <path>, a:  add file to experiment\n";
    std::cout << "  info <path|hash>, i:   show info about file\n";
    std::cout << "  unlink <path|hash>, u: detach file from experiment\n";
    std::cout << "  track <path|hash>, t:  Track a file's heritage\n";
  }

  void command_file(int argc, char *argv[], const options& cmdopts){
    const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}
    };

    std::string message;
    int opt = 0;

    while ((opt = getopt_long(argc, argv, "hm:", long_options, 0)) != -1) {
      switch (opt) {
        case 'h':
          srdp::print_help_file();
          return;
        default:
          throw std::invalid_argument("Unknown option");
      }
    }

    if (argc > optind) {
      const std::string cmd(argv[optind]);

      std::string target_dir = "./";
      if (!cmdopts.dir.empty()) target_dir = cmdopts.dir;
      Srdp srdp(target_dir, true);

      if (cmd == "list" || cmd == "l") { // List all files in experiment
        auto flist = srdp.get_file(cmdopts.project, cmdopts.experiment).list();

        for (auto f : flist){
          print_file_info(f);
        }

      } else if (cmd == "add" || cmd == "a") { // add file to experiment
        if (argc <= optind+2)
          throw std::runtime_error("No role/path given");

        optind++;
        File::role_t role = File::string_to_role(argv[optind]);

        if (role == File::role_t::none)
          throw std::runtime_error("Invalid role");

        optind++;
        fs::path path = argv[optind];

        File file = srdp.add_file(cmdopts.project, cmdopts.experiment, path, role);

        std::cout << "Added " << File::role_to_string(*file.role) << " " << path << " (" << scas::Hash::convert_hash_to_string(file.hash) <<")\n";

      } else if (cmd == "unlink" || cmd == "u") { // remove file entry
        if (argc <= optind+1)
          throw std::runtime_error("No path/hash given");

        // id either by path or hash
        optind++;
        std::string file_id(argv[optind]);

        auto file = srdp.load_file(cmdopts.project, cmdopts.experiment, file_id);

        std::cout << "Remove " << File::role_to_string(*file.role)
          << " " << *file.path
          << " (" << scas::Hash::convert_hash_to_string(file.hash) <<")\n";

        srdp.unlink_file(cmdopts.project, cmdopts.experiment, file_id);

      } else if (cmd == "info" || cmd == "i") { // print info on file
        if (argc <= optind+1)
          throw std::runtime_error("No path/hash given");

        // id either by path or hash
        optind++;
        std::string file_id(argv[optind]);

        auto file = srdp.load_file(cmdopts.project, cmdopts.experiment, file_id);

        print_file_info(file);

      } else if (cmd == "track" || cmd == "t") { // Track files
        if (argc <= optind+1)
          throw std::runtime_error("No path/hash given");

        // id either by path or hash
        optind++;
        std::string file_id(argv[optind]);

        auto file = srdp.load_file(cmdopts.project, cmdopts.experiment, file_id);
        auto tree = file.track();

        auto indent = [](int depth) -> std::string {
          std::string i;
          i.resize(depth, ' ');
          return i;
        };

        std::function<void(srdp::FileTree&,int)> print_tree = [&](srdp::FileTree& tree, int depth=0) {
          auto t = indent(depth);

          std::cout << t << (tree.node.path ? *tree.node.path : "")
            << " " << (tree.node.role ? File::role_to_string(*tree.node.role) : "")
            << "  <- " << tree.node.resolve_creator() << "\n";

          for (auto ch : tree.children)
            print_tree(ch, depth + 1);
        };

        print_tree(tree, 0);

      } else {
        srdp::print_help_file();
        throw std::invalid_argument("Invalid command specified");
      }
    } else {
      srdp::print_help_file();
      std::cout << "\n";
      throw std::invalid_argument("No command specified");
    }

  }

  void print_help(){
    std::cout << srdp::name + ": the simple resarch data pipeline tool\n\n";
    std::cout << "Usage: " + basename + " [options] <sub command>\n\n";
    std::cout << "Global options:\n";
    std::cout << "  --help, -h:        Show help.\n";
    std::cout << "  --dir, -d:         Base directory. This option is only need if dp is\n";
    std::cout << "                     executued from outside the project directory.\n";
    std::cout << "  --project, -d:     Select project project by name.\n";
    std::cout << "  --experiment, -e:  Select project experiment by name.\n";
    std::cout << "  --version, -v:     Show program version.\n";
    std::cout << "\n";
    std::cout << "Possible sub commands:\n";
    std::cout << "  init             Initialize project directory.\n";
    std::cout << "  project, p       Manage project settings.\n";
    std::cout << "  experiment, e    Manage experiment settings.\n";
    std::cout << "  file, f          Manage file handling.\n";
    std::cout << "  verify, v        Verfify store and database.\n";
  }

  void command_verify(int argc, char *argv[], const options& cmdopts){
    std::string target_dir = "./";
    if (!cmdopts.dir.empty()) target_dir = cmdopts.dir;
    Srdp srdp(target_dir, true);

    srdp.verify();
  }
}

/**
 * Main
 */
int main(int argc, char* argv[]){
  try {

    const struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"dir", required_argument, 0, 'd'},
      {"project", required_argument, 0, 'p'},
      {"experiment", required_argument, 0, 'e'},
      {"version", no_argument, 0, 'v'},
      {0, 0, 0, 0}
    };

    srdp::basename = fs::path(argv[0]).filename();

    options command_opts;
    int opt=0;
    while ((opt = getopt_long(argc, argv, "+hd:p:e:v", long_options, 0)) != -1) {
      switch (opt) {
        case 'h':
          srdp::print_help();
          return EXIT_SUCCESS;
        case 'd':
          command_opts.dir = optarg;
          break;
        case 'p':
          command_opts.project = optarg;
          break;
        case 'e':
          command_opts.experiment = optarg;
          break;
        case 'v':
          std::cout << srdp::name + " " + srdp::version << "\n";
          return EXIT_SUCCESS;
        default:
          throw std::invalid_argument("Unknown option");
      }
    }

    if (argc > optind) {
      const int new_argc = argc-optind;
      const std::string cmd(argv[optind]);
      char **new_argv = argv + optind;
      optind = 0;

      if (cmd == "init"){
        srdp::command_init(new_argc, new_argv, command_opts);
      } else if (cmd == "p" || cmd == "project") {
        srdp::command_project(new_argc, new_argv, command_opts);
      } else if (cmd == "e" || cmd == "experiment") {
        srdp::command_experiment(new_argc, new_argv, command_opts);
      } else if (cmd == "f" || cmd == "file") {
        srdp::command_file(new_argc, new_argv, command_opts);
      } else if (cmd == "v" || cmd == "verify") {
        srdp::command_verify(new_argc, new_argv, command_opts);
      } else
        throw std::invalid_argument("Unknown command");
    } else {
      srdp::print_help();
      std::cout << "\n";
      throw std::invalid_argument("No command given");
    }
  } catch (std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }


  return EXIT_SUCCESS;
}
