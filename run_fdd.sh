cd linux_driver
make remove
make insert

cd ml605_api
gcc -o 3test_  3test_.cpp -lpthread

./3test_
