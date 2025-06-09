// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include <catch2/catch_test_macros.hpp>

#include "files.h"

const std::filesystem::path db_path("testf.db");
const std::string project_name("proje");
const std::string experiment_name("exp");

SCENARIO("Files in DB","[files]") {
  std::shared_ptr<srdp::Sql> db = std::make_shared<srdp::Sql>(db_path);

  REQUIRE_NOTHROW( srdp::Project::create_table(*db) );
  REQUIRE_NOTHROW( srdp::Experiment::create_table(*db) );
  REQUIRE_NOTHROW( srdp::File::create_table(*db) );

  srdp::Project prj(db, project_name);

  GIVEN("Experiment"){
    srdp::Experiment exp(db, prj, experiment_name);

    THEN("Files Constructor") {
      srdp::File file(db, exp);
      scas::Hash::hash_t hash;
      hash.fill(0);

      REQUIRE( file.experiment == exp.uuid );
      REQUIRE( file.hash == hash );
      REQUIRE( file.size == 0 );
    }

  }

  GIVEN("Blank File object") {
    srdp::Experiment exp(db, prj, experiment_name);
    srdp::File file(db, exp);
    scas::Hash hash;

    WHEN("File entry creation") {

      const std::string content("hash 1");
      const std::string name("my/file");
      const std::string path("the/file");
      const std::string owner("user");
      const std::string meta("text");

      hash.update(content);

      file.hash = hash.get_hash_binary();
      file.size = content.length();
      file.role = srdp::File::role_t::input;
      file.original_name = name;
      file.owner = owner;
      file.metadata = meta;
      file.path = path;

      REQUIRE( file.create() );

      THEN("Find file") {
        REQUIRE( file.exists(file.hash) );
        REQUIRE( file.is_mapped(file.hash) );
      }

      THEN("Can read back file data"){
        srdp::File file2(db, exp);

        file2.load(file.hash);
        REQUIRE ( file2.hash == file.hash );
        REQUIRE ( file2.size == content.length() );
        REQUIRE ( file2.role );
        REQUIRE ( *file2.role == *file.role );
        REQUIRE_FALSE( file2.creator_uuid );  // root input can not have a creator
        REQUIRE( file2.original_name );
        REQUIRE( *file2.original_name == name );
        REQUIRE( file2.path );
        REQUIRE( *file2.path == path );
        REQUIRE( file2.owner );
        REQUIRE( *file2.owner == owner );
        REQUIRE( file2.metadata );
        REQUIRE( *file2.metadata == meta );
      };

      THEN("Can load from constructor"){
        srdp::File file2(db, exp, file.hash);
        REQUIRE ( file2.hash == file.hash );
        REQUIRE ( file2.size == content.length() );
      }

      THEN("Can modify entry") {
        file.original_name = "new name";
        file.owner = "new owner";
        file.metadata = "new meta";
        file.update();

        srdp::File file2(db, exp);
        file2.load(file.hash);

        REQUIRE( file2.original_name == "new name" );
        REQUIRE( file2.owner == "new owner" );
        REQUIRE( file2.metadata == "new meta" );
      }

      THEN("Can load list"){
        srdp::File file2(db, exp);

        hash.update("hash 2");

        file2.hash = hash.get_hash_binary();
        file2.size = content.length();
        file2.role = srdp::File::role_t::output;
        file2.create();

        std::vector<srdp::File> list = file.list();
        //
        // // FIXME order is not guaranteed
        CHECK( list[0].hash == file.hash );
        CHECK( list[1].hash == file2.hash );
      }
    }
  }

  GIVEN("Multiple Experiments") {
    scas::Hash hash;

    std::vector<std::vector<srdp::File>> files(2);
    for (int j=0; j < 2; j++){
      srdp::Experiment exp1(db, prj, experiment_name + std::to_string(j));

      for (int i=0; i < 2; i++){
        srdp::File f(db, exp1);
        hash.update("file" + std::to_string(j) + std::to_string(i));
        f.hash = hash.get_hash_binary();
        f.size = 1;
        if (i==0)
          f.role = srdp::File::role_t::input;
        else
          f.role = srdp::File::role_t::output;
        f.create();

        files[j].push_back(f);
      }
    }

    // initial inputs must not have creator_uuid
    REQUIRE_FALSE( files[0][0].creator_uuid );
    REQUIRE( files[0][1].creator_uuid );

    WHEN("Files are indpendent") {

      THEN("Can unmap input"){
        REQUIRE_NOTHROW( files[0][1].unmap() );
        REQUIRE_NOTHROW( files[0][0].unmap() );
      }

      THEN("Experiment deletion is blocked"){
        REQUIRE_THROWS( srdp::Experiment(db, prj, files[0][0].experiment).remove() );
      }
    }

    WHEN("Files are linked") {

      srdp::File linked_file(db, files[1][0].experiment);

      linked_file.hash = files[0][1].hash;
      linked_file.size = 1;
      linked_file.role = srdp::File::role_t::input;
      REQUIRE_NOTHROW( linked_file.create() );

      REQUIRE( files[0][0].output_is_used(linked_file.hash) );

      THEN("Can unmap input of down stream job"){
        REQUIRE_NOTHROW( files[1][1].unmap() );
        REQUIRE_NOTHROW( files[1][0].unmap() );

        // Values are cleared
        REQUIRE( files[1][0].size == 0 );
        REQUIRE( !files[1][0].ctime );
      }

      THEN("Unmap is blocked by dependency"){
        REQUIRE_THROWS( files[0][1].unmap() );
        REQUIRE_THROWS( files[0][0].unmap() );
      }

      THEN("Can track heritage"){
        auto tree = files[1][1].track();

        // Tree:
        // [1][1]
        // -> [1][0]
        // -> linked_file==[0][1]
        //     -> [0][0]
        REQUIRE( tree.node.hash == files[1][1].hash );
        REQUIRE( tree.children.size() == 2 );
        REQUIRE( tree.children[0].node.hash == files[1][0].hash );
        REQUIRE( tree.children[1].node.hash == linked_file.hash );
        REQUIRE( tree.children[1].children.size() == 1 );
        REQUIRE( tree.children[1].children[0].node.hash == files[0][0].hash );
      }

      THEN ("Can list all files"){
        auto file_list = srdp::File(db).get_all_files();

        // 4 files + 1 linked
        REQUIRE( file_list.size() == 5 );
      }
    }
  }
  std::filesystem::remove(db_path);
}
