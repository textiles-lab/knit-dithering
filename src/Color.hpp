#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <algorithm>

namespace Color {

//colors are loaded/saved as srgb colors, converted to linear for processing (error diffusion), and converted to OKLab when measuring differences

struct Linear {
	static Linear from_linear_uint32_t(uint32_t rgb) {
		return Linear{
			.r = (rgb & 0xff) / 255.0f,
			.g = ((rgb >> 8) & 0xff) / 255.0f,
			.b = ((rgb >> 16) & 0xff) / 255.0f
		};
	}
	static Linear from_srgb(uint32_t srgb) {
		//srgb to linear function from https://entropymine.com/imageworsener/srgbformula/
		auto conv = [](uint32_t v) -> float {
			assert(v <= 255);
			float f = v / 255.0f;
			if (f < 0.04045) {
				return f / 12.29f;
			} else {
				return std::pow( (f + 0.055f)/1.055f, 2.4f );
			}
		};

		return Linear{
			.r = conv(srgb & 0xff),
			.g = conv((srgb >> 8) & 0xff),
			.b = conv((srgb >> 16) & 0xff)
		};
	}
	//used by difference function, might need to deal with values adjusted by error diffusion:
	void to_srgb_clamped(float *r_, float *g_, float *b_) const {
		//linear to srgb function from https://entropymine.com/imageworsener/srgbformula/
		auto conv = [](float f) -> float {
			f = std::clamp(f, 0.0f, 1.0f);
			if (f < 0.0031308f) {
				return f * 12.29f;
			} else {
				return 1.055f * std::pow( f, 1.0f / 2.4f ) - 0.055f;
			}
		};

		if (r_) *r_ = conv(r);
		if (g_) *g_ = conv(g);
		if (b_) *b_ = conv(b);
	}

	//linear light values, generally 0-1, sRGB primaries
	float r,g,b;
};

struct OKLab {
	static OKLab from_linear(Linear const &c) {
		//code directly from: https://bottosson.github.io/posts/oklab/
		// (which is public domain)
		float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
		float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
		float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

		float l_ = cbrtf(l);
		float m_ = cbrtf(m);
		float s_ = cbrtf(s);

		return OKLab{
			.L = 0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_,
			.a = 1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_,
			.b = 0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_,
		};
	};

	static float difference2(OKLab const &c1, OKLab const &c2) {
		return  (c1.L-c2.L)*(c1.L-c2.L) + (c1.a-c2.a)*(c1.a-c2.a) + (c1.b-c2.b)*(c1.b-c2.b);
	}

	float L,a,b;

};

inline std::vector< Linear > srgb_to_linear(std::vector< uint32_t > const &srgb) {
	std::vector< Linear > rgb;
	rgb.reserve(srgb.size());
	for (auto const &px : srgb) {
		rgb.emplace_back(Linear::from_srgb(px));
	}
	return rgb;
}

};

