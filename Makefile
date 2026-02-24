# Opciones del compilador (Muestra warnings básicos y permite depuración)
CC = gcc
CFLAGS = -Wall -g

# Regla principal: compila todo por defecto al escribir 'make'
all: capitana4 ship3 ursula

# Regla para compilar la Capitana
capitana4: capitana4.c
	$(CC) $(CFLAGS) capitana4.c -o capitana4

# Regla para compilar el Barco (necesita map.c)
ship3: ship_3.c map.c
	$(CC) $(CFLAGS) ship_3.c map.c -o ship3

# Regla para compilar a Úrsula
ursula: ursula.c
	$(CC) $(CFLAGS) ursula.c -o ursula

# Regla para limpiar los ejecutables viejos al escribir 'make clean'
clean:
	rm -f capitana4 ship3 ursula