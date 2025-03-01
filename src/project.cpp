// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include "project.h"
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
namespace srdp {

  //
  // Utils
  //
  ctime_t get_timestamp_now(){
    return chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count();
  }

  //
  // New
  //

  Project::Project(std::shared_ptr<Sql>& dbmain, const std::string& sel_name, bool create_new) :
    db(dbmain)
  {
    if (!db)
      throw std::runtime_error("DB pointer invalid");

    auto res = db->query("SELECT uuid, metadata, owner, ctime FROM projects WHERE name = ?;",
             Sql::vec_sql_t{sel_name},
             Sql::vec_sql_t{Sql::blob_t(), // uuid
                            std::string(), // meta
                            std::string(), // owner
                            int64_t(0)}    // ctime
             );

    if (!res && !create_new)
      throw std::runtime_error("Project not found in database");

    if (!res && create_new){
      create(sel_name);
    } else {
      auto row = *res;
      name = sel_name;
      uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
      metadata = Sql::sql_repack_optional<std::string>(row[1]);
      owner = Sql::sql_repack_optional<std::string>(row[2]);
      ctime = Sql::sql_repack_optional<ctime_t>(row[3]);
    }
  }

  Project::Project(std::shared_ptr<Sql>& dbin, uuids::uuid uuid_sel) : db(dbin) {
    load(uuid_sel);
  }

  Project::Project(std::shared_ptr<Sql>& dbin): db(dbin) {}

  void Project::create_table(Sql& db){
      db.exec(R"(
        CREATE TABLE IF NOT EXISTS projects (
          uuid BLOB(16) NOT NULL PRIMARY KEY,
          name VARCHAR(128) NOT NULL,
          metadata TEXT,
          owner TEXT,
          ctime INTEGER,
          journal TEXT
        );
        CREATE INDEX IF NOT EXISTS idx_project_name ON projects (name);
      )");
  }

  void Project::create(const std::string& new_name){
    uuids::uuid new_uuid = uuids::random_generator()();

    db->query("INSERT INTO projects (name, uuid) VALUES(?, ?)",
           Sql::vec_sql_t{new_name, bin_to_blob(new_uuid)});

    uuid = new_uuid;
    name = new_name;
    metadata = std::optional<std::string>();
    owner = std::optional<std::string>();
    ctime = std::optional<ctime_t>();
  }

  void Project::load(const uuids::uuid& uuid_sel){
    auto res = db->query("SELECT name, metadata, owner, ctime FROM projects WHERE uuid = ?;",
             Sql::vec_sql_t{bin_to_blob(uuid_sel)},
             Sql::vec_sql_t{std::string(), // name
                            std::string(), // meta
                            std::string(), // owner
                            int64_t(0)}    // ctime
             );

    if (!res)
      throw std::runtime_error("Project not found in database");

    auto row = *res;
    uuid = uuid_sel;
    name = std::get<std::string>(*row[0]);
    metadata = Sql::sql_repack_optional<std::string>(row[1]);
    owner = Sql::sql_repack_optional<std::string>(row[2]);
    ctime = Sql::sql_repack_optional<ctime_t>(row[3]);
  }

  void Project::load(const std::string& name_sel) {
    auto res = db->query("SELECT uuid, metadata, owner, ctime FROM projects WHERE name = ?;",
             Sql::vec_sql_t{name_sel},
             Sql::vec_sql_t{Sql::blob_t(), // uuid
                            std::string(), // meta
                            std::string(), // owner
                            int64_t(0)}    // ctime
             );
    if (!res)
      throw std::runtime_error("Project not found in database");

    auto row = *res;
    name = name_sel;
    uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
    metadata = Sql::sql_repack_optional<std::string>(row[1]);
    owner = Sql::sql_repack_optional<std::string>(row[2]);
    ctime = Sql::sql_repack_optional<ctime_t>(row[3]);
  }

