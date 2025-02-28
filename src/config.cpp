#include "config.h"

namespace srdp {
  Config::Config(const std::shared_ptr<Sql>& dbin) : db(dbin)
  {
    if (!db)
      throw std::runtime_error("Invalid DB pointer.");
  }

  void Config::create_table(Sql& db){
    db.query(R"(
      CREATE TABLE IF NOT EXISTS config (
        name  VARCHAR(32) NOT NULL PRIMARY KEY,
        value_blob BLOB(32),
        value_string VARCHAR(32)
      );
    )");
  }

  uuids::uuid Config::get_uuid(const std::string& key){
    uuids::uuid uuid;
    auto res = db->query("SELECT value_blob FROM config WHERE name = ?;",
               Sql::vec_sql_t{key},
               Sql::vec_sql_t{bin_to_blob(uuids::random_generator()())});

    if (res){
      auto row = *res;
      if (!row[0])
        return uuid;

      uuid =  blob_to_bin<uuids::uuid>(std::get<Sql::blob_t>(*row[0]));
    }

    return uuid;
  }

  void Config::set_uuid(const std::string& key, const uuids::uuid& uuid){
    db->query("INSERT OR REPLACE INTO config (name, value_blob, value_string) VALUES (?, ?, NULL);",
              Sql::vec_sql_t{key, bin_to_blob(uuid)});
  }

  std::string Config::get_string(const std::string& key){
    std::string str;
    auto res = db->query("SELECT value_string FROM config WHERE name = ?;",
               Sql::vec_sql_t{key},
               Sql::vec_sql_t{key});

    if (res){
      auto row = *res;
      if (!row[0]) return str;
      if (!std::holds_alternative<std::string>(*row[0])) return str;

      str =  std::get<std::string>(*row[0]);
    }

    return str;
  }

  void Config::set_string(const std::string& key, const std::string& value){
    db->query("INSERT OR REPLACE INTO config (name, value_string, value_blob) VALUES (?, ?, NULL);",
              Sql::vec_sql_t{key, value});
  }

}
