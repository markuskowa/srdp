// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <map>
#include "files.h"

#include <boost/uuid/uuid_io.hpp>


namespace srdp {

  void File::create_table(Sql& db){
    db.exec(R"(
        CREATE TABLE IF NOT EXISTS files (
          hash BLOB(32) NOT NULL PRIMARY KEY,
          size INTEGER NOT NULL CHECK (size >= 0),
          name VARCHAR(1024),
          creator BLOB(16) REFERENCES experiments(uuid),
          owner TEXT,
          ctime INTEGER,
          metadata TEXT,
          final BOOLEAN DEFAULT FALSE,
          FOREIGN KEY(creator) REFERENCES experiments(uuid)
        );

        CREATE INDEX IF NOT EXISTS idx_creator_uuid ON files (creator);

        CREATE TABLE IF NOT EXISTS file_map (
          uuid BLOB(16) NOT NULL REFERENCES experiments(uuid),
          hash BLOB(32) NOT NULL REFERENCES files(hash),
          role INTEGER NOT NULL CHECK (role > 0 and role <= 4),
          path VARCHAR(1024),
          UNIQUE(uuid, hash),
          FOREIGN KEY(uuid) REFERENCES experiments(uuid),
          FOREIGN KEY(hash) REFERENCES files(hash)
        );

        CREATE INDEX IF NOT EXISTS idx_experiment_uuid ON file_map (uuid);
        CREATE INDEX IF NOT EXISTS idx_file_hash ON file_map (hash);

        CREATE TABLE IF NOT EXISTS file_roles (
          id INTEGER NOT NULL PRIMARY KEY,
          role VARCHAR(32) NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_file_roles ON file_roles (role);

        INSERT OR IGNORE INTO file_roles(id, role) VALUES(1, 'input');
        INSERT OR IGNORE INTO file_roles(id, role) VALUES(2, 'output');
        INSERT OR IGNORE INTO file_roles(id, role) VALUES(3, 'note');
        INSERT OR IGNORE INTO file_roles(id, role) VALUES(4, 'program');
        INSERT OR IGNORE INTO file_roles(id, role) VALUES(5, 'nixpath');
    )");
  }

  std::string File::role_to_string(role_t role){
    static const std::map<role_t, std::string> role_map = {
        {role_t::none, "none"},
        {role_t::input, "input"},
        {role_t::output, "output"},
        {role_t::program, "program"},
        {role_t::note, "note"},
    };

    auto it = role_map.find(role);
    return it->second;
  }

  File::role_t File::string_to_role(const std::string& role){
    static const std::map<std::string, role_t> role_map = {
        {"none"   , role_t::none   },
        {"input"  , role_t::input  },
        {"output" , role_t::output },
        {"program", role_t::program},
        {"note"   , role_t::note   },
        {"i"      , role_t::input  },
        {"o"      , role_t::output },
        {"p"      , role_t::program},
        {"n"      , role_t::note   },
    };

    auto it = role_map.find(role);
    if (it != role_map.end())
      return it->second;
    else
      throw std::runtime_error("Invalid role");
  }

  File::File(std::shared_ptr<Sql>& dbin) :
    db(dbin)
  {
  }

  File::File(std::shared_ptr<Sql>& dbin, const Experiment& exp) :
    db(dbin), experiment(exp.uuid)
  {
    hash.fill(0);
  }

  File::File(std::shared_ptr<Sql>& dbin, const uuids::uuid& exp) :
    db(dbin), experiment(exp)
  {
    hash.fill(0);
  }

  File::File(std::shared_ptr<Sql>& dbin, const Experiment& exp, const scas::Hash::hash_t& sel_hash) :
    db(dbin), experiment(exp.uuid)
  {
    load(sel_hash);
  }

  File::File(std::shared_ptr<Sql>& dbin, const uuids::uuid& sel_uuid, const scas::Hash::hash_t& sel_hash) :
    db(dbin), experiment(sel_uuid)
  {
    load(sel_hash);
  }

  bool File::exists(const scas::Hash::hash_t& hash_exists) {
    auto res = db->query(R"(
        SELECT count(*) FROM files WHERE hash = ?;
    )",
             Sql::vec_sql_t{bin_to_blob(hash_exists)},
             Sql::vec_sql_t{int(0)});

    if (res){
      auto row = *res;
      if (std::get<int>(*row[0]) > 0)
        return true;
      else
        return false;
    }
    else
      return false;
  }

