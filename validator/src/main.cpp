#include <iostream>
#include <libpq-fe.h>

int main() {
    PGconn* conn = PQconnectdb("dbname=postgres user=saumyakumar host=/tmp");
    if (PQstatus(conn) != CONNECTION_OK) {
        std::cerr << "Connection failed: " << PQerrorMessage(conn) << "\n";
        PQfinish(conn);
        return 1;
    }
    std::cout << "Connected to Postgres " << PQserverVersion(conn) << "\n";
    PQfinish(conn);
    return 0;
}