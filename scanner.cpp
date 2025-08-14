#include <sqlite3.h>
#include <iostream>
#include <string>
#include <zbar.h>  // Add the ZBar header

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iomanip>

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

int main() {
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
       // Получаем данные из колонок
       int id = sqlite3_column_int(stmt, 0);
       const unsigned char* productName = sqlite3_column_text(stmt, 1);
       double price = sqlite3_column_double(stmt, 2);

       // Конвертируем название в string (const char* → string)
       std::string nameStr(reinterpret_cast<const char*>(productName));

       // Форматируем цену
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
