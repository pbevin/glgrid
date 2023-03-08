all: grid lines

grid: grid.cc glad.c glad/glad.h KHR/khrplatform.h
	g++ -I. -g -O grid.cc glad.c -o grid -lglfw

lines: lines.c
	gcc -g -O lines.c -o lines -lglfw -lGLEW -lGL
