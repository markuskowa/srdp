// SPDX-FileCopyrightText: 2025 Markus Kowalewski
//
// SPDX-License-Identifier: GPL-3.0-only

#include <iostream>
#include "srdp.h"

namespace srdp {

  void print_project(const Project& prj){
    std::cout << "name:  "  << prj.name << "\n";
    std::cout << "uuid:  "  << uuids::to_string(prj.uuid) << "\n";
    std::cout << "owner: "  << (prj.owner ? *prj.owner : "") << "\n";
    std::cout << "ctime: "  << (prj.ctime ? Srdp::get_time_stamp_fmt(*prj.ctime) : "") << "\n";
    std::cout << "abstract:\n  " << (prj.metadata ? *prj.metadata : "") << "\n";
  };

  void print_experiment(const Experiment& exp){
    std::cout << "name:    "  << exp.name << "\n";
    std::cout << "uuid:    "  << uuids::to_string(exp.uuid) << "\n";
    std::cout << "owner:   "  << (exp.owner ? *exp.owner : "") << "\n";
    std::cout << "ctime:   "  << (exp.ctime ? Srdp::get_time_stamp_fmt(*exp.ctime) : "") << "\n";
    std::cout << "abstract:\n  " << (exp.metadata ? *exp.metadata : "") << "\n";
  };

  void print_file_info(File& f){
    std::cout << "path:     " << (f.path ? *f.path : "") << "\n";
    if (f.role)
      std::cout << "role:     " << File::role_to_string(*f.role) << "\n";
    if (f.original_name)
      std::cout << "name:     " << *f.original_name << "\n";
    std::cout << "size:     " << f.size << "\n";
    std::cout << "hash:     " << scas::Hash::convert_hash_to_string(f.hash) << "\n";
    std::cout << "owner:    " << (f.owner ? *f.owner : "") << "\n";
    std::cout << "ctime:   "  << (f.ctime ? Srdp::get_time_stamp_fmt(*f.ctime) : "") << "\n";
    std::cout << "creator:  " << f.resolve_creator() << "\n";
    std::cout << "metadata: " << (f.metadata ? *f.metadata : "") << "\n";
    std::cout << "\n\n";
  };
}
