#include <iostream>
#include <fstream>
#include <catch2/catch_test_macros.hpp>

#include "srdp.h"

const std::string base_dir = "test_srdp";
const std::string project_name = "project_name";
const std::string experiment_name = "experiment_name";
const std::string file_name = "file_name";

void helper_create_file(const std::string& ofname, const std::string& ofcontent){
  std::ofstream ofile(ofname, std::ios::trunc);
  ofile << ofcontent;
  ofile.close();
}

SCENARIO("SRDP management", "[srdp]") {
  GIVEN("Fully automatic detection"){

    auto old_cwd = fs::current_path();

    fs::create_directory(base_dir);
    fs::current_path(base_dir);

    REQUIRE_NOTHROW( srdp::Srdp::init("./") );
    REQUIRE( fs::exists( srdp::Srdp::cfg_dir ));
    REQUIRE( fs::exists( srdp::Srdp::cfg_dir / srdp::Srdp::db_file ));
    REQUIRE( fs::exists( srdp::Srdp::cfg_dir / srdp::Srdp::default_store_dir ));


    WHEN("Project is present") {
      srdp::Srdp dp;
      REQUIRE_NOTHROW( dp.create_project(project_name) );

      THEN("Can act on project"){
        // open
        REQUIRE( dp.open_project().name == project_name );
        REQUIRE( dp.open_project(project_name).name == project_name );

        auto prj = dp.open_project();
        REQUIRE( dp.open_project(srdp::uuids::to_string(prj.uuid)).uuid == prj.uuid );

        // list
        auto prj_list = dp.get_project();
        REQUIRE( prj_list.list().size() == 1 );

        // remove
        REQUIRE_NOTHROW( dp.remove_project(project_name) );

        REQUIRE_NOTHROW( dp.create_project(project_name) );
        prj = dp.open_project(project_name);
        REQUIRE_NOTHROW( dp.remove_project(srdp::uuids::to_string(prj.uuid)) );
      }

      THEN ("Can act on experiment") {
        REQUIRE_NOTHROW( dp.create_experiment(experiment_name) );

        // Double creation with same name should not be possible
        REQUIRE_NOTHROW( dp.create_experiment(experiment_name) );

        // list
        auto exp = dp.get_experiment();
        REQUIRE( exp.list().size() == 1 );

        // load
        REQUIRE_NOTHROW( exp = dp.open_experiment() );
        REQUIRE_NOTHROW( dp.open_experiment(experiment_name).uuid == exp.uuid );
        REQUIRE_NOTHROW( dp.open_experiment(srdp::uuids::to_string(exp.uuid)).uuid == exp.uuid );

        // remove
        REQUIRE_NOTHROW( dp.remove_experiment(experiment_name) );
      }

      THEN("Can handle files") {
        REQUIRE_NOTHROW( dp.create_experiment(experiment_name) );

        scas::Store store(srdp::Srdp::cfg_dir / srdp::Srdp::default_store_dir);
        const std::string f1i = file_name + "1i";
        const std::string f1o = file_name + "1o";

        helper_create_file(f1i, f1i);
        helper_create_file(f1o, f1o);

        REQUIRE_NOTHROW( dp.add_file("", "", f1i, srdp::File::role_t::input) );
        REQUIRE_NOTHROW( dp.add_file("", "", f1o, srdp::File::role_t::output) );
        REQUIRE( store.file_is_in_store(f1i) );
        REQUIRE( store.file_is_in_store(f1o) );

        // Double check relative vs. absolute link? Should be fixed in scas!

        REQUIRE_NOTHROW( dp.create_experiment(experiment_name + "2") );

        const std::string f2i = file_name + "2i";
        const std::string f2o = file_name + "2o";
        helper_create_file(f2i, f2i);
        helper_create_file(f2o, f2o);

        // add linked file to second experiment
        REQUIRE_NOTHROW( dp.add_file("", "", f1o, srdp::File::role_t::input) );
        REQUIRE_NOTHROW( dp.add_file("", "", f2i, srdp::File::role_t::input) );
        REQUIRE_NOTHROW( dp.add_file("", "", f2o, srdp::File::role_t::output) );
        REQUIRE( store.file_is_in_store(f1o) );
        REQUIRE( store.file_is_in_store(f2i) );
        REQUIRE( store.file_is_in_store(f2o) );

        // Can load file by path and hash
        auto file = dp.get_file();
        REQUIRE_NOTHROW( file = dp.load_file("", "", f2o) );
        REQUIRE( file.path == f2o );
        REQUIRE_NOTHROW( file = dp.load_file("", "", scas::Hash::convert_hash_to_string(file.hash)) );
        REQUIRE( file.path == f2o );

        // make sure we can not remove dependencies
        REQUIRE_THROWS( dp.unlink_file(project_name, experiment_name, f1i) );
        REQUIRE( store.file_is_in_store(f1i) );
        REQUIRE_THROWS( dp.unlink_file(project_name, experiment_name, f1o) );
        REQUIRE( store.file_is_in_store(f1o) );

        // Check proper unlinking
        REQUIRE_NOTHROW( dp.unlink_file(project_name, experiment_name + "2", f1o) );
        REQUIRE( store.file_is_in_store(f1o) );

        REQUIRE_NOTHROW( dp.unlink_file(project_name, experiment_name + "2", f2o) );
        REQUIRE_FALSE( store.file_is_in_store(f2o) );
      }
    }

    fs::current_path(old_cwd);
    fs::remove_all(base_dir);
  }
}

// project:
// - create
// - load by UUID/name/config
// - remove by UIID/name

// experiment
// - create
// - Enforece unique names
// - load by UUID/name/config
// - remove -"-

// file
// add, integrity of paths
// remove, keep input integrity
// linkage o -> i, proper name recognition
