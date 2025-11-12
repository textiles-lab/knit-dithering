#pragma once

#include "Color.hpp"
#include "Cost.hpp"

#include <vector>
#include <cstdint>
#include <iostream>
#include <functional>

struct DitherParams {
	std::vector< Color::Linear > const &yarns_linear;
	uint32_t image_width = 0;
	uint32_t image_height = 0;
	std::vector< Color::Linear > const &image_linear;
	
	uint32_t use_within = 11; //every window of this length in a row must use every yarn (0 == disable)
	uint32_t cross_within = 20; //every window of this length in a row must contain a crossing (0 == disable)

	Difference const &difference; //difference function

	bool diffuse = true; //use error diffusion

	uint32_t seed = 0; //was: 3141926265u; //seed for pseudo-random stream; '0' is special value meaning "just pick the first one"

	uint32_t max_threads = 0; //maximum number of compute threads to use; '0' means automatically pick (probably based on max core count).
};

//returns yarn indices array of same size as input image.
std::vector< uint8_t > optimal_dither(DitherParams const &params);

std::vector< uint8_t > greedy_dither(DitherParams const &params);

//helper used by both dithers:
// applies error diffusion to image_linear
// (or, if params.diffuse == false, this does nothing)
void error_diffusion(DitherParams const &params, uint32_t row, std::vector< uint8_t > const &dithered, std::vector< Color::Linear > *image_linear);


//---------------------------------------
//moving State up here so it can be shared by both optimal and greedy dither approaches:

//Relevant path information just after a stitch is placed:
struct State {
	State(size_t yarns_) : last_used(yarns_, 0) { }
	std::vector< uint8_t > last_used; //how many stitches since this yarn was last used (if one, yarn was just used; if use_within, state is a dead end; zero is a special value indicating yarn has never been used)
	uint8_t last_cross = 0; //how many stitches since the most recent crossing started (if zero, no crossing has happened yet; otherwise will be >= 2 since you can't have a crossing in just one stitch worth of space!
	bool operator==(State const &) const = default;
	bool operator<(State const &o) const {
		if (last_used != o.last_used) return last_used < o.last_used;
		else return last_cross < o.last_cross;
	}
	//call a callback on all valid successor states to this state, after filling a stitch at column 'x' of the row:
	void next_states(DitherParams const &params, uint32_t x, std::function< void( uint32_t, State const & ) > const &cb) const {
		uint32_t use_within = params.use_within;
		uint32_t cross_within = params.cross_within;

		//PARANOIA:
		assert(cross_within == 0 || (last_cross <= cross_within && last_cross != 1)); //if cross_within is enabled, last_cross should either be zero (at start of row) or >= 2 and <= cross_within
		assert(cross_within != 0 || last_cross == 0); //if cross_within disabled, last_cross should be zero, always

		for (uint8_t y = 0; y < last_used.size(); ++y) {
			//move state forward:
			State next_state = *this;
			if (cross_within != 0) {
				if (next_state.last_cross != 0) {
					next_state.last_cross += 1;
					assert(next_state.last_cross > 0); //no overflow, please!
				}
			}

			for (auto &last_used : next_state.last_used) {
				if (last_used == 0) continue;
				last_used += 1;
				if (use_within == 0) {
					if (cross_within == 0) {
						//if crossing tracking also disabled, all we need to know is that yarn was used (and if it was most-recently used):
						last_used = 2;
					} else {
						//if crossing tracking is enabled, need to know how long since yarn was used until after it exceeds cross_within:
						if (last_used > cross_within + 1) last_used = cross_within + 1;
					}
				}
				assert(last_used != 0); //no overflow, please!
			}

			//use the yarn:

			//update for crossings:
			if (cross_within != 0 //cross_within must be enabled
			 && next_state.last_used[y] != 0 //yarn must have been used
			 && next_state.last_used[y] % 2 == 0) { //and yarn must have been used an even number of steps ago (since it's about to be used an odd number of steps ago)
			 	//if there is no last crossing, or the last crossing was before this crossing, update it:
				if (next_state.last_cross == 0 || next_state.last_cross > next_state.last_used[y]) {
					next_state.last_cross = next_state.last_used[y];
				}
			}
			//mark the yarn used:
			next_state.last_used[y] = 1;

			//check for validity:
			bool valid = true;
			if (use_within != 0) {
				for (auto const last_used : next_state.last_used) {
					if (last_used == 0) {
						//if not used yet, then treat as if yarn was last used just left of x=0:
						// i.e., moving over x=0, state would have last_used=1, so next_state should have last_used=2
						if (x + 2 > use_within) valid = false;
					} else {
						if (last_used > use_within) valid = false;
					}
				}
			}
			if (cross_within != 0) {
				if (next_state.last_cross == 0) {
					//no crossings yet, so make sure we aren't past the window:
					if (x + 2 > cross_within) valid = false;
				} else {
					if (next_state.last_cross > cross_within) valid = false;
				}
			}

			if (valid) {
				cb( y, next_state );
			}
		}
	}
};

inline std::ostream &operator<<(std::ostream &out, State const &s) {
	out << "[";
	for (uint32_t y = 0; y < s.last_used.size(); ++y) {
		if (y != 0) out << ",";
		if (s.last_used[y] == 0) out << '_';
		else out << int(s.last_used[y]);
	}
	out << "]x" << int(s.last_cross);
	return out;
}

namespace std {
	template< >
	struct hash< State > {
		size_t operator()(State const &s) const {
			static std::hash< std::string > h;
			std::string str(reinterpret_cast< const char * >(s.last_used.data()), s.last_used.size());
			return h(str) ^ (size_t(s.last_cross) << (8*(sizeof(size_t)-1)));
		}
	};
}

