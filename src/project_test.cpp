#include <iostream>
#include <catch2/catch_test_macros.hpp>

#include "files.h"

const std::filesystem::path db_path("test.db");
const std::string project_name("new_proj");

TEST_CASE("Project Constructors", "[project]") {
  std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);

  REQUIRE_NOTHROW( srdp::File::create_table(*db) );
  REQUIRE_NOTHROW( srdp::Experiment::create_table(*db) );
  REQUIRE_NOTHROW( srdp::Project::create_table(*db) );

  srdp::Project project1(db, project_name, true);

  REQUIRE( project1.name == project_name );
  REQUIRE( !project1.uuid.is_nil() );
  auto uuid = project1.uuid;

  srdp::Project project2(db, project_name, false);
  REQUIRE( project2.name == project_name );
  REQUIRE( !project2.uuid.is_nil() );
  REQUIRE( project2.uuid == uuid );

  srdp::Project project3(db, uuid);
  REQUIRE( project3.name == project_name );
  REQUIRE( project3.uuid == uuid );

  srdp::Project project4(db);
  REQUIRE( project4.uuid.is_nil() );

  std::filesystem::remove(db_path);
}

SCENARIO("Read/Write interface", "[project]") {

  GIVEN("An project table with one project") {
    std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);
    REQUIRE_NOTHROW( srdp::Project::create_table(*db) );

    srdp::Project project1(db, project_name);
    REQUIRE( !project1.uuid.is_nil() );
    REQUIRE( project1.name == project_name );
    REQUIRE( !project1.metadata );
    REQUIRE( !project1.owner );
    REQUIRE( !project1.ctime );

    THEN("Can update and reload") {
      project1.metadata = "meta";
      project1.owner = "theowner";
      project1.ctime = 1;
      REQUIRE_NOTHROW( project1.update() );

      srdp::Project project2(db);
      REQUIRE_NOTHROW( project2.load(project_name) );
      REQUIRE( *project2.metadata == "meta");
      REQUIRE( *project2.owner == "theowner");
      REQUIRE( *project2.ctime == 1 );

      srdp::Project project3(db);
      REQUIRE_NOTHROW( project3.load(project1.uuid) );
      REQUIRE( *project3.metadata == "meta");
      REQUIRE( *project3.owner == "theowner");
      REQUIRE( *project3.ctime == 1 );
    }

    THEN("Can handle more than 1 project"){
      REQUIRE_NOTHROW( project1.create("Another project") );
      REQUIRE( !project1.uuid.is_nil() );
      REQUIRE( project1.name == "Another project" );
      REQUIRE( !project1.metadata );
      REQUIRE( !project1.owner );
      REQUIRE( !project1.ctime );

      srdp::Project project2(db, "Another project");
      REQUIRE( project1.uuid == project2.uuid );
      REQUIRE( project1.name == project2.name );

      auto list = project1.list();
      REQUIRE( list.size() == 2);
      REQUIRE( list[0].name != list[1].name );
      REQUIRE( list[0].uuid != list[1].uuid );

      // FIXME: order is not guaranteed
      REQUIRE( list[0].name == project_name );
      REQUIRE( list[1].name == "Another project" );
    }

    THEN("Properly fails loads"){

      // Failed searches
      REQUIRE_THROWS( project1.load("Invalid name") );
      REQUIRE_THROWS( project1.load(srdp::uuids::random_generator()()) );
    }

    THEN("Can handle journal"){
      REQUIRE ( project1.get_journal() == "");
      REQUIRE_NOTHROW( project1.set_journal("The journal") );
      REQUIRE( project1.get_journal() == "The journal" );
      REQUIRE_NOTHROW( project1.append_journal(" and more") );
      REQUIRE( project1.get_journal() == "The journal and more" );
    }

    THEN("Can remove project"){
      auto uuid = project1.uuid;

      REQUIRE_NOTHROW( project1.remove() );
      REQUIRE( project1.name == "");
      REQUIRE_FALSE( project1.uuid.is_nil() );
      REQUIRE_FALSE( project1.metadata );
      REQUIRE_FALSE( project1.owner );
      REQUIRE_FALSE( project1.ctime );

      REQUIRE_THROWS( project1.load(uuid) );
    }

    std::filesystem::remove(db_path);
  }
}


