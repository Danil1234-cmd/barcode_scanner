#include <sqlite3.h>
#include <iostream>
#include <stdexcept>

int main() {
    sqlite3* db;
    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        std::cerr << "Error opening database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    const char* createTableSQL =
        "CREATE TABLE IF NOT EXISTS products ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "barcode TEXT NOT NULL UNIQUE,"
        "product_name TEXT NOT NULL,"
        "price REAL);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return 1;
    }


    const char* deleteDataSQL = "DELETE FROM products;";
    if (sqlite3_exec(db, deleteDataSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    const char* resetSeqSQL = "DELETE FROM sqlite_sequence WHERE name='products';";
    if (sqlite3_exec(db, resetSeqSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    std::cout << "Database initialized successfully!" << std::endl;
    sqlite3_close(db);
    return 0;
}
