// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <catch2/catch_test_macros.hpp>

#include "files.h"

const std::filesystem::path db_path("teste.db");
const std::string project_name("proje");
const std::string experiment_name("exp");


TEST_CASE("Experiment Constructors", "[experiment]") {
  std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);

  REQUIRE_NOTHROW( srdp::File::create_table(*db) );
  REQUIRE_NOTHROW( srdp::Experiment::create_table(*db) );
  REQUIRE_NOTHROW( srdp::Project::create_table(*db) );

  srdp::Project prj(db, project_name);

  // Experiment(std::shared_ptr<Sql>& dbin, const Project& project, const std::string& name, bool create = true);
  srdp::Experiment e1(db, prj, experiment_name, true);
  REQUIRE( !e1.uuid.is_nil() );
  REQUIRE( e1.project == prj.uuid );
  REQUIRE( e1.name == experiment_name );
  REQUIRE( !e1.metadata );
  REQUIRE( !e1.owner );
  REQUIRE( !e1.ctime );
  REQUIRE( !e1.locked );

  srdp::Experiment e2(db, prj, experiment_name, false);
  REQUIRE( !e2.uuid.is_nil() );
  REQUIRE( e2.project == prj.uuid );
  REQUIRE( e2.name == experiment_name );
  REQUIRE( !e2.metadata );
  REQUIRE( !e2.owner );
  REQUIRE( !e2.ctime );
  REQUIRE( !e2.locked );

  // Experiment(std::shared_ptr<Sql>& dbin, const Project& project, const uuids::uuid& uuid);
  srdp::Experiment e3(db, prj, e1.uuid);
  REQUIRE( !e3.uuid.is_nil() );
  REQUIRE( e3.project == prj.uuid );
  REQUIRE( e3.name == experiment_name );
  REQUIRE( !e3.metadata );
  REQUIRE( !e3.owner );
  REQUIRE( !e3.ctime );
  REQUIRE( !e3.locked );

  // Experiment(std::shared_ptr<Sql>& dbin, const Project& project);
  srdp::Experiment e4(db, prj);
  REQUIRE( e4.uuid.is_nil() );
  REQUIRE( e4.project == prj.uuid );
  REQUIRE( e4.name == "" );
  REQUIRE( !e4.metadata );
  REQUIRE( !e4.owner );
  REQUIRE( !e4.ctime );
  REQUIRE( !e4.locked );

  // Experiment(std::shared_ptr<Sql>& dbin, uuids::uuid& project_uuid);
  srdp::Experiment e5(db, prj.uuid);
  REQUIRE( e5.uuid.is_nil() );
  REQUIRE( e5.project == prj.uuid );
  REQUIRE( e5.name == "" );
  REQUIRE( !e5.metadata );
  REQUIRE( !e5.owner );
  REQUIRE( !e5.ctime );
  REQUIRE( !e5.locked );

  std::filesystem::remove(db_path);
}


SCENARIO("Read/Write interface", "[experiment]") {
  GIVEN("An project with one experiment") {
    std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);

    REQUIRE_NOTHROW( srdp::File::create_table(*db) );
    REQUIRE_NOTHROW( srdp::Experiment::create_table(*db) );
    REQUIRE_NOTHROW( srdp::Project::create_table(*db) );

    srdp::Project prj(db, project_name);

    THEN("Can create"){
      srdp::Experiment e1(db, prj);
      e1.create(experiment_name);

      srdp::Experiment e2(db, prj, experiment_name);
      REQUIRE( e2.uuid == e1.uuid );
      REQUIRE( e2.project == prj.uuid );
      REQUIRE( e2.name == experiment_name );
      REQUIRE( !e2.metadata );
      REQUIRE( !e2.owner );
      REQUIRE( !e2.ctime );
      REQUIRE( !e2.locked );

      // Make sure unique names are enforced:
      srdp::Experiment e3(db, prj);
      REQUIRE_THROWS( e3.create(experiment_name) );
      REQUIRE_NOTHROW( e3.create(experiment_name + "new") );
    }

    THEN("Can update"){
      srdp::Experiment e1(db, prj, experiment_name);

      e1.metadata = "meta";
      e1.owner = "user";
      e1.ctime = 1;
      e1.locked = true;
      e1.update();

      srdp::Experiment e2(db, prj, experiment_name);
      REQUIRE( e2.uuid == e1.uuid );
      REQUIRE( e2.project == prj.uuid );
      REQUIRE( e2.name == experiment_name );
      REQUIRE( e2.metadata );
      REQUIRE( *e2.metadata == "meta" );
      REQUIRE( e2.owner );
      REQUIRE( *e2.owner == "user" );
      REQUIRE( e2.ctime );
      REQUIRE( *e2.ctime == 1 );
      REQUIRE( e2.locked );
    }

    THEN("Can handle more than one experiment"){
      srdp::Experiment e1(db, prj, experiment_name);
      e1.create("second");

      std::vector<srdp::Experiment> list = e1.list();

      REQUIRE( list[0].uuid != list[1].uuid );
      REQUIRE( list[0].name != list[1].name );

      // FIXME order not guaranteed
      REQUIRE( list[0].name == experiment_name );
      REQUIRE( list[1].name == "second" );
    }

    THEN("Can handle journal"){
      srdp::Experiment e1(db, prj, experiment_name);

      REQUIRE ( e1.get_journal() == "");
      REQUIRE_NOTHROW( e1.set_journal("The journal") );
      REQUIRE( e1.get_journal() == "The journal" );
      REQUIRE_NOTHROW( e1.append_journal(" and more") );
      REQUIRE( e1.get_journal() == "The journal and more" );
    }

    THEN("Can remove"){
      srdp::Experiment e1(db, prj, experiment_name);
      auto uuid = e1.uuid;

      // Deletion should be blocked
      REQUIRE_THROWS( prj.remove() );

      REQUIRE_NOTHROW( e1.remove() );
      REQUIRE_FALSE( e1.uuid.is_nil() );
      REQUIRE( e1.name == "" );
      REQUIRE_FALSE( e1.metadata );
      REQUIRE_FALSE( e1.owner );
      REQUIRE_FALSE( e1.ctime );
      REQUIRE_FALSE( e1.locked );

      REQUIRE_THROWS( e1.load(uuid) );
    }

    std::filesystem::remove(db_path);
  }
}
