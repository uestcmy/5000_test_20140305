cd liunx_driver
make remove
make insert
cd ml605_api
gcc -o test_save_20140404 test_save_20140404.cpp -lpthread
./test_save_20140404
