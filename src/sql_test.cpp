// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include "sql.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("One-shot interface","[sql]") {
  srdp::Sql db("oneshot.db");

  const std::string sql_create = R"(
    CREATE TABLE IF NOT EXISTS mytable (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            string TEXT NOT NULL,
            int INTEGER,
            blob BLOB(32),
            empty INTEGER DEFAULT NULL,
            bool BOOLEAN DEFAULT TRUE
        );
  )";

  const std::string sql_insert = R"(
    INSERT INTO mytable (string, int, blob) VALUES ('Alice', 50000000000, X'00aaff');
  )";

  const std::string sql_count = R"(
    SELECT COUNT(*) FROM mytable;
  )";

  const std::string sql_select = R"(
    SELECT id, string, int, blob, empty, bool FROM mytable;
  )";

  REQUIRE_NOTHROW( db.exec(sql_create) );
  REQUIRE_NOTHROW( db.exec(sql_insert) );

  // Count number of rows
  REQUIRE_NOTHROW( db.prepare(sql_count) );

  bool row_result = false;
  REQUIRE_NOTHROW( row_result = db.step_row() );
  REQUIRE( row_result );
  REQUIRE( db.column_count() == 1 );
  const auto num_rows = db.get_column_int(0);
  REQUIRE ( num_rows );
  REQUIRE ( *num_rows == 1 );

  // Check entries
  REQUIRE_NOTHROW( db.prepare(sql_select) );

  REQUIRE_NOTHROW( row_result = db.step_row() );
  REQUIRE( row_result );
  REQUIRE( db.column_count() == 6 );

  // Invalid column index should return an empty value
  REQUIRE ( !db.get_column_int(10) );
  REQUIRE ( !db.get_column_int64(10) );
  REQUIRE ( !db.get_column_blob(10) );
  REQUIRE ( !db.get_column_str(10) );
  REQUIRE ( !db.get_column_bool(10) );

  const auto r_id = db.get_column_int(0);
  const auto r_string = db.get_column_str(1);
  const auto r_int = db.get_column_int64(2);
  const auto r_blob = db.get_column_blob(3);
  const auto r_empty = db.get_column_int(4);
  const auto r_bool = db.get_column_bool(5);

  REQUIRE ( r_id );
  REQUIRE ( *r_id == 1 );
  REQUIRE ( r_string );
  REQUIRE ( *r_string == "Alice" );
  REQUIRE ( r_int );
  REQUIRE ( *r_int == 50000000000 );
  REQUIRE ( r_blob );
  REQUIRE ( (*r_blob)[0] == 0x00 );
  REQUIRE ( (*r_blob)[1] == 0xaa );
  REQUIRE ( (*r_blob)[2] == 0xff );
  REQUIRE ( !r_empty );
  REQUIRE ( *r_bool );

  REQUIRE_NOTHROW ( db.finalize() );

  std::filesystem::remove("oneshot.db");
}

TEST_CASE("Binding", "[sql]"){
  srdp::Sql db("binding.db");

  const std::string sql_create = R"(
    CREATE TABLE IF NOT EXISTS mytable (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            string TEXT NOT NULL
        );
  )";

  const std::string sql_insert = R"(
    INSERT INTO mytable (string) VALUES (?);
  )";

  const std::string sql_select = R"(
    SELECT id, string FROM mytable WHERE string = ?;
  )";

  REQUIRE_NOTHROW( db.exec(sql_create) );

  REQUIRE_NOTHROW( db.prepare(sql_insert) );

  REQUIRE_NOTHROW( db.bind_str(1, "Alice") );
  REQUIRE_NOTHROW( db.step() );

  REQUIRE_NOTHROW( db.reset( ));

  REQUIRE_NOTHROW( db.bind_str(1, "Bob") );
  REQUIRE_NOTHROW( db.step() );

  REQUIRE_NOTHROW( db.finalize( ));

  REQUIRE_NOTHROW( db.prepare(sql_select) );
  REQUIRE_NOTHROW( db.bind_str(1, "Bob") );
  REQUIRE_NOTHROW( db.step() );

  const auto r_id = db.get_column_int(0);
  const auto r_string = db.get_column_str(1);

  REQUIRE( r_id );
  REQUIRE( *r_id == 2 );
  REQUIRE( r_string );
  REQUIRE( *r_string == "Bob" );


  std::filesystem::remove("binding.db");
}

TEST_CASE("High level query", "[sql]"){
  srdp::Sql db("highlevel.db");

  const std::string sql_create = R"(
    CREATE TABLE IF NOT EXISTS mytable (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            string TEXT NOT NULL
        );
  )";

  const std::string sql_insert = R"(
    INSERT INTO mytable (string) VALUES (?);
  )";

  const std::string sql_select = R"(
    SELECT id, string FROM mytable WHERE string = ?;
  )";

  db.query(sql_create);
  db.query(sql_insert, srdp::Sql::vec_sql_t{"Alice"});
  db.query(sql_insert, srdp::Sql::vec_sql_t{"Bob"});

  auto res = db.query(sql_select, srdp::Sql::vec_sql_t{"Alice"}, srdp::Sql::vec_sql_t{int(0), std::string()});
  REQUIRE( res );
  REQUIRE( (*res)[0] );
  REQUIRE( std::get<int>(*((*res)[0])) == 1);
  REQUIRE( (*res)[1] );
  REQUIRE( std::get<std::string>(*((*res)[1])) == "Alice");

  REQUIRE ( !db.next_row() );

  std::filesystem::remove("highlevel.db");
}