  void Project::update(){
    if (uuid.is_nil())
      throw std::invalid_argument("Project UUID is not set");

    db->query(R"(
      UPDATE projects SET
        name = ?,
        metadata = ?,
        owner = ?,
        ctime = ?
      WHERE uuid = ?;)",
      Sql::vec_sql_t{name,
                     Sql::optional_null(metadata),
                     Sql::optional_null(owner),
                     Sql::optional_null(ctime),
                     bin_to_blob(uuid)});
  }

  void Project::remove(){
    db->query("DELETE FROM projects WHERE uuid = ?",
             Sql::vec_sql_t{bin_to_blob(uuid)});

    uuid = uuids::random_generator()();
    name = std::string();
    metadata = std::optional<std::string>();
    owner = std::optional<std::string>();
    ctime = std::optional<ctime_t>();
  }

  std::vector<Project> Project::list(){
    auto res = db->query("SELECT uuid, name, metadata, owner, ctime FROM projects ORDER BY ctime",
             Sql::vec_sql_t{},
             Sql::vec_sql_t{Sql::blob_t(), // uuid
                            std::string(), // name
                            std::string(), // meta
                            std::string(), // owner
                            int64_t(0)});  // ctime

    std::vector<Project> project_list;

    while (res) {
      auto row = *res;

      Project p(db);
      p.uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
      p.name = std::get<std::string>(*row[1]);
      p.metadata = Sql::sql_repack_optional<std::string>(row[2]);
      p.owner = Sql::sql_repack_optional<std::string>(row[3]);
      p.ctime = Sql::sql_repack_optional<ctime_t>(row[4]);
      project_list.push_back(p);

      res = db->next_row();
    }

    return project_list;
  }


  std::string Project::get_journal(){
    auto res = db->query("SELECT journal FROM projects WHERE uuid = ?;",
             Sql::vec_sql_t{bin_to_blob(uuid)},
             Sql::vec_sql_t{std::string()});

    if (!res)
      throw std::runtime_error("Invalid project UUID");

    auto row = *res;

    if (row[0]) return std::get<std::string>(*row[0]);
    else return "";
  }

  void Project::set_journal(const std::string& text){
    db->query("UPDATE projects SET journal = ? WHERE uuid = ?;",
             Sql::vec_sql_t{text, bin_to_blob(uuid)});
  }

  void Project::append_journal(const std::string& text){
    db->query("UPDATE projects SET journal = concat(journal, ?) WHERE uuid = ?;",
             Sql::vec_sql_t{text, bin_to_blob(uuid)});
  }

  ///
  /// Old
  ///
  // Project::Project(const fs::path& db_path) : db(db_path) {
  //   init_db();
  //
  //   db->prepare("SELECT COUNT(*) FROM projects;");
  //   db->step_row();
  //
  //   if (*db->get_column_int(0) == 0){
  //     throw std::runtime_error("No projects in database");
  //   } else if (*db->get_column_int(0) > 1) {
  //     throw std::runtime_error("More than one project in database. Select name or UUID.");
  //   }
  //
  //   db->finalize();
  //
  //   db->prepare("SELECT uuid, name FROM projects;");
  //   db->step_row();
  //
  //   uuid = blob_to_bin<uuids::uuid>(*db->get_column_blob(0));
  //   name = *db->get_column_str(1);
  //
  //   db->finalize();
  // }
  //
  // Project::Project(const fs::path& db_path, const std::string& project_name, bool create) : db(db_path) {
  //   init_db();
  //
  //   db->prepare("SELECT uuid, name FROM projects WHERE name = ?;");
  //   db->bind_str(1, project_name);
  //   const bool row = db->step_row();
  //   if (!row && !create)
  //     throw std::runtime_error("Project not found in database");
  //
  //   if (!row && create) {
  //     uuids::random_generator gen;
  //     uuid = gen();
  //     name = project_name;
  //
  //     db->prepare("INSERT INTO projects (uuid, name) VALUES( ?, ? )");
  //     db->bind_blob(1, bin_to_blob(uuid));
  //     db->bind_str(2, name);
  //     db->step();
  //     db->finalize();
  //     return;
  //   }
  //
  //   uuid = blob_to_bin<uuids::uuid>(*db->get_column_blob(0));
  //   name = *db->get_column_str(1);
  //
  //   db->finalize();
  //   db->flush();
  // }
  //
  // void Project::init_db(){
  //   // Initialize if empty
  //   db->prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='projects'");
  //   db->step_row();
  //   if (*db->get_column_int(0) < 1){
  //     db->finalize();
  //
  //     db->exec(R"(
  //       CREATE TABLE IF NOT EXISTS projects (
  //         uuid BLOB(16) NOT NULL PRIMARY KEY,
  //         name VARCHAR(64) NOT NULL,
  //         owner TEXT,
  //         ctime INTEGER,
  //         journal TEXT
  //       );
  //       CREATE INDEX IF NOT EXISTS idx_project_name ON projects (name);
  //
  //       CREATE TABLE IF NOT EXISTS files (
  //         hash BLOB(32) NOT NULL PRIMARY KEY,
  //         name VARCHAR(1024) NOT NULL,
  //         creator BLOB(16),
  //         owner TEXT,
  //         ctime INTEGER,
  //         metadata TEXT DEFAULT ""
  //       );
  //
  //       CREATE INDEX IF NOT EXISTS idx_creator_uuid ON files (creator);
  //
  //       CREATE TABLE IF NOT EXISTS experiments (
  //         uuid BLOB(16) NOT NULL PRIMARY KEY,
  //         project BLOB(16) NOT NULL,
  //         name VARCHAR(64) NOT NULL,
  //         owner TEXT,
  //         ctime INTEGER,
  //         journal TEXT,
  //         locked BOOLEAN DEFAULT FALSE
  //       );
  //
  //       CREATE INDEX IF NOT EXISTS idx_project_uuid ON experiments (project);
  //
  //       CREATE TABLE IF NOT EXISTS file_map (
  //         uuid BLOB(16) NOT NULL,
  //         hash BLOB(32) NOT NULL,
  //         role INTEGER NOT NULL
  //       );
  //
  //       CREATE INDEX IF NOT EXISTS idx_experiment_uuid ON file_map (uuid);
  //       CREATE INDEX IF NOT EXISTS idx_file_hash ON file_map (hash);
  //     )");
  //   } else db->finalize();
  //
  //   db->flush();
  // }
  //
  // std::string Project::get_name(){
  //   return name;
  // }
  //
  // uuids::uuid Project::get_uuid(){
  //   return uuid;
  // }
  //
  // std::optional<std::string> Project::get_owner(){
  //   auto res = db->query("SELECT owner FROM projects WHERE uuid = ?",
  //            Sql::vec_sql_t{bin_to_blob(uuid)},
  //            Sql::vec_sql_t{std::string()});
  //
  //   if (!res)
  //     throw std::runtime_error("Invalid project UUID");
  //
  //   auto row = *res;
  //   return Sql::sql_repack_optional<std::string>(row[0]);
  //
  //   // db->prepare("SELECT owner FROM projects WHERE uuid = ?");
  //   // db->bind_blob(1, bin_to_blob(uuid));
  //   // db->step_row();
  //   // std::optional<std::string> ret = db->get_column_str(0);
  //   // db->finalize();
  //   //
  //   // return ret;
  // };
  //
  // void Project::set_owner(const std::optional<std::string>& value){
  //   if (value)
  //     db->query("UPDATE projects SET owner = ? WHERE uuid = ?;",
  //             Sql::vec_sql_t{*value, bin_to_blob(uuid)});
  //
  //   // if(value) {
  //   //   db->prepare("UPDATE projects SET owner = ? WHERE uuid = ?");
  //   //   db->bind_str(1, *value);
  //   //   db->bind_blob(2, bin_to_blob(uuid));
  //   //   db->step();
  //   //   db->finalize();
  //   // }
  // }
  //
  // std::optional<ctime_t> Project::get_ctime(){
  //   auto res = db->query("SELECT ctime FROM projects WHERE uuid = ?",
  //            Sql::vec_sql_t{bin_to_blob(uuid)},
  //            Sql::vec_sql_t{int64_t(0)});
  //
  //   if (!res)
  //     throw std::runtime_error("Invalid project UUID");
  //
  //   auto row = *res;
  //   return Sql::sql_repack_optional<ctime_t>(row[0]);
  //
  //   // db->prepare("SELECT ctime FROM projects WHERE uuid = ?");
  //   // db->bind_blob(1, bin_to_blob(uuid));
  //   // db->step_row();
  //   //
  //   // std::optional<ctime_t> ret;
  //   // if (db->get_column_int64(0))
  //   //   ret = int_to_ctime(*db->get_column_int64(0));
  //   //
  //   // db->finalize();
  //
  //   // return ret;
  // }
  //
  // void Project::set_ctime(const std::optional<ctime_t>& value){
  //   if (value)
  //     db->query("UPDATE projects SET ctime = ? WHERE uuid = ?;",
  //             Sql::vec_sql_t{*value, bin_to_blob(uuid)});
  //
  //   // if(value) {
  //   //   db->prepare("UPDATE projects SET ctime = ? WHERE uuid = ?");
  //   //   db->bind_int64(1, ctime_to_int(*value));
  //   //   db->bind_blob(2, bin_to_blob(uuid));
  //   //   db->step();
  //   //   db->finalize();
  //   // }
  // }
  //
  // void Project::add_experiment(Experiment& exp){
  //   db->query("INSERT INTO experiments (uuid, project, name, owner, ctime) VALUES (?, ?, ?, ?, ?);",
  //            Sql::vec_sql_t{bin_to_blob(exp.get_uuid()),
  //                           bin_to_blob(uuid),
  //                           exp.get_name(),
  //                           exp.get_owner() ? Sql::sql_t(*exp.get_owner()) : Sql::sql_t(Sql::null_value()),
  //                           exp.get_ctime() ? Sql::sql_t(*exp.get_ctime()) : Sql::sql_t(Sql::null_value())});
  //
  //   exp.set_project(uuid);
  // }
  //
  // Experiment Project::get_experiment(const std::string& name){
  //   auto res = db->query("SELECT uuid, project, name, owner, ctime, locked FROM experiments WHERE project = ? AND name = ?;",
  //            Sql::vec_sql_t{bin_to_blob(uuid), name},
  //            Sql::vec_sql_t{bin_to_blob(uuid),
  //                           bin_to_blob(uuid),
  //                           std::string(),
  //                           std::string(),
  //                           int64_t(0),
  //                           bool(true)});
  //   if (!res)
  //     throw std::runtime_error("Experiment not found by name in DB.");
  //
  //   auto row = *res;
  //   Experiment exp(blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0])), // uuid
  //                  blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[1])), // project
  //                  std::get<std::string>(*row[2]),               // name
  //                  Sql::sql_repack_optional<std::string>(row[3]),// owner
  //                  Sql::sql_repack_optional<ctime_t>(row[4]), // ctime
  //                  std::get<bool>(*row[5]) // locked
  //
  //   );
  //
  //   return exp;
  // }
  //
  // Experiment Project::get_experiment(const uuids::uuid& exp_uuid){
  //   auto res = db->query("SELECT uuid, project, name, owner, ctime, locked FROM experiments WHERE project = ? AND uuid = ?;",
  //            Sql::vec_sql_t{bin_to_blob(uuid), bin_to_blob(exp_uuid)},
  //            Sql::vec_sql_t{bin_to_blob(uuid),
  //                           bin_to_blob(uuid),
  //                           std::string(),
  //                           std::string(),
  //                           int64_t(0),
  //                           bool(true)});
  //   if (!res)
  //     throw std::runtime_error("Experiment not found by name in DB.");
  //
  //   auto row = *res;
  //   Experiment exp(blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0])), // uuid
  //                  blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[1])), // project
  //                  std::get<std::string>(*row[2]),               // name
  //                  Sql::sql_repack_optional<std::string>(row[3]),// owner
  //                  Sql::sql_repack_optional<ctime_t>(row[4]), // ctime
  //                  std::get<bool>(*row[5]) // locked
  //
  //   );
  //
  //   return exp;
  // }
  // std::string Project::get_journal_experiment(const Experiment& exp){
  //   auto res = db->query("SELECT journal, project FROM experiments WHERE uuid = ?;",
  //            Sql::vec_sql_t{bin_to_blob(exp.get_uuid())},
  //            Sql::vec_sql_t{std::string(), Sql::blob_t{}});
  //
  //   if (!res)
  //     throw std::runtime_error("Invalid project UUID");
  //
  //   auto row = *res;
  //
  //   if (uuid != blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[1])))
  //     throw std::logic_error("Experiment not part of project");
  //
  //   if (row[0]) return std::get<std::string>(*row[0]);
  //   else return "";
  // }
  //
  // void Project::set_journal_experiment(const Experiment& exp, const std::string& text){
  //   if (uuid != exp.get_project())
  //     throw std::logic_error("Experiment not part of project");
  //
  //   db->query("UPDATE experiments SET journal = ? WHERE uuid = ?;",
  //            Sql::vec_sql_t{text, bin_to_blob(exp.get_uuid())});
  //
  // }
  //
  // void Project::append_journal_experiment(const Experiment& exp, const std::string& text){
  //   if (uuid != exp.get_project())
  //     throw std::logic_error("Experiment not part of project");
  //
  //   db->query("UPDATE experiments SET journal = CONCAT(journal, ?) WHERE uuid = ?;",
  //            Sql::vec_sql_t{text, bin_to_blob(exp.get_uuid())});
  // }
  //
  // void Project::set_lock_experiment(Experiment& exp){
  //   if (uuid != exp.get_project())
  //     throw std::logic_error("Experiment not part of project");
  //
  //   db->query("UPDATE experiments SET lock = ? WHERE uuid = ?;",
  //            Sql::vec_sql_t{exp.get_locked(), bin_to_blob(exp.get_uuid())});
  // }
  //
  // void Project::add_file(const Experiment& exp, const File& file){
  //         // hash BLOB(32) NOT NULL PRIMARY KEY,
  //         // name VARCHAR(1024) NOT NULL,
  //         // creator BLOB(16),
  //         // owner TEXT,
  //         // ctime INTEGER,
  //         // metadata TEXT DEFAULT ""
  //         //
  //
  //   if (!file.role)
  //     throw std::invalid_argument("File role can not be undefined");
  //
  //   db->query(R"(
  //     BEGIN TRANSACTION;
  //     INSERT INTO files IF NOT EXISTS (hash, name, creator, owner, ctime, metadata) VALUES(?,?,?,?,?,?);
  //     INSERT INTO files_map (hash, uuid, role) VALUES(?,?,?);
  //     COMMIT;
  //   )",
  //     Sql::vec_sql_t{bin_to_blob(file.hash), // table files
  //                    file.original_name,
  //                    bin_to_blob(exp.get_uuid()),
  //                    Sql::optional_null(file.owner),
  //                    file.ctime,
  //                    file.metadata,
  //                    bin_to_blob(file.hash), // table files_map
  //                    bin_to_blob(exp.get_uuid()),
  //                    int(*file.role)
  //                    }
  //       );
  // }
}
