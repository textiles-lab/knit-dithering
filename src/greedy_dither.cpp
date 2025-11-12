#include "dither.hpp"

#include <chrono>
#include <unordered_set>

std::vector< uint8_t > greedy_dither(DitherParams const &params) {

	uint32_t const image_width = params.image_width;
	uint32_t const image_height = params.image_height;
	std::vector< Color::Linear > const &yarns_linear = params.yarns_linear;
	Difference const &difference = params.difference;
	//a copy because diffusion needs to modify it:
	std::vector< Color::Linear > image_linear = params.image_linear;

	//this will probably eventually be in params:
	uint32_t const beam_width = 100;
	assert(beam_width >= 1);

	std::vector< uint8_t > dither;
	dither.reserve(image_width * image_height);

	//initial state:
	State init(yarns_linear.size());
	//mark all yarns as unused:
	for (auto &last_used : init.last_used) {
		last_used = 0;
	}
	//no crossing yet:
	init.last_cross = 0;

	struct Layer {
		std::unordered_map< State, Cost > visited;
		std::unordered_set< State > to_expand;
	};

	for (uint32_t row = 0; row < image_height; ++row) {
		auto before = std::chrono::high_resolution_clock::now();
		std::cout << (row+1) << "/" << image_height << ":"; std::cout.flush();

		//costs of using each yarn:
		std::vector< Cost > yarn_costs; //yarn_costs[x * image_width + y] is the cost of using yarn y at pixel x
		yarn_costs.reserve(image_width * yarns_linear.size());
		for (uint32_t x = 0; x < image_width; ++x) {
			Color::Linear px_color = image_linear[row*image_width+x];
			for (uint32_t y = 0; y < yarns_linear.size(); ++y) {
				Color::Linear yarn_color = yarns_linear[y];
				yarn_costs.emplace_back( difference(px_color, yarn_color) );
			}
		}
		assert(yarn_costs.size() == image_width * yarns_linear.size());


		std::vector< Layer > layers;
		layers.resize(image_width + 1);

		//initial state:
		layers[0].visited.emplace(init, Cost{0});
		layers[0].to_expand.emplace(init);

		//work in multiple passes, until enough states arrive at the end:
		while (layers.back().visited.size() < beam_width) {

			uint32_t x = 0;
			//skip all the finished layers:
			while (x < image_width && layers[x].to_expand.empty()) ++x;

			//oh, actually finished(!)
			if (x == image_width) {
				std::cout << " (opt!)"; std::cout.flush();
				break;
			}

			assert(x < image_width);
			assert(!layers[x].to_expand.empty());

			//keep expanding unfinished layers:
			for (; x < image_width; ++x) {
				Layer &prev = layers[x];
				Layer &next = layers[x+1];

				if (prev.to_expand.empty()) break; //didn't make it to the end this pass, drat

				std::vector< State > to_expand(prev.to_expand.begin(), prev.to_expand.end());

				//std::cout << "{" << x << ": " << to_expand.size() << "}"; std::cout.flush(); //DEBUG

				//sort lowest costs to the front of to_expand:
				std::sort(to_expand.begin(), to_expand.end(), [&](State const &a, State const &b){
					Cost ca = prev.visited.at(a);
					Cost cb = prev.visited.at(b);
					if (ca != cb) return ca < cb;
					else return a < b;
				});

				const uint32_t block = 200;

				if (to_expand.size() > block) {
					to_expand.erase(to_expand.begin() + block, to_expand.end());
				}

				for (auto const &state : to_expand) {
					//expand (find next layer states from) the state:
					Cost cost = prev.visited.at(state);
					state.next_states(params, x, [&](uint32_t y, State const &next_state) {

						//eliminate (some) dead states:

						//this might not be 100% right -- setting use_within == yarns gives odd results
						if (params.use_within != 0) {
							//compute how many steps until each yarn must be used:
							std::vector< uint8_t > within;
							within.reserve(next_state.last_used.size());
							
							for (auto const &lu : next_state.last_used) {
								if (lu == 0) {
									int32_t w = int32_t(params.use_within) - int32_t(x+1);
									assert(w >= 0);
									within.emplace_back(w);
								} else {
									int32_t w = 1 + int32_t(params.use_within) - int32_t(lu);
									assert(w >= 0);
									within.emplace_back(w);
								}
							}

							//make sure there are enough steps to use every yarn that wants to be used in that many steps:
							std::sort(within.begin(), within.end());
							for (uint32_t i = 0; i < within.size(); ++i) {
								if (x + i + 1 > image_width) break; //saved by the boundary
								if (within[i] < i + 1) return;
							}
						}

						Cost next_cost = cost + yarn_costs[x*yarns_linear.size() + y];
						auto ret = next.visited.emplace(next_state, next_cost);
						if (ret.second || ret.first->second > next_cost) {
							ret.first->second = next_cost;
							next.to_expand.emplace(next_state);
						}
					});

					//mark as expanded:
					auto f = prev.to_expand.find(state);
					assert(f != prev.to_expand.end());
					prev.to_expand.erase(f);
				}
			}

			//std::cout << " (" << x << ":" << layers[std::max(0,int(x)-1)].visited.size() << "/" << beam_width << ")"; std::cout.flush();
		}
	
		//read back best path:
		{
			//backtrack:
			std::vector< State > path;
			path.reserve(image_width + 1);

			std::vector< uint8_t > path_yarns;
			path_yarns.reserve(image_width);

			assert(!layers.back().visited.empty());
			//find the best final state:
			State lowest = layers.back().visited.begin()->first;
			for (auto const &[state, cost] : layers.back().visited) {
				if (cost < layers.back().visited.at(lowest)) {
					lowest = state;
				}
			}
			std::cout << " cost " << layers.back().visited[lowest];

			path.emplace_back(lowest);

			for (uint32_t x = image_width-1; x < image_width; --x) {
				Layer const &prev = layers[x];
				Layer const &next = layers[x+1];

				Cost best = std::numeric_limits< Cost >::infinity();
				uint8_t best_yarn = 0xff;
				State const *best_from = nullptr;
				for (auto const &[from_state, from_cost] : prev.visited) {
					from_state.next_states(params, x, [&](uint32_t y, State const &to_state) {
						if (to_state != path.back()) return;
						Cost cost = from_cost + yarn_costs[x * yarns_linear.size() + y];
						if (cost < best) {
							best = cost;
							best_yarn = y;
							best_from = &from_state;
						}
					});
				}
				assert(best_from);
				assert(best <= next.visited.at(path.back()));

				path.emplace_back(*best_from);
				path_yarns.emplace_back(best_yarn);
			}

			std::reverse(path.begin(), path.end());
			std::reverse(path_yarns.begin(), path_yarns.end());

			dither.insert(dither.end(), path_yarns.begin(), path_yarns.end());
		}

		//do error diffusion:
		error_diffusion(params, row, dither, &image_linear);

		auto after = std::chrono::high_resolution_clock::now();
		std::cout << " (" <<  std::chrono::duration< double >(after - before).count() * 1000 << "ms)" << std::endl;

	}

	return dither;
}
