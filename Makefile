

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CPP=g++ -Wall -Werror -g -O2 --std=c++20 -Iextern/stb -Wno-unused-function -Wno-unused-but-set-variable
endif
ifeq ($(UNAME_S),Darwin)
    CPP=clang++ -Wall -Werror -g -O2 --std=c++20 -Iextern/stb -Wno-unused-function -Wno-unused-but-set-variable -Wno-deprecated-declarations
endif 
 

knit-dither : objs/knit-dither.o objs/optimal_dither.o objs/greedy_dither.o objs/error_diffusion.o
	$(CPP) -o '$@' $^

objs/knit-dither.o : src/knit-dither.cpp src/Color.hpp src/Cost.hpp src/dither.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'

objs/optimal_dither.o : src/optimal_dither.cpp src/Color.hpp src/Cost.hpp src/dither.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'

objs/greedy_dither.o : src/greedy_dither.cpp src/Color.hpp src/Cost.hpp src/dither.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'

objs/error_diffusion.o : src/error_diffusion.cpp src/Color.hpp src/Cost.hpp src/dither.hpp
	mkdir -p objs
	$(CPP) -c -o '$@' '$<'

example/dithered-front.png example/dithered-back.png : knit-dither example/front.png example/back.png example/yarn_measured_rayon_11.png
	./knit-dither \
		--in-front example/front.png \
		--in-back example/back.png \
		--yarns example/yarn_measured_rayon_11.png \
		--select-yarns 5 \
		--use-within 9 \
		--cross-within 24 \
		--out-front example/dithered-front.png \
		--out-back example/dithered-back.png

example/knitout.k : example/dithered-front.png example/dithered-back.png
	./knit-jacquard.js example/dithered-front.png example/dithered-back.png --bindoff > example/knitout.k

clean :
	rm -f knit-dither objs/*.o

#keep intermediates:
.SECONDARY :
