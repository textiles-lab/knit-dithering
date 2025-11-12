#include "dither.hpp"

void error_diffusion(DitherParams const &params, uint32_t row, std::vector< uint8_t > const &dithered, std::vector< Color::Linear > *image_linear_) {
	assert(image_linear_);
	auto &image_linear = *image_linear_;


	if (!params.diffuse) return;

	//grab useful values from params:
	uint32_t const image_width = params.image_width;
	uint32_t const image_height = params.image_height;
	std::vector< Color::Linear > const &yarns_linear = params.yarns_linear;

	//make sure image is the right size:
	assert(image_linear.size() == image_width * image_height);

	//make sure the dither raster is big enough to get yarn values from:
	assert(dithered.size() >= row * image_width);

	for (uint32_t x = 0; x < image_width; ++x) {
		uint8_t y = dithered[row * image_width + x];
		assert(y < yarns_linear.size());
		Color::Linear px_color = image_linear[row * image_width + x];
		Color::Linear yarn_color = yarns_linear[y];

		struct D {
			int8_t x,y;
			float w;
		};
		for (D const &d : {
		/*
			//Floyd-Steinberg
			D{.x = -2, .y = 1, .w = 3.0f / 16.0f},
			D{.x =  0, .y = 1, .w = 5.0f / 16.0f},
			D{.x =  2, .y = 1, .w = 1.0f / 16.0f},
		*/
			//modified to be symmetric; still lower-than-one total kernel weight
			D{.x = -2, .y = 1, .w = 2.0f / 16.0f},
			D{.x =  0, .y = 1, .w = 5.0f / 16.0f},
			D{.x =  2, .y = 1, .w = 2.0f / 16.0f},
		}) {
			int32_t x_ = int32_t(x) + d.x;
			int32_t row_ = int32_t(row) + d.y;
			if (0 <= x_ && uint32_t(x_) < image_width && 0 <= row_ && uint32_t(row_) < image_height) {
				Color::Linear &target = image_linear[row_*image_width + x_];
				target.r += d.w * (px_color.r - yarn_color.r);
				target.g += d.w * (px_color.g - yarn_color.g);
				target.b += d.w * (px_color.b - yarn_color.b);
			}
		}
	}
}
