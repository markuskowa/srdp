#include "experiment.h"

#include <iostream>

namespace srdp {

  void Experiment::create_table(Sql& db){
    db.exec(R"(

        CREATE TABLE IF NOT EXISTS experiments (
          uuid BLOB(16) NOT NULL PRIMARY KEY,
          project BLOB(16) NOT NULL REFERENCES projects(uuid),
          name VARCHAR(64) NOT NULL,
          metadata TEXT,
          owner TEXT,
          ctime INTEGER,
          journal TEXT,
          locked BOOLEAN DEFAULT FALSE,
          UNIQUE(project, name),
          FOREIGN KEY(project) REFERENCES projects(uuid)
        );

        CREATE INDEX IF NOT EXISTS idx_project_uuid ON experiments (project);
        CREATE INDEX IF NOT EXISTS idx_experiment_name ON experiments (name);
      )");
//FOREIGN KEY (uuid) REFERENCES file_map(uuid)
  }

  Experiment::Experiment(std::shared_ptr<Sql>& dbin, const Project& projectin, const std::string& sel_name, bool create_new) :
    db(dbin), project(projectin.uuid), name(sel_name)
  {
    if (project.is_nil())
      throw std::runtime_error("Experiment not attached to a project.");

    auto res = db->query("SELECT uuid, metadata, owner, ctime, locked FROM experiments WHERE project = ? AND name = ?;",
             Sql::vec_sql_t{bin_to_blob(project), name},
             Sql::vec_sql_t{bin_to_blob(project),
                            std::string(),
                            std::string(),
                            int64_t(0),
                            bool(true)});
    if (!res && !create_new)
      throw std::runtime_error("Experiment not found by name in DB.");

    if (!res && create_new) {
      create(name);
    } else {
      auto row = *res;
      uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
      metadata = Sql::sql_repack_optional<std::string>(row[1]);
      owner = Sql::sql_repack_optional<std::string>(row[2]);
      ctime = Sql::sql_repack_optional<ctime_t>(row[3]);
      locked = std::get<bool>(*row[4]);
    }
  }

  Experiment::Experiment(std::shared_ptr<Sql>& dbin, const Project& projectin, const uuids::uuid& sel_uuid) :
    db(dbin), project(projectin.uuid)
  {
    load(sel_uuid);
  }

  Experiment::Experiment(std::shared_ptr<Sql>& dbin, const Project& projectin) :
    db(dbin),  project(projectin.uuid)
  {
    if (project.is_nil())
      throw std::runtime_error("Experiment not attached to a project.");
  }

  Experiment::Experiment(std::shared_ptr<Sql>& dbin, uuids::uuid& project_uuid) :
    db(dbin), project(project_uuid)
  {
    if (project.is_nil())
      throw std::runtime_error("Experiment not attached to a project.");
  }

  Experiment::Experiment(std::shared_ptr<Sql>& dbin) : db(dbin) {}

  void Experiment::load(const uuids::uuid& sel_uuid) {
    auto res = db->query(R"(
      SELECT project, name, metadata, owner, ctime, locked
      FROM experiments
      WHERE uuid = ?;)",
             Sql::vec_sql_t{bin_to_blob(sel_uuid)},
             Sql::vec_sql_t{bin_to_blob(sel_uuid),
                            std::string(),
                            std::string(),
                            std::string(),
                            int64_t(0),
                            bool(true)});

    if (!res)
      throw std::runtime_error("Experiment not found by name in DB.");

