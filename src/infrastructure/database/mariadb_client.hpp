#pragma once

#include <mariadb/mysql.h>
#include <string>

#include "core/configuration.hpp"

namespace infrastructure::database {

class MariaDbClient {
public:
    MariaDbClient();
    ~MariaDbClient();

    bool initialize();
    bool connect(const core::DatabaseConfig& cfg);
    bool isConnected() const;
    bool ping();

    bool execute(const std::string& query);
    MYSQL_RES* storeResult();

    void disconnect();

private:
    MYSQL* handle_{nullptr};
    core::DatabaseConfig config_;
};

} // namespace infrastructure::database
