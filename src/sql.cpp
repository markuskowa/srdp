#include "sql.h"
#include <cstring>
#include <iostream>
#include <assert.h>


namespace srdp {

  template<>
  Sql::blob_t bin_to_blob<uuids::uuid>(const uuids::uuid& uuid){
    if (uuid.is_nil())
      throw std::invalid_argument("UUID is nil. Can not convert to blob.");

    Sql::blob_t blob(uuid.size());
    std::memcpy(blob.data(), &uuid, uuid.size());
    return blob;
  }

  Sql::Sql(const fs::path& dbfile) : stmt(nullptr), db(nullptr) {
    int ret = sqlite3_open(std::string(dbfile).c_str(), &db);

    if (ret != SQLITE_OK || db == nullptr) {
      std::string msg(sqlite3_errmsg(db));
      db = nullptr;
      throw std::runtime_error("Can not open sqlite database: "  + msg);
    }

    query("PRAGMA foreign_keys = 1;");
  }

  Sql::~Sql(){
    sqlite3_finalize(stmt);
    stmt = nullptr;
    flush();
    sqlite3_close(db);
    db = nullptr;
  }

  void Sql::db_error(const std::string& msg){
    sqlite3_finalize(stmt);
    stmt = nullptr;
    std::string sql_err(sqlite3_errmsg(db));
    throw std::runtime_error("SQlite error: " + sql_err);
  }

  void Sql::db_error(const std::string& msg, char* errmsg){
    std::string sql_err(errmsg);
    sqlite3_free(errmsg);
    throw std::runtime_error("SQlite error: " + msg + "; " + sql_err);
  }

