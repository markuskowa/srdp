// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_SQL_H
#define SRDP_SQL_H

#include <string>
#include <filesystem>
#include <vector>
#include <optional>
#include <variant>
#include <functional>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <sqlite3.h>


namespace srdp {

  namespace fs = std::filesystem;
  namespace uuids = boost::uuids;

  class Sql {
    public:
      class null_value {}; // Dummy to indicate NULL types
      using blob_t = std::vector<unsigned char>;
      using sql_t = std::variant<int, int64_t, bool, std::string, blob_t, null_value>;
      using vec_sql_t = std::vector<sql_t>;
      using vec_sql_opt_t = std::vector<std::optional<sql_t>>;

      template <class T>
      static std::optional<T> sql_repack_optional(const std::optional<sql_t>& value){
        if (value)
          return std::optional<T>(std::get<T>(*value));
        else
          return std::optional<T>();
      }

      template <class T>
      static sql_t optional_null(const std::optional<T>& value){
        if (value)
          return sql_t(*value);
        else
          return sql_t(null_value());
      }

      template <class Tin, class Tout>
      static std::optional<Tout> sql_repack_optional(const std::optional<sql_t>& value, std::function<Tout(const Tin&)> transform){
        if (value)
          return std::optional<Tout>(transform(std::get<Tin>(*value)));
        else
          return std::optional<Tout>();
      }

      template <class Tin, class Tout>
      static std::optional<Tout> sql_repack_optional(const Tin& value, std::function<Tout(const Tin&)> transform){
        if (value)
          return std::optional<Tout>(transform(*value));
        else
          return std::optional<Tout>();
      }

    private:
      sqlite3* db;
      sqlite3_stmt* stmt;
      vec_sql_t row_result_types;

      void db_error(const std::string& msg);
      void db_error(const std::string& msg, char* errmsg);

    public:
      Sql(const fs::path& dbfile);
      ~Sql();

      bool is_open() { return db; }
      // High level functions
      std::optional<Sql::vec_sql_opt_t> query(
          const std::string& sql_query,
          const vec_sql_t& bindings = vec_sql_t(0),
          const vec_sql_t& result_types = vec_sql_t(0));

      std::optional<vec_sql_opt_t> next_row();

      void exec(const std::string& sql);

      // Low level functions
      void prepare(const std::string& sql);

      bool step_row();
      int step();
      void finalize();
      void reset();
      void clear_bindings();
      void flush();
      int column_count();

      void bind_int(int index, int value);
      void bind_int64(int index, int64_t value);
      void bind_str(int index, const std::string& value);
      void bind_blob(int index, const std::vector<unsigned char>& value);
      void bind_null(int index);
      void bind_bool(int index, bool value);

      std::optional<int> get_column_int(int column);
      std::optional<int64_t> get_column_int64(int column);
      std::optional<std::string> get_column_str(int column);
      std::optional<std::vector<unsigned char>> get_column_blob(int column);
      std::optional<bool> get_column_bool(int column);

  };

  template<class T>
  T blob_to_bin(const Sql::blob_t& blob){
    T bin;

    if (bin.size() != blob.size())
      throw std::invalid_argument("Blob size mismatch. Conversion to UUID/hash not possible.");

    std::memcpy(&bin, blob.data(), bin.size());

    return bin;
  }

  template<class T>
  Sql::blob_t bin_to_blob(const T& bin){
    Sql::blob_t blob(bin.size());
    std::memcpy(blob.data(), bin.data(), bin.size());
    return blob;
  }

  template<>
  Sql::blob_t bin_to_blob<uuids::uuid>(const uuids::uuid& uuid);

}

#endif /* SRDP_SQL_H */
