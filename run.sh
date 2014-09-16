cd linux_driver
make remove
make insert

cd ml605_api
gcc -o 5000aaa0_socket_3port_20140311 5000aaa0_socket_3port_20140311.cpp -lpthread

./5000aaa0_socket_3port_20140311
