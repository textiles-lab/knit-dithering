#include "dither.hpp"

#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <random>

#define USE_THREADS
#ifdef USE_THREADS

struct JobQueue {
	struct Shared {
		std::deque< std::function< void() > > queue;
		std::mutex mutex;
		std::condition_variable cv;
		bool quit = false;

		uint32_t pending = 0;
		std::condition_variable done_cv;
	} shared;
	std::vector< std::thread > workers;

	JobQueue(uint32_t max_threads) {
		unsigned int n = std::thread::hardware_concurrency();
		if (max_threads != 0) n = std::min(n, max_threads);
		std::cout << "Spawning " << n << " worker threads." << std::endl;
		workers.reserve(n);
		for (unsigned int i = 0; i < n; ++i) {
			//making a non-member-variable pointer to copy to thread:
			workers.emplace_back(worker_main, &shared);
		}
	}
	~JobQueue() {
		std::cout << " Waiting for worker threads to exit..."; std::cout.flush();
		{
			std::lock_guard< std::mutex > lock(shared.mutex);
			shared.queue.clear();
			shared.quit = true;
			shared.cv.notify_all();
		}
		for (auto &worker : workers) {
			worker.join();
		}
		std::cout << " done." << std::endl;
	}

	void run(std::function< void() > const &fn) {
		std::lock_guard< std::mutex > lock(shared.mutex);
		shared.queue.emplace_back(fn);
		shared.cv.notify_one();
	}
	void wait() {
		std::unique_lock< std::mutex > lock(shared.mutex);
		while (!(shared.queue.empty() && shared.pending == 0)) {
			shared.done_cv.wait(lock);
		}
	}

	static void worker_main(Shared *shared_) {
		Shared &shared = *shared_;

		std::unique_lock< std::mutex > lock(shared.mutex);
		while (!shared.quit) {
			if (shared.queue.empty()) {
				shared.cv.wait(lock);
			} else {
				std::function< void() > fn = std::move(shared.queue.front());
				shared.queue.pop_front();
				shared.pending += 1;
				lock.unlock();

				fn();

				lock.lock();
				shared.pending -= 1;
				shared.done_cv.notify_all();
			}
		}
	}

};

#endif


