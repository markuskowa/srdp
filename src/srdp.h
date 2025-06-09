// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_SRDP_H
#define SRDP_SRDP_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <regex>

#include "project.h"
#include "experiment.h"
#include "files.h"
#include "config.h"

#include "cmake_config.h"

namespace srdp {
  const std::string name = PROJECT_NAME;
  const std::string version = PROJECT_VERSION;

  class Srdp {
    private:
      const fs::path gc_roots_dir = "gc-roots"; // Needed as seperate dir?

      std::shared_ptr<Sql> db;
      bool interactive = false;
      fs::path top_level_dir;

      fs::path find_top_level_dir(const fs::path& start_path);
      fs::path get_store_dir();
      fs::path get_cfg_dir();
      fs::path rel_to_top(const fs::path& path, bool proximate = false);
      bool path_is_in_dir(const fs::path& path);
      void find_last_experiment();
      void find_open_experiments();

      void check_db_schema_version();
    public:
      static const std::string db_schema_version;
      static const fs::path cfg_dir;
      static const fs::path db_file;
      static const fs::path default_store_dir;

      Config config;

      // Try to find the config automatically
      Srdp();

      // Open by project path
      Srdp(const fs::path& project_path, bool interactive = false);

      /* Create new directory.
       *
       * Directory must not exist.
       * If path is given the CAS is assumend to be external (and existing).
       */
      static void init(const fs::path& dir, const fs::path& store = fs::path());

      static std::string get_time_stamp_fmt(ctime_t = get_timestamp_now());
      static std::string get_user_name();
      static bool is_uuid(const std::string& uuid);

      void edit_text(std::string& text);

      Project create_project(const std::string& name);
      Project get_project() { return Project(db); };

      // Takes a name or uuid
      Project open_project(const std::string& name = std::string());
      void remove_project(const std::string& name);

      void list_experiments();
      Experiment create_experiment(const std::string& name, const std::string& project = std::string());

      Experiment get_experiment(const std::string& project = std::string()) { return Experiment(db, open_project(project)); } ;
      Experiment open_experiment(const std::string& name = std::string(), const std::string& project = std::string());
      void remove_experiment(const std::string& name = std::string(), const std::string& project = std::string());

      File get_file(const std::string& project = std::string(), const std::string& experiment = std::string()) { return File(db, open_experiment(experiment, project)); }
      void list_files();
      File add_file(const std::string& project, const std::string& experiment, const fs::path& name, File::role_t role);
      File load_file(const std::string& project, const std::string& experiment, const std::string& id);
      void unlink_file(const std::string& project, const std::string& experiment, const std::string& id);

      void verify();
  };
}

#endif

