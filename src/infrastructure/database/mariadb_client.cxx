#include "infrastructure/database/mariadb_client.hpp"

#include <iostream>

#include "core/logger.hpp"

namespace infrastructure::database {

MariaDbClient::MariaDbClient() = default;

MariaDbClient::~MariaDbClient() {
    disconnect();
}

bool MariaDbClient::initialize() {
    handle_ = mysql_init(nullptr);
    if (handle_ == nullptr) {
        LOG_ERROR("database", "mysql_init() failed");
        return false;
    }
    return true;
}

bool MariaDbClient::connect(const core::DatabaseConfig& cfg) {
    config_ = cfg;
    if (handle_ == nullptr && !initialize()) {
        return false;
    }

    MYSQL* result = mysql_real_connect(
        handle_,
        cfg.host.c_str(),
        cfg.user.c_str(),
        cfg.password.c_str(),
        cfg.schema.c_str(),
        cfg.port,
        nullptr,
        0);

    if (result == nullptr) {
        LOG_ERROR("database", "mysql_real_connect() failed: ", mysql_error(handle_));
        return false;
    }
    LOG_INFO("database", "Connected to MariaDB at ", cfg.host, ":", cfg.port);
    return true;
}

bool MariaDbClient::isConnected() const {
    return handle_ != nullptr;
}

bool MariaDbClient::ping() {
    if (handle_ == nullptr) {
        return false;
    }
    return mysql_ping(handle_) == 0;
}

bool MariaDbClient::execute(const std::string& query) {
    if (handle_ == nullptr && !initialize()) {
        return false;
    }
    if (mysql_query(handle_, query.c_str()) != 0) {
        LOG_ERROR("database", "Query failed: ", mysql_error(handle_), ". SQL: ", query);
        return false;
    }
    return true;
}

MYSQL_RES* MariaDbClient::storeResult() {
    if (handle_ == nullptr) {
        return nullptr;
    }
    return mysql_store_result(handle_);
}

void MariaDbClient::disconnect() {
    if (handle_ != nullptr) {
        mysql_close(handle_);
        handle_ = nullptr;
    }
}

} // namespace infrastructure::database