std::vector< uint8_t > optimal_dither(DitherParams const &params) {

#ifdef USE_THREADS
	JobQueue job_queue(params.max_threads);
#endif

	std::vector< Color::Linear > const &yarns_linear = params.yarns_linear;
	uint32_t const image_width = params.image_width;
	uint32_t const image_height = params.image_height;

	//NOTICE: making a copy of image_linear since error diffusion will edit the pixel values. This is slightly inelegant.
	std::vector< Color::Linear > image_linear = params.image_linear;

	Difference const &difference = params.difference;

	bool print_state_table = false; //show the states and their transitions

	constexpr uint32_t YARN_SHIFT = 27;
	constexpr uint32_t STATE_MASK = 0x07ffffff;
	static_assert(~(31u << YARN_SHIFT) == STATE_MASK, "Yarn shift avoids state mask perfectly.");

	struct Table {
		std::vector< State > states;

		//"pull"-style propagation from previous table:
		std::vector< uint32_t > first_from; //first index to read from for each state
		std::vector< uint32_t > froms; //(yarn index << 26) | (prev table state index)

		#ifdef USE_THREADS
		//try to give each worker about the same number of 'froms' to deal with:
		std::vector< uint32_t > worker_first_to;
		#endif //USE_THREADS
	};

	//tables[x] is the states before selecting a yarn for column x:
	std::vector< Table > tables;
	tables.reserve(image_width + 1); //okay, a lot of these are probably redundant, right?

	{ //build all valid states and the transition table between them.
		auto before = std::chrono::high_resolution_clock::now();

		//NOTE: with current code, a 6-yarn table build takes about 49sec; 5-yarn takes about 2.7sec

		{ //initial table:
			State init(yarns_linear.size());
			//mark all yarns as unused:
			for (auto &last_used : init.last_used) {
				last_used = 0;
			}
			//get the full cross_within limit:
			init.last_cross = 0;

			//(this is actually what the standard State() constructor already does)

			tables.emplace_back();
			tables.back().states.emplace_back(init);
		}

		for (uint32_t x = 0; x < image_width; ++x) {
			
			Table &prev = tables[x];
			tables.emplace_back();
			Table &next = tables[x+1];

			std::unordered_map< State, uint32_t > next_index;
			next_index.reserve(prev.states.size()*4);

			auto set_next_froms = [&](bool assert_on_add) {
				std::vector< std::vector< uint32_t > > next_froms;
				next_froms.reserve(prev.states.size() * yarns_linear.size());

				//account for already-indexed states:
				next_froms.resize(next_index.size());

				size_t total_froms = 0;

				for (uint32_t s = 0; s < prev.states.size(); ++s) {
					State const &state = prev.states[s];
					if (print_state_table && !assert_on_add) std::cout << "" << s << ":" << state << " ->";
					state.next_states(params, x, [&](uint32_t y, State const &next_state){
						auto ret = next_index.emplace(next_state, next_index.size());
						if (ret.second) {
							assert(!assert_on_add);
							next.states.emplace_back(next_state);
							next_froms.emplace_back();
							assert(next.states.size() == next_index.size());
							assert(next.states.size() == next_froms.size());
						}
						uint32_t to = ret.first->second;
						assert(next.states.at(to) == next_state);

						assert((to & STATE_MASK) == to); //state indices must be small enough to pack
						next_froms.at(to).emplace_back((y << YARN_SHIFT) | (s & STATE_MASK));
						total_froms += 1;

						if (print_state_table && !assert_on_add) std::cout << " " << to << ":" << next_state;
					});
					if (print_state_table && !assert_on_add) std::cout << std::endl;
				}
#if 0
				if (print_state_table && assert_on_add) {
					std::cout << "---- optimal perm (for figure) ---" << std::endl;
					assert(prev.states.size() == next.states.size());
					std::vector< uint32_t > test_position;
					test_position.reserve(prev.states.size());
					for (uint32_t i = 0; i < prev.states.size(); ++i) {
						test_position.emplace_back(i);
					}
					std::vector< uint32_t > best_position;
					uint32_t best_cost = -1U;
					do {
					/*
						bool symmetric = true;
						for (uint32_t to = 0; to < next_froms.size(); ++to) {
							State s = next.states.at(to);
						}
						*/
						uint32_t test_cost = 0;
						for (uint32_t to = 0; to < next_froms.size(); ++to) {
							for (auto const &from_ : next_froms[to]) {
								uint32_t from = from_ & STATE_MASK;
								int32_t d = int32_t(test_position.at(to)) - int32_t(test_position.at(from));
								test_cost += d*d;
							}
						}
						if (test_cost < best_cost) {
							best_cost = test_cost;
							best_position = test_position;
						}
					} while (std::next_permutation(test_position.begin(), test_position.end()));

					std::cout << "best cost: " << best_cost << std::endl;
					for (uint32_t s = 0; s < prev.states.size(); ++s) {
						State const &state = prev.states[s];
						std::cout << "" << best_position.at(s) << ":" << state << " ->";
						state.next_states(params, x, [&](uint32_t y, State const &next_state){
							auto ret = next_index.emplace(next_state, next_index.size());
							assert(!ret.second);
							uint32_t to = ret.first->second;
							assert(next.states.at(to) == next_state);
							std::cout << " " << best_position.at(to) << ":" << next_state;
						});
						std::cout << std::endl;
					}

				}
#endif				

				//collapse next_froms into a nice compact two-array format:
				assert(next.first_from.empty());
				assert(next.froms.empty());
				next.first_from.reserve(next.states.size() + 1);
				next.froms.reserve(total_froms);

				for (uint32_t to = 0; to < next.states.size(); ++to) {
					next.first_from.emplace_back(next.froms.size());

					next.froms.insert(next.froms.end(), next_froms[to].begin(), next_froms[to].end());

					//PARANOIA: they do always get added in order, right?
					for (uint32_t i = 0; i + 1 < next_froms[to].size(); ++i) {
						assert(next_froms[to][i] < next_froms[to][i+1]);
					}

				}
				next.first_from.emplace_back(next.froms.size());

				assert(next.froms.size() == total_froms);
				assert(next.first_from.size() == next.states.size() + 1);
			};

			//build the next states with whatever indices:
			set_next_froms(false);

			std::cout << "Table size at " << x << " is " << next.states.size() << std::endl;

			//if prev and next have the same states, set them up to have the same indices.
			//now the last table can loop with itself.
			if (prev.states.size() == next.states.size()) {
				std::unordered_set< State > prev_states(prev.states.begin(), prev.states.end());
				std::unordered_set< State > next_states(next.states.begin(), next.states.end());
				if (prev_states == next_states) {
					std::cout << "  this is the last table." << std::endl;
					//re-index this state:
					next_index.clear();
					next.states = prev.states;
					next_index.reserve(next.states.size());
					for (uint32_t to = 0; to < next.states.size(); ++to) {
						next_index[next.states[to]] = to;
					}

					next.first_from.clear();
					next.froms.clear();

					set_next_froms(true);

					break;
				}
			}

		}

		auto after = std::chrono::high_resolution_clock::now();
		std::cout << "Built " << tables.size() << " transition tables in " << std::chrono::duration< double >(after - before).count() * 1000.0 << "ms." << std::endl;
	}

	#if 0
		//TODO: do some sort of froms reporting on the tables like this mayhap:
			uint32_t most_froms = 0;
			uint32_t largest_range = 0;
			uint64_t average_range = 0;

			for (uint32_t s = 0; s < max_states; ++s) {
				state_first_from.emplace_back(state_froms.size());
				state_froms.insert(state_froms.end(), state_from[s].begin(), state_from[s].end());

				//PARANOIA: they do always get added in order, right?
				for (uint32_t i = 0; i + 1 < state_from[s].size(); ++i) {
					assert(state_from[s][i] < state_from[s][i+1]);
				}

				uint32_t min_from = max_states;
				uint32_t max_from = 0;
				for (uint32_t f : state_from[s]) {
					min_from = std::min(min_from, f & 0x00ffffff);
					max_from = std::max(max_from, f & 0x00ffffff);
				}

				if (max_from >= min_from) {
					largest_range = std::max< uint32_t >(largest_range, max_from + 1 - min_from);
					average_range += max_from + 1 - min_from;
				}
	
				most_froms = std::max< uint32_t >(most_froms, state_from[s].size());
			}
			state_first_from.emplace_back(state_froms.size());

			assert(state_first_from.size() == max_states + 1);

			std::cout << "Total froms: " << state_froms.size() << " of a possible " << max_states * yarns_linear.size() << std::endl;

			std::cout << "Most froms was " << most_froms << "; largest range spanned " << largest_range << ", average " << average_range / double(max_states) << std::endl;
		};

	#endif 

	
	auto before_dither = std::chrono::high_resolution_clock::now();

	//store dithered image here (as selected yarn indices):
	std::vector< uint8_t > dithered;
	dithered.reserve(image_width * image_height);

	#ifdef USE_THREADS

	//NOTE: 'froms' isn't used on the first table, so don't divide it:
	for (uint32_t t = 1; t < tables.size(); ++t) {
		Table &table = tables[t];

		//this is a heuristic -- running with too little work per thread just makes things slower because of synchronization delays;
		// so make sure each thread has at least 10000 froms to process.
		uint32_t divisions = std::max< uint32_t >(1, std::min< uint32_t >(job_queue.workers.size(), table.froms.size() / 10000) );

		if (params.max_threads != 0) {
			divisions = std::min(divisions, params.max_threads);
		}

		table.worker_first_to.emplace_back(0);
		uint32_t worker_froms = 0;
		for (uint32_t to = 0; to < table.states.size(); ++to) {
			uint32_t froms_begin = table.first_from.at(to);
			uint32_t froms_end = table.first_from.at(to+1);
			worker_froms += froms_end - froms_begin;
			if (worker_froms >= table.froms.size() / divisions || to + 1 == table.states.size()) {
				//std::cout << " Worker " << worker_first_to.size() << " will do [" << worker_first_to.back() << ", " << to+1 << ") -- " << worker_froms << " froms." << std::endl;
				table.worker_first_to.emplace_back(to+1);
				worker_froms = 0;
			}
		}
		//std::cout << "[table " << (&table - &tables[0]) << "] Dividing " << table.froms.size() << " state froms over " << divisions << " threads." << std::endl;
	}

	#endif //USE_THREADS

	std::mt19937 mt(params.seed);

	uint32_t random_choices = 0;


	//---- per-row ----
	Cost total_cost{0};
	for (uint32_t row = 0; row < image_height; ++row) {

		auto rv = [&](uint32_t max) -> uint32_t {
			if (max > 1) random_choices += 1;

			if (params.seed == 0) return 0;
			else if (params.seed == 1) return row % max;
			else return mt() % max; //n.b. not exactly uniform but close enough for this application
		};


		auto before = std::chrono::high_resolution_clock::now();

		std::cout << (row+1) << "/" << image_height << ":"; std::cout.flush();

		assert(!tables.empty());

		//store min cost to every state: (will be used for backtracking later)
		std::vector< std::vector< Cost > > min_costs;
		min_costs.reserve(image_width + 1);

		//first states get cost zero:
		min_costs.emplace_back(tables[0].states.size(), Cost{0});

		//remaining states start at inf and will be computed via min:
		for (uint32_t x = 0; x < image_width; ++x) {
			Table const &next = tables[std::min< uint32_t >(x + 1, tables.size()-1)];
			min_costs.emplace_back(next.states.size(), std::numeric_limits< Cost >::infinity());
		}
		assert(min_costs.size() == image_width + 1);


		for (uint32_t x = 0; x < image_width; ++x) { //for each column of the image:
			std::vector< Cost > yarn_costs;
			yarn_costs.reserve(yarns_linear.size());
			{ // (pre-)compute the costs of using each yarn here:
				Color::Linear px_color = image_linear[row*image_width+x];
				for (uint32_t y = 0; y < yarns_linear.size(); ++y) {
					Color::Linear yarn_color = yarns_linear[y];
					yarn_costs.emplace_back( difference(px_color, yarn_color) );
				}
			}
			assert(yarn_costs.size() == yarns_linear.size());

			Table const &prev = tables[std::min< uint32_t >(x, tables.size()-1)];
			Table const &next = tables[std::min< uint32_t >(x + 1, tables.size()-1)];

			#define PULL_VERSION
			#ifdef PULL_VERSION
			//"Pull version"
			//for every next state, pull cost forward
			{
				std::vector< Cost > const &prev_min_costs = min_costs.at(x);
				std::vector< Cost > &next_min_costs = min_costs.at(x+1);

				assert(prev_min_costs.size() == prev.states.size());
				assert(next_min_costs.size() == next.states.size());

				auto pull_costs = [&](uint32_t to_begin, uint32_t to_end){
					for (uint32_t to = to_begin; to < to_end; ++to) {
						uint32_t const *froms_begin = next.froms.data() + next.first_from[to];
						uint32_t const *froms_end = next.froms.data() + next.first_from[to+1];
						assert(froms_begin <= next.froms.data() + next.froms.size());

						Cost &next_min_cost = next_min_costs[to];
						for (uint32_t const *yarn_from = froms_begin; yarn_from != froms_end; ++yarn_from) {
							uint8_t y = *yarn_from >> YARN_SHIFT;
							uint32_t from = *yarn_from & STATE_MASK;
							Cost test_cost = prev_min_costs[from] + yarn_costs[y];
							if (test_cost < next_min_cost) {
								next_min_cost = test_cost;
							}
						}
					}
				};


				#ifdef USE_THREADS
				if (next.worker_first_to.size() <= 2) {
				#endif //USE_THREADS
					pull_costs(0, next_min_costs.size());
				#ifdef USE_THREADS
				} else {
					for (uint32_t w = 1; w < next.worker_first_to.size(); ++w) {
						uint32_t begin = next.worker_first_to[w-1];
						uint32_t end = next.worker_first_to[w];
						job_queue.run([&pull_costs,begin,end](){
							pull_costs(begin, end);
						});
					}
					job_queue.wait();
				}
				#endif //USE_THREADS

			}
			#endif //PULL_VERSION

		}

		if (min_costs.at(image_width).empty()) {
			std::cerr << " ERROR: no valid dither exists." << std::endl;
			exit(1);
		}

		auto before_readback = std::chrono::high_resolution_clock::now();

		//Now read off a minimum-cost path to the end state:
		{
			std::vector< uint32_t > possible_lowest;
			possible_lowest.emplace_back(0);
			for (uint32_t s = 1; s < min_costs[image_width].size(); ++s) {
				assert(!possible_lowest.empty());

				if (min_costs[image_width][s] < min_costs[image_width][possible_lowest[0]]) {
					possible_lowest.clear();
					possible_lowest.emplace_back(s);
				} else if (min_costs[image_width][s] == min_costs[image_width][possible_lowest[0]]) {
					possible_lowest.emplace_back(s);
				}
			}
			assert(!possible_lowest.empty());

			//std::cout << "Have " << possible_lowest.size() << " same-cost ending states." << std::endl;

			uint32_t lowest = possible_lowest[rv(possible_lowest.size())];

			std::cout << " cost " << min_costs[image_width][lowest]; std::cout.flush();

			uint32_t could_randomize = 0; //track when we might have a chance to do a random tiebreak between options

			std::vector< uint32_t > path;
			path.reserve(image_width+1);
			path.emplace_back(lowest);
			for (uint32_t x = image_width-1; x < image_width; --x) {
				Table const &prev = tables[std::min< uint32_t >(x, tables.size()-1)];
				Table const &next = tables[std::min< uint32_t >(x+1, tables.size()-1)];

				Cost best = std::numeric_limits< Cost >::infinity();
				std::vector< uint32_t > best_froms;
				for (uint32_t i = next.first_from.at(path.back()); i < next.first_from.at(path.back()+1); ++i) {
					uint32_t from = next.froms.at(i) & STATE_MASK;
					assert(from < prev.states.size());

					Cost test = min_costs[x].at(from);
					if (test < best) {
						best_froms.clear();
						best = test;
					}
					if (test == best) {
						best_froms.emplace_back(from);
					}
				}
				assert(!best_froms.empty());
				path.emplace_back( best_froms[rv(best_froms.size())] );
				if (best_froms.size() > 1) could_randomize += 1;
			}
			std::reverse(path.begin(), path.end());
			assert(path.size() == image_width+1);

			//std::cout << " had " << could_randomize << " tied costs"; //DEBUG

			Cost check_cost = 0.0;

			for (uint32_t x = 0; x < image_width; ++x) {
				Table const &prev = tables[std::min< uint32_t >(x, tables.size()-1)];
				Table const &next = tables[std::min< uint32_t >(x+1, tables.size()-1)];

				uint32_t s = path[x];
				uint32_t s_next = path[x+1];

				//read off what yarn was used in the state:
				uint8_t y = 0xff;
				prev.states.at(s).next_states(params, x, [&](uint32_t i, State const &next_state){
					if (next_state == next.states.at(s_next)) {
						assert(y == 0xff); //should only have one used yarn
						y = i;
					}
				});
				assert(y != 0xff); //should have at least one used yarn
				dithered.emplace_back(y); //store in output
				//std::cout << char('A' + y); std::cout.flush();

				Color::Linear px_color = image_linear[row*image_width+x];
				Color::Linear yarn_color = yarns_linear[y];
				check_cost += difference(px_color, yarn_color);
			}

			//apply error diffusion (if parameters say so):
			error_diffusion(params, row, dithered, &image_linear);

			//these should be *identical*, even given floating point rounding -- same numbers added in the same order:
			assert(min_costs[image_width][lowest] == check_cost);

			assert(dithered.size() == (row + 1) * image_width); //wrote enough pixels to the output, right?


			//accumulate for later total cost display
			total_cost += min_costs[image_width][lowest];

			/*
			{ //PARANOIA: check max_float and max_crossing:
				std::vector< uint32_t > last_used(yarns_linear.size(), 0); //how many needles since yarn use
				uint32_t last_crossing = 0; //how many needles since a crossing
				std::cout << "\n";
				for (uint32_t x = 0; x < image_width; ++x) {
					uint8_t y = dithered[row * image_width + x];
					assert(y < yarns_linear.size());

					std::cout << x << ": " << char('a' + y) << " " << states[path[x]] << " -> ";
					for (uint32_t i = 0; i < yarns_linear.size(); ++i) {
						uint32_t to = state_to[path[x] * yarns_linear.size() + i];
						if (to != -1U) {
							std::cout << " " << states[to];
						}
					}
					std::cout << std::endl;

					//increment last used position for all yarns:
					for (uint32_t &u : last_used) u += 1;

					//update time since crossing:
					last_crossing += 1;
					if (last_used[y] % 2 == 1) last_crossing = 0;

					//yarn used at this needle gets reset to zero:
					last_used[y] = 0;

					std::cout << "  calc: [";
					for (uint32_t &u : last_used) {
						if (&u != &last_used[0]) std::cout << ',';
						std::cout << int(u);
					}
					std::cout << "]x" << int(last_crossing) << std::endl;

					//record float info:
					for (uint32_t &u : last_used) {
						assert(u <= max_float);
					}
					//record crossing info:
					assert(last_crossing <= max_crossing);
				}
			}
			*/


			
		}

		auto after = std::chrono::high_resolution_clock::now();
		std::cout << " (" <<  std::chrono::duration< double >(before_readback - before).count() * 1000 << "ms";
		std::cout << " + " <<  std::chrono::duration< double >(after - before_readback).count() * 1000 << "ms";
		std::cout << " = " <<  std::chrono::duration< double >(after - before).count() * 1000 << "ms)" << std::endl;
	}

	auto after_dither = std::chrono::high_resolution_clock::now();

	std::cout << "Overall, made " << random_choices << " arbitrary choices among equal-cost alternatives." << std::endl;

	std::cout << "Dither completed in " <<  std::chrono::duration< double >(after_dither - before_dither).count() * 1000 << "ms." << std::endl;

	return dithered;
}
