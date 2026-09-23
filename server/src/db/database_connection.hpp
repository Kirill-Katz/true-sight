#pragma once

#include <string>
#include <pqxx/pqxx>

class DatabaseConnection {
    public:
        explicit DatabaseConnection(const std::string& connection_string)
            : connection_(connection_string)
        {}

        pqxx::result exec(
            const std::string& sql,
            const pqxx::params& params
        );

        bool is_open() const
        {
            return connection_.is_open();
        }

    private:
        pqxx::connection connection_;
};
