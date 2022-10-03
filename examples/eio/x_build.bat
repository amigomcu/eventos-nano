md build

gcc -std=c99 -g ^
main.c ^
eio_port_win32.c ^
..\..\eio\eio.c ^
..\..\eio\eio_serial.c ^
..\..\eio\eio_pin.c ^
-I ..\..\eio ^
-I ..\..\eventos ^
-o build\eio
