CC			:= cc
CFLAGS	:= -I/opt/homebrew/include
LDFLAGS	:= -L/opt/homebrew/lib
LIBS		:= -lraylib \
					 -framework CoreVideo \
					 -framework IOKit \
					 -framework Cocoa \
					 -framework OpenGL

chip8: src/chip8.c
	$(CC) src/chip8.c $(CFLAGS) $(LDFLAGS) $(LIBS) -o chip8

clean:
	rm -f chip8