  bool File::is_mapped(const scas::Hash::hash_t& hash_exists){
    auto res = db->query(R"(
        SELECT count(*) FROM file_map WHERE hash = ? AND uuid = ?;
    )",
             Sql::vec_sql_t{bin_to_blob(hash_exists), bin_to_blob(experiment)},
             Sql::vec_sql_t{int(0)});

    if (res){
      auto row = *res;
      if (std::get<int>(*row[0]) > 0)
        return true;
      else
        return false;
    }
    else
      return false;
  }

  void File::load(const scas::Hash::hash_t& sel_hash){
    auto res = db->query(R"(
        SELECT
          files.size, files.name, files.creator, files.owner, files.ctime, files.metadata, file_map.path, file_map.role
        FROM file_map
        JOIN files ON file_map.hash = files.hash
        WHERE file_map.hash = ? AND file_map.uuid = ?;
      )",
       Sql::vec_sql_t{bin_to_blob(sel_hash), bin_to_blob(experiment)},
       Sql::vec_sql_t{int64_t(0),               // size
                      std::string(),            // name
                      bin_to_blob(experiment),  // creator
                      std::string(),            // owner
                      int64_t(0),               // ctime
                      std::string(),            // metadata
                      std::string(),            // path
                      int(0),                   // role
                      });


    if (res) {
      auto row = *res;
      hash = sel_hash;
      size = std::get<int64_t>(*row[0]);
      original_name = Sql::sql_repack_optional<std::string>(row[1]);
      creator_uuid = Sql::sql_repack_optional<Sql::blob_t, uuids::uuid>(row[2], blob_to_bin<uuids::uuid>);
      owner = Sql::sql_repack_optional<std::string>(row[3]);
      ctime = Sql::sql_repack_optional<int64_t>(row[4]);
      metadata = Sql::sql_repack_optional<std::string>(row[5]);
      path = Sql::sql_repack_optional<std::string>(row[6]);
      role = Sql::sql_repack_optional<int, role_t>(row[7], [](int in){return role_t(in);});
    } else
      throw std::runtime_error("File not found in DB");
  }

  void File::load(const std::string& sel_path){
    auto res = db->query(R"(
        SELECT
          files.hash, files.size, files.name, files.creator, files.owner, files.ctime, files.metadata, file_map.role
        FROM file_map
        JOIN files ON file_map.hash = files.hash
        WHERE file_map.path = ? AND file_map.uuid = ?;
      )",
       Sql::vec_sql_t{sel_path, bin_to_blob(experiment)},
       Sql::vec_sql_t{bin_to_blob(scas::Hash::hash_t()), // hash
                      int64_t(0),               // size
                      std::string(),            // name
                      bin_to_blob(experiment),  // creator
                      std::string(),            // owner
                      int64_t(0),               // ctime
                      std::string(),            // metadata
                      int(0),                   // role
                      });


    if (res) {
      auto row = *res;
      hash = blob_to_bin<scas::Hash::hash_t>(std::get<Sql::blob_t>(*row[0]));
      size = std::get<int64_t>(*row[1]);
      original_name = Sql::sql_repack_optional<std::string>(row[2]);
      creator_uuid = Sql::sql_repack_optional<Sql::blob_t, uuids::uuid>(row[3], blob_to_bin<uuids::uuid>);
      owner = Sql::sql_repack_optional<std::string>(row[4]);
      ctime = Sql::sql_repack_optional<int64_t>(row[5]);
      metadata = Sql::sql_repack_optional<std::string>(row[6]);
      path = sel_path;
      role = Sql::sql_repack_optional<int, role_t>(row[7], [](int in){return role_t(in);});
    } else
      throw std::runtime_error("File not found in DB");
  }

  std::string File::resolve_creator(){
    auto res = db->query(R"(
        SELECT projects.name, experiments.name
        FROM files
        JOIN experiments ON files.creator = experiments.uuid
        JOIN projects ON experiments.project = projects.uuid
        WHERE hash = ?;
      )",
       Sql::vec_sql_t{bin_to_blob(hash)},
       Sql::vec_sql_t{std::string(), std::string()}); // name

    if (!res) return "";

    auto row = *res;
    return std::get<std::string>(*row[0]) + "::" + std::get<std::string>(*row[1]);
  }

  bool File::create(){
    // Insert into file, feedback if exists
    // Insert into map, fail if exists

    if (hash.empty())
      throw std::runtime_error("File hash is not set");

    if (experiment.is_nil())
      throw std::runtime_error("File not attached to experiment");

    if (size == 0)
      throw std::runtime_error("Invalid file size");

    if (!role)
      throw std::runtime_error("File's role is not set");

    bool file_is_new = true;
    if (exists(hash))
      file_is_new = false;
    else {

      if (role == role_t::output)
        creator_uuid = experiment;
      else
        creator_uuid = std::optional<uuids::uuid>();

      if (!ctime)
        ctime = get_timestamp_now();

      // create file
      db->query(R"(
           INSERT INTO files (hash, size, name, creator, owner, ctime, metadata) VALUES (?, ?, ?, ?, ?, ?, ?);
         )",
         Sql::vec_sql_t{bin_to_blob(hash),
                        int64_t(size),
                        Sql::optional_null(original_name),
                        bin_to_blob(experiment),
                        Sql::optional_null(owner),
                        *ctime,
                        Sql::optional_null(metadata)});
    }

    // is part of experiment
    // map to experiment

    if (is_mapped(hash))
        throw std::invalid_argument("File is already mapped to experiment");
    else {
      db->query(R"(
           INSERT INTO file_map (hash, uuid, path, role) VALUES (?, ?, ?, ?);
         )",
         Sql::vec_sql_t{bin_to_blob(hash), bin_to_blob(experiment), Sql::optional_null(path), int(*role)});
    }

    return file_is_new;
  }

  bool File::output_is_used(const scas::Hash::hash_t& sel_hash){
    auto res = db->query(R"(
      SELECT COUNT(*)
      FROM file_map
      JOIN file_roles ON file_map.role = file_roles.id
      WHERE file_roles.role = 'input' AND hash = (
        SELECT hash
        FROM file_map
        JOIN file_roles ON file_map.role = file_roles.id
        WHERE uuid = ? AND file_roles.role = 'output'
      );
      )",
       Sql::vec_sql_t{bin_to_blob(experiment)},
       Sql::vec_sql_t{int(0)});

    if (res){
      auto row = *res;
      if (std::get<int>(*row[0]) > 0)
        return true;
      else
        return false;
    }

    return false;
  }

  void File::unmap(){
    if (output_is_used(hash))
      throw std::invalid_argument("File is in use by other experiment");

      // std::cout << "unlink " << boost::uuids::to_string(experiment) << " " << scas::Hash::convert_hash_to_string(hash) << std::endl;
    // FIXME strict checking: if input => experiment can not have outputs

    db->query(R"(
         DELETE FROM file_map WHERE hash = ? AND uuid = ?;
       )",
         Sql::vec_sql_t{bin_to_blob(hash), bin_to_blob(experiment)});

    hash.fill(0);
    size = 0;
    ctime = std::optional<ctime_t>();
    original_name = std::optional<std::string>();
    creator_uuid = std::optional<uuids::uuid>();
    owner = std::optional<std::string>();
    metadata = std::optional<std::string>();
    role = std::optional<role_t>();
  }

  void File::change_role(role_t set_role){
    if (role == role_t::output) {
      if (output_is_used(hash))
        throw std::invalid_argument("File is in use by other experiment");
    }

    db->query(R"(
         UPDATE file_map
         SET role = ?
         WHERE hash = ? AND uuid = ?;
       )",
         Sql::vec_sql_t{int(set_role), bin_to_blob(hash), bin_to_blob(experiment)});

  }

  void File::update(){
    // owner, meta, role
    db->query(R"(
         UPDATE files SET name = ?, owner = ?, metadata = ? WHERE hash = ?;
       )",
       Sql::vec_sql_t{Sql::optional_null(original_name),
                      Sql::optional_null(owner),
                      Sql::optional_null(metadata),
                      bin_to_blob(hash)});

  }

  std::vector<File> File::list(std::optional<role_t> role){
    std::optional<Sql::vec_sql_opt_t> res;
    if (role) {
      res = db->query(R"(
          SELECT hash, path FROM file_map
          WHERE uuid = ? AND role = ?
          ORDER BY role;
        )",
         Sql::vec_sql_t{bin_to_blob(experiment), int(*role)},
         Sql::vec_sql_t{bin_to_blob(scas::Hash::hash_t()), ""});
    } else {
      res = db->query(R"(
          SELECT hash, path FROM file_map
          WHERE uuid = ?
          ORDER BY role;
        )",
         Sql::vec_sql_t{bin_to_blob(experiment)},
         Sql::vec_sql_t{bin_to_blob(scas::Hash::hash_t()), ""});
    }

    std::vector<std::tuple<scas::Hash::hash_t, std::optional<std::string>>> hash_list;
    std::vector<File> files;

    while (res){
      auto row = *res;
      hash_list.push_back(std::tuple<scas::Hash::hash_t, std::optional<std::string>>(
            blob_to_bin<scas::Hash::hash_t>(std::get<Sql::blob_t>(*row[0])),
            Sql::sql_repack_optional<std::string>(row[1])));
      res = db->next_row();
    }


    for (auto hashi : hash_list) {
      files.push_back(File(db, experiment, std::get<0>(hashi)));
      files.back().path = std::get<1>(hashi);
    }

    return files;
  }

  FileTree File::track(int depth, int max_depth){
    FileTree tree(*this);
    if (depth > max_depth) return tree;
    if (!role) return tree;

    //FIXME handle all other roles correctly

    if (role == role_t::output) { // process dependencies
      auto list_inputs = list(role_t::input);

      for (auto f : list_inputs) {
        tree.children.push_back(f.track(depth + 1));
      }
    } else if (role == role_t::input) { // process inputs
      if (creator_uuid) {
        auto file_list = File(db, *creator_uuid).list(role_t::input);

        for (auto f : file_list){
          if (f.hash != hash){
            tree.children.push_back(f.track(depth + 1));
          }
        }
      }
    }

    return tree;
  }

  std::vector<File> File::get_all_files(){
    auto res = db->query(R"(
        SELECT
          files.hash, files.size, files.name, files.creator, files.owner, files.ctime, files.metadata, file_map.path, file_map.role, file_map.uuid
        FROM file_map
        JOIN files ON file_map.hash = files.hash;
      )",
       Sql::vec_sql_t{},
       Sql::vec_sql_t{bin_to_blob(hash),        // hash
                      int64_t(0),               // size
                      std::string(),            // name
                      bin_to_blob(uuids::random_generator()()),  // creator
                      std::string(),            // owner
                      int64_t(0),               // ctime
                      std::string(),            // metadata
                      std::string(),            // path
                      int(0),                   // role
                      bin_to_blob(uuids::random_generator()())
                      });

    std::vector<File> files;

    while (res) {
      auto row = *res;
      File f(db);

      f.hash = blob_to_bin<scas::Hash::hash_t>(std::get<Sql::blob_t>(*row[0]));
      f.size = std::get<int64_t>(*row[1]);
      f.original_name = Sql::sql_repack_optional<std::string>(row[2]);
      f.creator_uuid = Sql::sql_repack_optional<Sql::blob_t, uuids::uuid>(row[3], blob_to_bin<uuids::uuid>);
      f.owner = Sql::sql_repack_optional<std::string>(row[4]);
      f.ctime = Sql::sql_repack_optional<int64_t>(row[5]);
      f.metadata = Sql::sql_repack_optional<std::string>(row[6]);
      f.path = Sql::sql_repack_optional<std::string>(row[7]);
      f.role = Sql::sql_repack_optional<int, role_t>(row[8], [](int in){return role_t(in);});
      f.experiment = blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[9]));

      files.push_back(f);

     res = db->next_row();
    }

    return files;
  }
}
