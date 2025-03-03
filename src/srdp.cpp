// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <boost/uuid/uuid_io.hpp>
#include "srdp.h"


namespace srdp {

  const fs::path Srdp::cfg_dir = ".srdp";
  const fs::path Srdp::db_file = "project.db";
  const fs::path Srdp::default_store_dir = "store";

  Srdp::Srdp()
  {
    top_level_dir = find_top_level_dir(fs::current_path());
    db = std::make_shared<Sql>(top_level_dir / cfg_dir / db_file);

    if (!db)
      throw std::runtime_error("Invalid DB pointer.");

    config = Config(db);
    if (!isatty(0)) interactive = false;
  }

  Srdp::Srdp(const fs::path& project_path, bool interactive) : interactive(interactive)
  {
    top_level_dir = find_top_level_dir(project_path);
    db = std::make_shared<Sql>(top_level_dir / cfg_dir / db_file);

    if (!db)
      throw std::runtime_error("Srdp: DB pointer invalid");

    config = Config(db);
    if (!isatty(0)) interactive = false;
  }

  //
  // Static members
  //
  void Srdp::init(const fs::path& dir, const fs::path& store_dir){
    if (fs::exists(dir) && !fs::is_directory(dir))
      throw std::invalid_argument("Target is not a directory");

    // cfg must not already exist
    if (fs::exists(dir / cfg_dir))
      throw std::runtime_error("SRDP directory already initialized");

    // create files
    fs::create_directories(dir / cfg_dir);

    std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(dir / cfg_dir / db_file);

    // Create database
    Project::create_table(*db);
    Experiment::create_table(*db);
    File::create_table(*db);
    Config::create_table(*db);

    Config cfg(db);

    fs::path final_store_dir;

    // Configure scas
    if (store_dir.empty()){
      final_store_dir = cfg_dir / default_store_dir;
      scas::Store store(final_store_dir);
      store.create_store_fs();
    } else {
      final_store_dir = fs::relative(
          fs::absolute(store_dir),
          fs::absolute(dir));
    }

    cfg.set_store_path(final_store_dir);
  }

  std::string Srdp::get_time_stamp_fmt(ctime_t timestamp){
     std::tm* timeinfo = std::localtime(&timestamp);
     if (timeinfo == nullptr)
       throw std::runtime_error("Error converting time stamp");

    const std::string format("%Y-%m-%d %H:%M:%S");
    std::stringstream ss;
    ss << std::put_time(timeinfo, format.c_str());
    return ss.str();
  }

  bool Srdp::is_uuid(const std::string& uuid){
    static const std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(uuid, uuid_regex);
  }

