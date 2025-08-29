#include "ScannerWindow.h"
#include "ui_ScannerWindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <zint.h>
#include <sqlite3.h>  // Добавляем прямой include
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

// Вспомогательные функции для работы с БД
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

// Функция распознавания штрих-кода
std::string barcode_reader(const char* filename) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);

    if (!image) {
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

ScannerWindow::ScannerWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ScannerWindow)
{
    ui->setupUi(this);
}

ScannerWindow::~ScannerWindow()
{
    delete ui;
}

void ScannerWindow::on_choiceBarcode_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));

    if (fileName.isEmpty()) {
        return;
    }

    std::string barcode = barcode_reader(fileName.toStdString().c_str());

    if (barcode.empty()) {
        QMessageBox::warning(this, "Error", "Barcode not found or error occurred");
        return;
    }

    sqlite3* db;
    sqlite3_stmt* stmt;

    if (sqlite3_open("products.db", &db) != SQLITE_OK) {
        QMessageBox::critical(this, "Database Error", sqlite3_errmsg(db));
        return;
    }

    ensure_table_structure(db);

    std::string sql = "SELECT id, product_name, price FROM products WHERE barcode = ?";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        QMessageBox::critical(this, "SQL Error", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* productName = sqlite3_column_text(stmt, 1);
        double price = sqlite3_column_double(stmt, 2);

        QString result = QString("Product ID: %1\nName: %2\nPrice: $%3")
                             .arg(id)
                             .arg(QString::fromUtf8(reinterpret_cast<const char*>(productName)))
                             .arg(price, 0, 'f', 2);

        QMessageBox::information(this, "Product Found", result);
    } else {
        QMessageBox::information(this, "Not Found",
                                 QString("Product not found for barcode: %1").arg(QString::fromStdString(barcode)));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}
