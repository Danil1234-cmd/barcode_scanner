# Barcode scan
### What is it?
#### A simple C++ program that can be used to scan barcodes with a camera

### How to use it? (Use GUI):
```bash
cd barcode_desktop_app
mkdir build
cd build
cmake ..
make
./barcode_desktop_app
```

### How to use it? (Use Termial):

#### Create a database and fill it out
```bash
sqlite3 products.db
SQLite version 3.45.1 2024-01-30 16:01:20
Enter ".help" for usage hints.
sqlite> .quit

./init_database
```

#### Create a barcode and data in SQL:
```bash
./barcode_main
Choice a function generate/scanner (Enter a name of function): generate

Generated unique barcode: BZSZFUDDNNHC
Enter product name: Name_of_product
Enter product cost: 15
Product added successfully! ID: 1
Barcode successfully saved to test_barcodes/BZSZFUDDNNHC_barcode.png
```

#### Run the scanner and enter the path to the barcode file:

```bash
./barcode_main
Choice a function generate/scanner (Enter a name of function): scanner

Enter file name: test_barcodes/BZSZFUDDNNHC_barcode.png
Recognized barcode: BZSZFUDDNNHC
Product ID: 1
Name: Name_of_product
Price: $15.00
```

#### If you have modified the files, type the following to compile:
```bash
./compile/main_compile.sh #For main.cpp file
./compile/init_compile.sh #For init_database.cpp
```

### Planned updates:
- Add to the GUI version the ability to use the camera to get a barcode image
- Creating an app on Android and IOS


#### (Thank you for using my software)
