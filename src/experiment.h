// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_EXPERIMENT_H
#define SRDP_EXPERIMENT_H

#include "project.h"

namespace srdp {

  class Experiment {
    private:
      std::shared_ptr<Sql> db;

    public:
      uuids::uuid project;

      // Accessible properties
      uuids::uuid uuid;
      std::string name;
      std::optional<std::string> metadata;
      std::optional<std::string> owner;
      std::optional<ctime_t> ctime;
      bool locked; // Currently not used

      static void create_table(Sql& db);

      // load data
      Experiment(std::shared_ptr<Sql>& dbin, const Project& project, const std::string& name, bool create = true);
      Experiment(std::shared_ptr<Sql>& dbin, const Project& project, const uuids::uuid& uuid);

      // create empty
      Experiment(std::shared_ptr<Sql>& dbin, const Project& project);
      Experiment(std::shared_ptr<Sql>& dbin, uuids::uuid& project_uuid);
      Experiment(std::shared_ptr<Sql>& dbin);

      void load(const uuids::uuid& uuid);
      void load(const std::string& name);
      void create(const std::string& name);
      void remove();

      void update();
      std::vector<Experiment> list();

      std::string get_journal();
      void set_journal(const std::string& text);
      void append_journal(const std::string& text);
  };
}

#endif
