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

#include <zbar.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iomanip>


// generator
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

int generate() {
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



// scanner
// Real Barcode Recognition Function Using ZBar
std::string barcode_reader(const char* filename) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);

    if (!image) {
        std::cerr << "Error loading image: " << filename << std::endl;
        return "";
    }


    unsigned char* gray_image = nullptr;
    if (channels == 1) {
        gray_image = image;
    }
    else {
        gray_image = new unsigned char[width * height];
        for (int i = 0; i < width * height; ++i) {
            unsigned char r = image[i * channels];
            unsigned char g = image[i * channels + 1];
            unsigned char b = image[i * channels + 2];
            gray_image[i] = static_cast<unsigned char>(0.299 * r + 0.587 * g + 0.114 * b);
        }
    }


    zbar::ImageScanner scanner;
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);


    zbar::Image zimg(width, height, "Y800", gray_image, width * height);


    std::string result;
    if (scanner.scan(zimg) > 0) {

        zbar::Image::SymbolIterator symbol = zimg.symbol_begin();
        result = symbol->get_data();
    }

    if (channels != 1) {
        delete[] gray_image;
    }
    stbi_image_free(image);

    return result;
}

int scan() {
    std::string file;
    std::cout << "Enter file name: ";
    std::cin >> file;

    std::string barcode = barcode_reader(file.c_str());

    if (barcode.empty()) {
        std::cerr << "Barcode not found or error occurred" << std::endl;
        return 1;
    }

    std::cout << "Recognized barcode: " << barcode << std::endl;

    sqlite3* db;
    sqlite3_stmt* stmt;

    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        std::cerr << "Database error: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    std::string sql = "SELECT id, product_name, price FROM products WHERE barcode = ?";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {

       int id = sqlite3_column_int(stmt, 0);
       const unsigned char* productName = sqlite3_column_text(stmt, 1);
       double price = sqlite3_column_double(stmt, 2);


       std::string nameStr(reinterpret_cast<const char*>(productName));


       std::cout << "Product ID: " << id << "\n"
                 << "Name: " << nameStr << "\n"
                 << "Price: $" << std::fixed << std::setprecision(2) << price << std::endl;
    } else {
       std::cout << "Product not found for barcode: " << barcode << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}


int main() {
    std::string choice;
    std::cout << "Choice a function generate/scanner (Enter a name of function):";
    std::cin >> choice;

    if (choice == "generate") {
	generate();
    }

    else if (choice == "scanner") {
	scan();
    }

    else {
	std::cout << "Please enter a correct name of function" << std::endl;
	main();
    }

    return 0;
}
