// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_PROJECT_H
#define SRDP_PROJECT_H

#include <memory>
#include <chrono>
#include <optional>
#include <filesystem>

#include "sql.h"

namespace srdp {

  namespace chrono = std::chrono;
  namespace fs = std::filesystem;

  typedef int64_t ctime_t;
  ctime_t get_timestamp_now();


  class Project {
    private:
      std::shared_ptr<Sql> db;

    public:
      uuids::uuid uuid;
      std::string name;
      std::optional<std::string> metadata;
      std::optional<std::string> owner;
      std::optional<ctime_t>     ctime;

      Project(std::shared_ptr<Sql>& dbin, const std::string& name, bool create = true);
      Project(std::shared_ptr<Sql>& dbin, uuids::uuid uuid);
      Project(std::shared_ptr<Sql>& dbin);

      static void create_table(Sql& db);
      void load(const uuids::uuid& uuid);
      void load(const std::string& name);
      void create(const std::string& name);
      void remove();

      void update();
      std::vector<Project> list();

      std::string get_journal();
      void set_journal(const std::string& text);
      void append_journal(const std::string& text);

      //
      // void add_file(const Experiment& exp, const File& file);
      // void get_files();
  };

}

#endif /* SRDP_PROJECT_H */
