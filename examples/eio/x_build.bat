md build

gcc -std=c99 -g ^
main.c ^
eio_port_win32.c ^
..\..\eio\eio.c ^
..\..\eio\eio_serial.c ^
..\..\eio\eio_pin.c ^
..\..\eio\eio_ain.c ^
..\..\eio\eio_aout.c ^
..\..\eio\eio_can.c ^
-I ..\..\eio ^
-I ..\..\eventos ^
-o build\eio
