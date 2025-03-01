// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <catch2/catch_test_macros.hpp>

#include "config.h"

const std::filesystem::path db_path("testc.db");

TEST_CASE("Config Interface", "[config]"){
  std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);
  REQUIRE_NOTHROW( srdp::Config::create_table(*db) );

  srdp::Config config(db);

  REQUIRE_NOTHROW( config.set_string("stringval", "str") );
  REQUIRE( config.get_string("stringval") == "str" );
  REQUIRE( config.get_uuid("stringval").is_nil() );

  auto uuid = srdp::uuids::random_generator()();

  REQUIRE_NOTHROW( config.set_uuid("uuidval", uuid) );
  REQUIRE( config.get_uuid("uuidval") == uuid );
  REQUIRE( config.get_string("uuidval") == "" );

  std::filesystem::remove(db_path);
}