  //
  // Private members
  //
  std::string Srdp::get_user_name(){
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return std::string(pw->pw_name);
    } else {
        throw std::runtime_error("Failed to get user name");
    }
  }

  fs::path Srdp::find_top_level_dir(const fs::path& start_path){
    fs::path currentPath = fs::canonical(start_path);

    while (true) {
        fs::path srdp_dir = currentPath / cfg_dir;

        if (fs::exists(srdp_dir) && fs::is_directory(srdp_dir)) {
            return currentPath;
        }

        if (currentPath.has_parent_path()) {
            currentPath = currentPath.parent_path();
        } else {
            throw std::runtime_error("Can not find srdp top level directory.");
        }
    }
  }

  fs::path Srdp::get_store_dir(){
    return rel_to_top(top_level_dir / config.get_store_path(), true);
  }

  fs::path Srdp::get_cfg_dir(){
    return top_level_dir / cfg_dir;
  }

  fs::path Srdp::rel_to_top(const fs::path& path, bool proximate){
    if (proximate) {
      auto apath = fs::absolute(path);
      auto atop = fs::absolute(top_level_dir);

      auto itpath = apath.begin();
      auto ittop = atop.begin();

      fs::path rel_path;

      // skip common part
      while ( itpath != apath.end() && ittop != atop.end() && *itpath == *ittop) {
        ++itpath;
        ++ittop;
      }

      for (; ittop != atop.end(); ++ittop)
        rel_path /= "..";

      for (; itpath != apath.end(); ++itpath)
        rel_path /= *itpath;

      // canonicalize relative path
      fs::path can_rel_path;

      for (auto it=rel_path.begin(); it != rel_path.end(); ++it){
        if (*it == ".")
          continue;
        else if (*it == "..") can_rel_path = can_rel_path.parent_path();
        else can_rel_path /= *it;
      }

      return can_rel_path;

    } else
      return fs::relative(path, top_level_dir);
  }

  bool Srdp::path_is_in_dir(const fs::path& path){
    fs::path apath = fs::canonical(path);
    fs::path atop = fs::absolute(top_level_dir);

    auto itpath = apath.begin();
    auto ittop = atop.begin();

    while (true){
      if (ittop == atop.end()) return true;
      if (itpath == apath.end()) return false;

      if (*itpath != *ittop) return false;

      ++itpath;
      ++ittop;
    }
  }

  void Srdp::edit_text(std::string& text){
    if (!interactive)
      throw std::runtime_error("Can not open editor in non-interactive mode");

    if (!isatty(0) || !isatty(1))
      throw std::runtime_error("Can not open editor in non-terminal mode");

    const std::string editor = std::getenv("EDITOR");
    if (editor.empty())
      throw std::runtime_error("Environmnent variable EDITOR is not set");

    // Write text to tmp file
    const std::string tmp_name_tpl = get_cfg_dir().string() + "/tmp_" + "XXXXXX";
    std::vector<char> tmp_name_vec(tmp_name_tpl.begin(), tmp_name_tpl.end());
    tmp_name_vec.push_back('\0');

    int fd = mkstemp( tmp_name_vec.data() );
    if (fd == -1)
      throw std::runtime_error("Can not open tmpfile");

    if ( write(fd, text.data(), text.length()) == -1)
      throw std::runtime_error("Write to file failed");

    close(fd);

    // Edit file
    std::string tmp_name(tmp_name_vec.data());
    if (std::system((editor + " " + tmp_name).c_str()) != 0)
      throw std::runtime_error("Editor failed");

    // Get back result and clean up
    std::ifstream file(tmp_name);
    if (!file.is_open())
      throw std::runtime_error("Read back of edited file failed");

    text = std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    fs::remove(tmp_name);
  }

  Project Srdp::create_project(const std::string& name){
    Project prj(db, name, true);
    config.set_project(prj.uuid);

    return prj;
  }

  Project Srdp::open_project(const std::string& name){
    if (name.empty()){
      return Project(db, config.get_project());
    } else if (is_uuid(name))
      return Project(db, uuids::string_generator()(name));
    else
      return Project(db, name, false);
  }

  void Srdp::remove_project(const std::string& name){
    Project prj = open_project(name);
    prj.remove();
  }

  Experiment Srdp::create_experiment(const std::string& name, const std::string& project){
    Project prj = open_project(project);
    Experiment exp(db, prj, name, true);

    config.set_experiment(exp.uuid);

    return exp;
  }

  Experiment Srdp::open_experiment(const std::string& name, const std::string& project){
    Project prj = open_project(project);

    if (name.empty()){
      return Experiment(db, prj, config.get_experiment());
    } else if (is_uuid(name)){
      return Experiment(db, prj, uuids::string_generator()(name));
    } else
      return Experiment(db, prj, name, false);
  }

  void Srdp::remove_experiment(const std::string& name, const std::string& project){
    open_experiment(name, project).remove();
  }

  File Srdp::add_file(const std::string& project, const std::string& experiment, const fs::path& name, File::role_t role){
    Experiment exp = open_experiment(experiment, project);
    File dbfile(db, exp);
    dbfile.role = role;
    dbfile.original_name = name.filename();
    dbfile.path = rel_to_top(name, true);
    dbfile.owner = get_user_name();
    dbfile.ctime = get_timestamp_now();  // FIXME use actual file mtime

    if (!path_is_in_dir(name))
      throw std::runtime_error("File not in project directory");

    // Create a link that is relative to project dir? Distinguish between external/internal store?
    scas::Store store(get_store_dir());

    std::string hash_str;
    bool exists = store.file_is_in_store(name);
    if (exists) {
      hash_str = store.get_hash_from_path(name);
    } else {
      store.move_to_store(name, hash_str);
    }

    scas::Hash::hash_t hash_bin = scas::Hash::convert_string_to_hash(hash_str);

    try {
      dbfile.hash = hash_bin;
      dbfile.size = fs::file_size(name); // FIXME: scas should return that!?
      dbfile.create();
    } catch (std::exception& e) {
      if (!exists){
        // FIXME restore original file permission?
        auto tmp_name = fs::path(".tmp_") / name;
        fs::rename(name, tmp_name);
        fs::copy(tmp_name, name);
        fs::remove(tmp_name);
      }
      throw;
    }

    return dbfile;
  }

  void Srdp::unlink_file(const std::string& project, const std::string& experiment, const std::string& id){
    auto exp = open_experiment(experiment, project);
    auto file = load_file(project, experiment, id);
    auto path = file.path;
    auto creator = file.creator_uuid;

    // detach from experiment
    file.unmap();

    // Only restore file if created by linked experiment
    if (path && creator) {
      if (exp.uuid == *creator) {

        auto tmp_name = top_level_dir / fs::path(".tmp_");
        tmp_name += *path;
        auto name = top_level_dir / *path;

        try {
          fs::rename(name, tmp_name);
          fs::copy(tmp_name, name);
        } catch (std::exception& e) {
          fs::rename(tmp_name, name);
        }

        fs::remove(tmp_name);
      }
    }
  }

  File Srdp::load_file(const std::string& project, const std::string& experiment, const std::string& id){
    auto file = get_file(project, experiment);

    try {
      file.load(scas::Hash::convert_string_to_hash(id));
    } catch (std::exception& e) {
      file.load(id);
    }

    return file;
  }

  void Srdp::verify(){
    scas::Store store(get_store_dir());

    if (!store.verify_store())
      std::cout << "Store is inconsistent!" << "\n";

    // check if files match DB
    auto file_list = File(db).get_all_files();

    for (auto f : file_list) {
      if (!f.path) {
        std::cout << "File " << scas::Hash::convert_hash_to_string(f.hash) << " "
          << (f.original_name ? *f.original_name : "")
          << " has no path assigned!\n";

      } else if (!fs::exists(top_level_dir / *f.path)) {
        std::cout << *f.path << " does not exist!\n";
      } else if (!store.file_is_in_store(top_level_dir / *f.path) || !store.path_coincides_with_store(top_level_dir / *f.path)) {
        std::cout << *f.path << " is not located in store!\n";
      } else if (fs::file_size(top_level_dir / *f.path) != f.size) {
        std::cout << *f.path << " has the wrong file size in DB!\n";
      }
    }
  }
}

