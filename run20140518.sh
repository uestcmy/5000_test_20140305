cd linux_driver
make remove
make insert

cd ml605_api
gcc -o 20140518socket 20140518socket.cpp -lpthread

./20140518socket