  void Sql::exec(const std::string& sql){
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK)
      db_error("exec failed", errmsg);
  }

  void Sql::prepare(const std::string& sql){
    if (stmt != nullptr) finalize();

    int ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK)
      db_error("prepare failed");
  };

  bool Sql::step_row(){
    if (stmt == nullptr)
      throw std::runtime_error("No open SQL Query");

    int res = sqlite3_step(stmt);
    return res == SQLITE_ROW;
  }

  int Sql::step(){
    if (stmt == nullptr)
      throw std::runtime_error("No open SQL Query");

    return sqlite3_step(stmt);
  }

  int Sql::column_count(){
    if (stmt == nullptr)
      throw std::runtime_error("No open SQL query");

    return sqlite3_column_count(stmt);
  }

  void Sql::finalize(){
    if (stmt != nullptr) {
      if (sqlite3_finalize(stmt) != SQLITE_OK){
        stmt = nullptr;
        throw std::runtime_error("sqlite3_finalize failed");
      }
    }
    stmt = nullptr;
  }

  void Sql::reset(){
    if (stmt != nullptr) {
      if (sqlite3_reset(stmt) != SQLITE_OK){
        stmt = nullptr;
        throw std::runtime_error("sqlite3_reset failed");
      }
    }
  }

  void Sql::flush(){
    sqlite3_db_cacheflush(db);
  }

  void Sql::clear_bindings(){
    if (stmt != nullptr) {
      if (sqlite3_clear_bindings(stmt) != SQLITE_OK){
        stmt = nullptr;
        throw std::runtime_error("sqlite3_clear_bindings failed");
      }
    }
  }

  void Sql::bind_int(int column, int value){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_int(stmt, column, value);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_int failed");
  }

  void Sql::bind_int64(int index, int64_t value){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_int64(stmt, index, value);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_int64 failed");
  }

  void Sql::bind_str(int index, const std::string& value){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_text(stmt, index, value.data(), value.length(), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_text failed");
  }

  void Sql::bind_blob(int index, const std::vector<unsigned char>& value){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_blob(stmt, index, reinterpret_cast<const char*>(value.data()), value.size(), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_blob failed");
  }

  void Sql::bind_null(int index){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_null(stmt, index);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_null failed");

  }

  void Sql::bind_bool(int index, bool value){
    if (stmt == nullptr)
      throw std::runtime_error("No open query");

    const int rc = sqlite3_bind_int(stmt, index, value);
    if (rc != SQLITE_OK)
      db_error("sqlite3_bind_int failed");
  }

  std::optional<int> Sql::get_column_int(int column){
    if (column_count() < column) return {};

    int nbytes = sqlite3_column_bytes(stmt, column);
    if (nbytes == 0) return {};

    return sqlite3_column_int(stmt, column);
  }

  std::optional<int64_t> Sql::get_column_int64(int column){
    if (column_count() < column) return {};

    int nbytes = sqlite3_column_bytes(stmt, column);
    if (nbytes == 0) return {};

    return sqlite3_column_int64(stmt, column);
  }

  std::optional<std::string> Sql::get_column_str(int column){
    if (column_count() < column) return {};
    int nbytes = sqlite3_column_bytes(stmt, column);
    if (nbytes == 0) return {};
    std::string res(nbytes, '\0');
    std::memcpy(res.data(), sqlite3_column_text(stmt, column), nbytes);

    return res;
  }

  std::optional<std::vector<unsigned char>> Sql::get_column_blob(int column){
    if (column_count() < column) return {};
    int nbytes = sqlite3_column_bytes(stmt, column);
    if (nbytes == 0) return {};
    std::vector<unsigned char> res(nbytes);
    std::memcpy(res.data(), sqlite3_column_blob(stmt, column), nbytes);

    return res;
  }

  std::optional<bool> Sql::get_column_bool(int column){
    if (column_count() < column) return {};
    int nbytes = sqlite3_column_bytes(stmt, column);
    if (nbytes == 0) return {};

    return sqlite3_column_int(stmt, column);
  }

  std::optional<Sql::vec_sql_opt_t> Sql::query(const std::string& sql_query, const vec_sql_t& bindings, const vec_sql_t& result_types){
    finalize(); // clear any previous statement

    prepare(sql_query);

    // Bind all parameters
    for (size_t i=0; i < bindings.size(); i++){
      auto bind = bindings[i];
      if (std::holds_alternative<int>(bind))
        bind_int(i+1, std::get<int>(bind));
      else if (std::holds_alternative<int64_t>(bind))
        bind_int64(i+1, std::get<int64_t>(bind));
      else if (std::holds_alternative<bool>(bind))
        bind_bool(i+1, std::get<bool>(bind));
      else if (std::holds_alternative<std::string>(bind))
        bind_str(i+1, std::get<std::string>(bind));
      else if (std::holds_alternative<blob_t>(bind))
        bind_blob(i+1, std::get<blob_t>(bind));
      else if (std::holds_alternative<null_value>(bind))
        bind_null(i+1);
      else assert (false); // should never happen
    }

    if (result_types.size() > 0)
      row_result_types = result_types;

    return next_row();
  }

  std::optional<Sql::vec_sql_opt_t> Sql::next_row(){
    if (step_row()) {
      if (row_result_types.size() > 0 && column_count() != row_result_types.size())
        throw std::invalid_argument("Number of columns expected do not match number of columns");

      vec_sql_opt_t row_results(row_result_types.size());

      for (size_t i=0; i < row_result_types.size(); i++){
        auto type = row_result_types[i];
        if (std::holds_alternative<int>(type))
          row_results[i] = get_column_int(i);
        else if (std::holds_alternative<int64_t>(type))
          row_results[i] = get_column_int64(i);
        else if (std::holds_alternative<bool>(type))
          row_results[i] = get_column_bool(i);
        else if (std::holds_alternative<std::string>(type))
          row_results[i] = get_column_str(i);
        else if (std::holds_alternative<blob_t>(type))
          row_results[i] = get_column_blob(i);
        else assert (false); // should never happen
      }

      return std::optional<vec_sql_opt_t>(row_results);
    }

    row_result_types.resize(0);
    finalize();
    return std::optional<vec_sql_opt_t>{};
  }
}
