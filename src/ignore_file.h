// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#ifndef SRDP_IGNORE_FILE_H
#define SRDP_IGNORE_FILE_H

#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <filesystem>
#include <fstream>

#include "cmake_config.h"


namespace srdp {


namespace fs = std::filesystem;

  class IgnoreFile {
    private:
      struct ignore_rule {
          std::regex pattern_regex;
          bool is_directory_only;
          bool is_negation;
      };

    std::vector<ignore_rule> rules;

    std::string glob_to_regex(const std::string& glob);

  public:
      IgnoreFile() = default;
      IgnoreFile(fs::path ignore_file);

      // Load patterns directly from a .gitignore file
      void load_from_file(const fs::path& file_path);

      // Add a single pattern manually
      void add_pattern(std::string pattern);

      // Check if a given file path matches any of the rules
      bool is_ignored(const fs::path& path) const;
  };
}

#endif /* SRDP_IGNORE_FILE_H */
