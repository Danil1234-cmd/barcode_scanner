#include <zint.h>
#include <stdio.h>
#include <cstring>
#include <sqlite3.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <random>
#include <ctime>
#include <set>


void ensure_table_structure(sqlite3* db) {
    const char* tableCheckSQL = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='products';";
    sqlite3_stmt* stmt_check;
    if (sqlite3_prepare_v2(db, tableCheckSQL, -1, &stmt_check, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Table check failed: " + std::string(sqlite3_errmsg(db)));
    }
    
    int table_exists = 0;
    if (sqlite3_step(stmt_check) == SQLITE_ROW) {
        table_exists = sqlite3_column_int(stmt_check, 0);
    }
    sqlite3_finalize(stmt_check);
    
    if (!table_exists) {

        const char* createTableSQL = 
            "CREATE TABLE products ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "barcode TEXT NOT NULL UNIQUE,"
            "product_name TEXT NOT NULL,"
            "price REAL);";
        
        char* errMsg = nullptr;
        if (sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "SQL error (create table): " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
        return;
    }

    const char* columnCheckSQL = "PRAGMA table_info(products);";
    sqlite3_stmt* stmt_col;
    if (sqlite3_prepare_v2(db, columnCheckSQL, -1, &stmt_col, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Column check failed: " + std::string(sqlite3_errmsg(db)));
    }

    bool has_id_column = false;
    bool has_autoinc = false;
    while (sqlite3_step(stmt_col) == SQLITE_ROW) {
        std::string col_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt_col, 1));
        if (col_name == "id") {
            has_id_column = true;
            std::string type = reinterpret_cast<const char*>(sqlite3_column_text(stmt_col, 2));
            if (type.find("AUTOINCREMENT") != std::string::npos) {
                has_autoinc = true;
            }
        }
    }
    sqlite3_finalize(stmt_col);

    if (!has_id_column || !has_autoinc) {
        const char* tempTableSQL =
            "CREATE TEMPORARY TABLE products_backup AS SELECT * FROM products;"
            "DROP TABLE products;"
            "CREATE TABLE products ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "barcode TEXT NOT NULL UNIQUE,"
            "product_name TEXT NOT NULL,"
            "price REAL);"
            "INSERT INTO products (barcode, product_name, price) "
            "SELECT barcode, product_name, price FROM products_backup;"
            "DROP TABLE products_backup;";

        char* errMsg = nullptr;
        if (sqlite3_exec(db, tempTableSQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = "SQL error (migrate table): " + std::string(errMsg);
            sqlite3_free(errMsg);
            throw std::runtime_error(error);
        }
    }
}


bool barcode_exists(sqlite3* db, const std::string& barcode) {
    ensure_table_structure(db);

    std::string sql = "SELECT COUNT(*) FROM products WHERE barcode = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Barcode check failed: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return count > 0;
}


std::string generate_random_barcode(int length = 12) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(alphanum) - 2);

    std::string result;
    for (int i = 0; i < length; ++i) {
        result += alphanum[distribution(generator)];
    }
    return result;
}


std::string generate_unique_barcode() {
    sqlite3* db;
    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        throw std::runtime_error("Error opening database: " + std::string(sqlite3_errmsg(db)));
    }


    ensure_table_structure(db);


    std::set<std::string> generated_codes;
    std::string barcode;
    bool is_unique = false;
    int attempts = 0;
    const int max_attempts = 100;

    while (!is_unique && attempts < max_attempts) {
        barcode = generate_random_barcode();


        if (generated_codes.find(barcode) != generated_codes.end()) {
            attempts++;
            continue;
        }

        generated_codes.insert(barcode);
        if (!barcode_exists(db, barcode)) {
            is_unique = true;
        }
        attempts++;
    }

    sqlite3_close(db);

    if (!is_unique) {
        throw std::runtime_error("Failed to generate unique barcode after " + std::to_string(max_attempts) + " attempts");
    }

    return barcode;
}

void add_to_database(const std::string& barcode) {
    sqlite3* db;
    std::string name;
    std::string cost;

    std::cout << "Enter product name: ";
    std::getline(std::cin >> std::ws, name);

    std::cout << "Enter product cost: ";
    std::cin >> cost;

    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        throw std::runtime_error("Error opening database: " + std::string(sqlite3_errmsg(db)));
    }

    ensure_table_structure(db);


    std::string sql = "INSERT INTO products (barcode, product_name, price) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, std::stod(cost));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::string error = "Insert failed: " + std::string(sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error(error);
    }

    int id = sqlite3_last_insert_rowid(db);
    std::cout << "Product added successfully! ID: " << id << std::endl;

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

int main() {
    std::string unique_barcode;
    try {
        unique_barcode = generate_unique_barcode();
        std::cout << "Generated unique barcode: " << unique_barcode << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    zint_symbol *barcode = ZBarcode_Create();
    if (!barcode) {
        fprintf(stderr, "Creation error Zint\n");
        return 1;
    }

    barcode->symbology = BARCODE_CODE128;
    barcode->height = 50;
    barcode->scale = 2.0;

    try {
        add_to_database(unique_barcode);
        std::string file = "test_barcodes/" + unique_barcode + "_barcode.png";
    	if (file.size() >= sizeof(barcode->outfile)) {
            fprintf(stderr, "File path too long\n");
            ZBarcode_Delete(barcode);
            return 1;
        }
        strcpy(barcode->outfile, file.c_str());

        if (ZBarcode_Encode(barcode, (unsigned char*)unique_barcode.c_str(), 0) != 0) {
           fprintf(stderr, "Error: %s\n", barcode->errtxt);
            ZBarcode_Delete(barcode);
            return 1;
        }

        if (ZBarcode_Print(barcode, 0) != 0) {
            fprintf(stderr, "Error of save: %s\n", barcode->errtxt);
            ZBarcode_Delete(barcode);
            return 1;
        }

        printf("Barcode successfully saved to %s\n", barcode->outfile);
        }
    catch (const std::runtime_error& e) {
        std::cerr << "Database error: " << e.what() << std::endl;
        ZBarcode_Delete(barcode);
            return 1;
    }

    ZBarcode_Delete(barcode);
    return 0;
}
