// SPDX-FileCopyrightText: 2026 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only


#include "ignore_file.h"

namespace srdp {

  IgnoreFile::IgnoreFile(fs::path ignore_file) {
    load_from_file(ignore_file);
  }

  // Helper to convert a glob string into a valid std::regex string
  std::string IgnoreFile::glob_to_regex(const std::string& glob) {
      std::string regex_str = "^";
      for (char c : glob) {
          switch (c) {
              case '*':
                  regex_str += ".*";
                  break;
              case '?':
                  regex_str += ".";
                  break;
              case '.': case '+': case '^': case '$': case '|':
              case '(': case ')': case '[': case ']': case '{': case '}':
                  regex_str += "\\";
                  regex_str += c;
                  break;
              default:
                  regex_str += c;
                  break;
          }
      }
      regex_str += "$";
      return regex_str;
  }

  void IgnoreFile::load_from_file(const fs::path& file_path) {
      std::ifstream file(file_path);
      if (!file.is_open()) return;

      std::string line;
      while (std::getline(file, line)) {
          // Trim whitespace
          line.erase(0, line.find_first_not_of(" \t\r\n"));
          line.erase(line.find_last_not_of(" \t\r\n") + 1);

          // Skip empty lines and comments
          if (line.empty() || line[0] == '#') {
              continue;
          }

          bool dir_only = false;
          if (line.back() == '/') {
              dir_only = true;
              line.pop_back();
          }

          try {
              ignore_rule rule;
              rule.pattern_regex = std::regex(glob_to_regex(line));
              rule.is_directory_only = dir_only;
              rule.is_negation = false;
              rules.push_back(rule);
          } catch (const std::regex_error&) {
              continue;
          }
      }
  }

  void IgnoreFile::add_pattern(std::string pattern) {
      if (pattern.empty() || pattern[0] == '#') return;

      bool dir_only = false;
      if (pattern.back() == '/') {
          dir_only = true;
          pattern.pop_back();
      }

      rules.push_back({std::regex(glob_to_regex(pattern)), dir_only, false});
  }

  bool IgnoreFile::is_ignored(const fs::path& path) const {
      std::string filename = path.filename().string();
      bool is_dir = fs::is_directory(path);

      for (const auto& rule : rules) {
          if (rule.is_directory_only && !is_dir) {
              continue;
          }

          if (std::regex_match(filename, rule.pattern_regex)) {
              return true;
          }
      }
      return false;
  }
}
