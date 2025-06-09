// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_FILES_H
#define SRDP_FILES_H


#include "store.h"
#include "experiment.h"

namespace srdp {

  class FileTree;
  /**
   * Database connected file object
   */
  class File {
    private:
      std::shared_ptr<Sql> db;

    public:
      enum class role_t {
        none = 0,
        input = 1,
        output = 2,
        note = 3,
        program = 4,
        nixpath = 5
      };

      static void create_table(Sql& db);
      static std::string role_to_string(role_t role);
      static role_t string_to_role(const std::string& role);

      uuids::uuid experiment;

      // Accessible properties, can be updated
      scas::Hash::hash_t hash;
      size_t size = 0;
      std::optional<ctime_t> ctime;
      std::optional<std::string> original_name;
      std::optional<std::string> path;
      std::optional<uuids::uuid> creator_uuid;
      std::optional<std::string> owner;
      std::optional<std::string> metadata;
      std::optional<role_t> role;

      // Create and load file
      File(std::shared_ptr<Sql>& dbin, const Experiment& experiment, const scas::Hash::hash_t& hash);
      File(std::shared_ptr<Sql>& dbin, const uuids::uuid& experiment, const scas::Hash::hash_t& hash);

      // Create empty
      File(std::shared_ptr<Sql>& dbin);
      File(std::shared_ptr<Sql>& dbin, const Experiment& experiment);
      File(std::shared_ptr<Sql>& dbin, const uuids::uuid& experiment);

      bool exists(const scas::Hash::hash_t& hash);
      bool is_mapped(const scas::Hash::hash_t& hash);
      bool output_is_used(const scas::Hash::hash_t& hash);
      void load(const scas::Hash::hash_t& hash);
      void load(const std::string& path);
      std::string resolve_creator();

      /**
       * Create with given properties.
       * Returns true if hash did not exist yet.
       * In case file already exists in DB only mapping to experiment is set.
       */
      bool create();

      /**
       * Remove file mapping from experiment.
       * Fails if any of the experiment's outputs is used by another experiment.
       */
      void unmap();

      /**
       * Change role of file.
       * Fails if any of the experiment's outputs is used by another experiment.
       */
      void change_role(role_t set_role);

      /**
       * Update file properties.
       * Only applies to original_name, owner, metadata
       */
      void update();

      /**
       * List all files connected to experiment.
       */
      std::vector<File> list(std::optional<role_t> role = std::optional<role_t>());

      /**
       * Track files heritage
       */
      FileTree track(int depth = 0, int max_depth = 10);

      /**
       * Get all mapped files in DB.
       */
      std::vector<File> get_all_files();
  };

  class FileTree {
    public:
      File node;
      std::vector<FileTree> children;

      FileTree(const File& in) : node(in){}
  };

}

#endif
