#ifndef SRDP_CONFIG_H
#define SRDP_CONFIG_H

#include <memory>
#include "sql.h"

namespace srdp {
  class Config {
    private:
      std::shared_ptr<Sql> db;

    public:
      Config(){};
      Config(const std::shared_ptr<Sql>& db);

      static void create_table(Sql& db);

      uuids::uuid get_uuid(const std::string& key);
      void set_uuid(const std::string& key, const uuids::uuid& uuid);

      std::string get_string(const std::string& key);
      void set_string(const std::string& key, const std::string& value);


      // Pre-defined values
      uuids::uuid get_project() { return get_uuid("project"); }
      void set_project(const uuids::uuid& uuid) { set_uuid("project", uuid); }

      uuids::uuid get_experiment() { return get_uuid("experiment"); }
      void set_experiment(const uuids::uuid& uuid) { set_uuid("experiment", uuid); }

      std::string get_owner() { return get_string("owner"); }
      void set_owner(const std::string& owner) { set_string("owner", owner); }

      std::string get_store_path() { return get_string("store_path"); }
      void set_store_path(const std::string& path) { set_string("store_path", path); }
  };
}

#endif

