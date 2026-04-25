#pragma once

#include <vector>

#include "core/configuration.hpp"
#include "domain/telemetry_models.hpp"
#include "infrastructure/database/mariadb_client.hpp"

namespace infrastructure::database {

class TelemetryRepository {
public:
    TelemetryRepository() = default;

    bool initialize(const core::DatabaseConfig& cfg);
    std::vector<domain::TelemetryReading> loadEnvironmental(std::size_t limit);
    std::vector<domain::TelemetryReading> loadSoilAndAir(std::size_t limit);

    void refreshConnection();

private:
    domain::TelemetryReading buildEnvReading(MYSQL_ROW row) const;
    domain::TelemetryReading buildSoilReading(MYSQL_ROW row) const;

    core::DatabaseConfig config_;
    MariaDbClient client_;
};

} // namespace infrastructure::database