    auto row = *res;
    uuid = sel_uuid;
    project = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
    name = std::get<std::string>(*row[1]);
    metadata = Sql::sql_repack_optional<std::string>(row[2]);
    owner = Sql::sql_repack_optional<std::string>(row[3]);
    ctime = Sql::sql_repack_optional<ctime_t>(row[4]);
    locked = std::get<bool>(*row[5]);
  }

  void Experiment::load(const std::string& sel_name){
    if (project.is_nil())
      throw std::runtime_error("Experiment not attached to a project.");

    auto res = db->query("SELECT uuid, metadata, owner, ctime, locked FROM experiments WHERE project = ? AND name = ?;",
             Sql::vec_sql_t{bin_to_blob(project), name},
             Sql::vec_sql_t{bin_to_blob(uuid),
                            std::string(),
                            std::string(),
                            int64_t(0),
                            bool(true)});
    if (!res)
      throw std::runtime_error("Experiment not found by name in DB.");

    auto row = *res;
    name = sel_name;
    uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
    metadata = Sql::sql_repack_optional<std::string>(row[1]);
    owner = Sql::sql_repack_optional<std::string>(row[2]);
    ctime = Sql::sql_repack_optional<ctime_t>(row[3]);
    locked = std::get<bool>(*row[4]);

  }

  void Experiment::create(const std::string& new_name){
    if (project.is_nil())
      throw std::runtime_error("Experiment not attached to a project.");

    uuids::uuid new_uuid = uuids::random_generator()();

    db->query("INSERT INTO experiments (name, uuid, project) VALUES(?, ?, ?)",
           Sql::vec_sql_t{new_name, bin_to_blob(new_uuid), bin_to_blob(project)});

    uuid = new_uuid;
    name = new_name;
    metadata = std::optional<std::string>();
    owner = std::optional<std::string>();
    ctime = std::optional<ctime_t>();
    locked = false;
  }

  void Experiment::update(){
    if (project.is_nil() || uuid.is_nil())
      throw std::runtime_error("Experiment not attached to a project or UUID not set.");

    db->query(R"(
      UPDATE experiments SET
        name = ?,
        metadata = ?,
        owner = ?,
        ctime = ?,
        locked = ?
      WHERE uuid = ?;)",
      Sql::vec_sql_t{name,
                     Sql::optional_null(metadata),
                     Sql::optional_null(owner),
                     Sql::optional_null(ctime),
                     locked,
                     bin_to_blob(uuid)});

  }

  void Experiment::remove(){
    db->query("DELETE FROM experiments WHERE uuid = ?",
             Sql::vec_sql_t{bin_to_blob(uuid)});

    uuid = uuids::random_generator()();
    name = std::string();
    metadata = std::optional<std::string>();
    owner = std::optional<std::string>();
    ctime = std::optional<ctime_t>();
    locked = false;
  }

  std::vector<Experiment> Experiment::list(){
    auto res = db->query(R"(
      SELECT uuid, project, name, metadata, owner, ctime, locked
      FROM experiments
      WHERE project = ?
      ORDER BY ctime
      )",
             Sql::vec_sql_t{bin_to_blob(project)},
             Sql::vec_sql_t{Sql::blob_t(), // uuid
                            Sql::blob_t(), // project
                            std::string(), // name
                            std::string(), // meta
                            std::string(), // owner
                            int64_t(0),    // ctime
                            bool(true)});  // locked

    std::vector<Experiment> experiment_list;

    while (res) {
      auto row = *res;

      Experiment e(db, project);
      e.uuid = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
      e.project = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[1]));
      e.name = std::get<std::string>(*row[2]);
      e.metadata = Sql::sql_repack_optional<std::string>(row[3]);
      e.owner = Sql::sql_repack_optional<std::string>(row[4]);
      e.ctime = Sql::sql_repack_optional<ctime_t>(row[5]);
      experiment_list.push_back(e);

      res = db->next_row();
    }

    return experiment_list;
  }

  std::string Experiment::get_journal(){
    auto res = db->query("SELECT journal FROM experiments WHERE uuid = ?;",
             Sql::vec_sql_t{bin_to_blob(uuid)},
             Sql::vec_sql_t{std::string()});

    if (!res)
      throw std::runtime_error("Invalid project UUID");

    auto row = *res;

    if (row[0]) return std::get<std::string>(*row[0]);
    else return "";
  }

  void Experiment::set_journal(const std::string& text){
    db->query("UPDATE experiments SET journal = ? WHERE uuid = ?;",
             Sql::vec_sql_t{text, bin_to_blob(uuid)});
  }

  void Experiment::append_journal(const std::string& text){
    db->query("UPDATE experiments SET journal = concat(journal, ?) WHERE uuid = ?;",
             Sql::vec_sql_t{text, bin_to_blob(uuid)});
  }
}
