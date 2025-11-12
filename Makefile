

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


%.dat : %.k ../knitout-backend-swg/knitout-to-dat.js
	node ../knitout-backend-swg/knitout-to-dat.js '$<' '$@'

result/sizes/%.k : result/sizes/%-f.png result/sizes/%-b.png knit-jacquard.js
	node knit-jacquard.js 'result/sizes/$*-f.png' 'result/sizes/$*-b.png' --bindoff > '$@'

result/extra/%.k : result/extra/%-f.png result/extra/%-b.png knit-jacquard.js
	node knit-jacquard.js 'result/extra/$*-f.png' 'result/extra/$*-b.png' --bindoff > '$@'

%.k : %-f.png %-b.png knit-jacquard.js
	node knit-jacquard.js '$*-f.png' '$*-b.png' > '$@'


result-dats : cross-within-dats use-within-dats sizes-dats costs-dats yarns-dats extra-dats

cross-within-dats : \
	result/cross-within/x0.dat \
	result/cross-within/x3.dat \
	result/cross-within/x4.dat \
	result/cross-within/x5.dat \
	result/cross-within/x6.dat \
	result/cross-within/x8.dat \
	result/cross-within/x12.dat \
	result/cross-within/x16.dat \
	result/cross-within/x24.dat \
	result/cross-within/x32.dat \

use-within-dats : \
	result/use-within/u0.dat \
	result/use-within/u5.dat \
	result/use-within/u6.dat \
	result/use-within/u7.dat \
	result/use-within/u8.dat \
	result/use-within/u9.dat \
	result/use-within/u10.dat \
	result/use-within/u11.dat \
	result/use-within/u12.dat \
	result/use-within/u13.dat \
	result/use-within/u14.dat \

sizes-dats : \
	result/sizes/S-u9.dat \
	result/sizes/M-u9.dat \
	result/sizes/L-u9.dat \
	result/sizes/X-u9.dat \

costs-dats : \
	result/costs/srgb.dat \
	result/costs/linear.dat \
	result/costs/oklab.dat \

yarns-dats : \
	result/yarns/y1-u9-x24.dat \
	result/yarns/y2-u9-x24.dat \
	result/yarns/y3-u9-x24.dat \
	result/yarns/y4-u9-x24.dat \
	result/yarns/y5-u9-x24.dat \
	result/yarns/y6-u9-x24.dat \
	result/yarns/y7-u9-x24.dat \
	result/yarns/y8-u9-x24.dat \
	result/yarns/y9-u9-x24.dat \

extra-dats : \
	result/extra/penny.dat \
	result/extra/portrait.dat \
	result/extra/cm1.dat \
	result/extra/cm2.dat \

#keep intermediates:
.SECONDARY :
