
#include "Color.hpp"
#include "Cost.hpp"
#include "dither.hpp"

#define STBI_ONLY_PNG
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <chrono>
#include <map>
#include <set>

int main(int argc, char **argv) {
	bool mirror_back = true;

	std::string in_front_png = "";
	std::string in_back_png = "";
	std::string in_png = "";

	std::string yarns_png = "";
	uint32_t select_yarns = 0;

	std::vector< Color::Linear > temp_vec_linear; //because DitherParams needs something to refer to
	LinearDifference temp_difference; //because DitherParams needs something to refer to
	DitherParams default_params{.yarns_linear=temp_vec_linear, .image_linear=temp_vec_linear, .difference=temp_difference};

	uint32_t cross_within = default_params.cross_within;
	uint32_t use_within = default_params.use_within;
	uint32_t seed = default_params.seed;
	uint32_t max_threads = default_params.max_threads;

	std::string out_front_png = "";
	std::string out_back_png = "";
	std::string out_png = "";

	bool diffuse = true;
	SRGBDifference srgb_difference;
	LinearDifference linear_difference;
	OKLabDifference oklab_difference;
	DemoDifference demo_difference;
	std::vector< Difference const * > differences{
		&srgb_difference,
		&linear_difference,
		&oklab_difference,
		&demo_difference
	};
	Difference const *difference = &oklab_difference;

	typedef std::vector< uint8_t > (*DitherFn)(DitherParams const &);

	DitherFn method = optimal_dither;

	std::vector< std::pair< std::string, DitherFn > > methods;
	methods.emplace_back("optimal", optimal_dither);
	methods.emplace_back("greedy", greedy_dither);

	{ //parse command line options:
		bool usage = false;
		try {
			for (int argi = 1; argi < argc; ++argi) {
				std::string arg(argv[argi]);
				if (arg == "--help") {
					usage = true;
				} else if (arg == "--in-front") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--in-front' must be followed by a filename.");
					in_front_png = argv[++argi];
				} else if (arg == "--in-back") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--in-back' must be followed by a filename.");
					in_back_png = argv[++argi];
				} else if (arg == "--in") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--in' must be followed by a filename.");
					in_png = argv[++argi];
				} else if (arg == "--yarns") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--yarns' must be followed by a filename.");
					yarns_png = argv[++argi];
				} else if (arg == "--select-yarns") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--select-yarns' must be followed by a non-negative integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> select_yarns) || (iss >> junk)) throw std::runtime_error("Failed to parse a non-negative integer from '" + val + "'.");
				} else if (arg == "--out-front") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--out-front' must be followed by a filename.");
					out_front_png = argv[++argi];
				} else if (arg == "--out-back") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--out-back' must be followed by a filename.");
					out_back_png = argv[++argi];
				} else if (arg == "--out") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--out' must be followed by a filename.");
					out_png = argv[++argi];
				/*
				} else if (arg == "--max-float") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--max-float' must be followed by a positive integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> max_float) || (iss >> junk)) throw std::runtime_error("Failed to parse a positive integer from '" + val + "'.");
				} else if (arg == "--max-crossing") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--max-crossing' must be followed by a positive integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> max_crossing) || (iss >> junk)) throw std::runtime_error("Failed to parse a positive integer from '" + val + "'.");
				*/
				} else if (arg == "--use-within") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--use-within' must be followed by a non-negative integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> use_within) || (iss >> junk)) throw std::runtime_error("Failed to parse a non-negative integer from '" + val + "'.");
				} else if (arg == "--cross-within") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--cross-within' must be followed by a non-negative integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> cross_within) || (iss >> junk)) throw std::runtime_error("Failed to parse a non-negative integer from '" + val + "'.");
				} else if (arg == "--seed") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--seed' must be followed by a non-negative integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> seed) || (iss >> junk)) throw std::runtime_error("Failed to parse a non-negative integer from '" + val + "'.");
				} else if (arg == "--max-threads") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--max-threads' must be followed by a non-negative integer.");
					std::string val = argv[++argi];
					std::istringstream iss(val);
					char junk = '\0';
					if (!(iss >> max_threads) || (iss >> junk)) throw std::runtime_error("Failed to parse a non-negative integer from '" + val + "'.");
				} else if (arg == "--cost") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--cost' must be followed by a string.");
					std::string val = argv[++argi];
					Difference const *found = nullptr;
					for (Difference const *d : differences) {
						if (d->name() == val) {
							assert(found == nullptr);
							found = d;
						}
					}
					if (!found) throw std::runtime_error("Unrecognized cost '" + val + "'.");
					difference = found;
				} else if (arg == "--method") {
					if (argi + 1 >= argc) throw std::runtime_error("Argument '--method' must be followed by a string.");
					std::string val = argv[++argi];
					bool found = false;
					for (auto const &nf : methods){
						if (nf.first == val) {
							assert(!found);
							found = true;
							method = nf.second;
						}
					}
					if (!found) throw std::runtime_error("Unrecognized method '" + val + "'.");
				} else if (arg == "--diffuse") {
					diffuse = true;
				} else if (arg == "--no-diffuse") {
					diffuse = false;
				} else {
					throw std::runtime_error("Unrecognized argument '" + arg + "'.");
				}
			}
		} catch (std::exception &e) {
			std::cerr << "ERROR: " << e.what() << std::endl;
			usage = true;
		}

		if (!( (in_png != "" && in_front_png == "" && in_back_png == "")
			|| (in_png == "" && in_front_png != "" && in_back_png != "") )) {
			std::cerr << "ERROR: must either specify '--in' or specify both of '--in-front' and '--in-back'." << std::endl;
			usage = true;
		}

		if (yarns_png == "") {
			std::cerr << "ERROR: must specify yarn colors via '--yarns' argument." << std::endl;
			usage = true;
		}

		if (out_png == "" && out_front_png == "" && out_back_png == "") {
			std::cerr << "ERROR: please specify at least one of `--out`, `--out-front`, and `--out-back`." << std::endl;
			usage = true;
		}

		if (usage) {
			std::cerr << "Usage:\n"
			"    optimal-dither --in <in.png> --yarns <yarns.png> --out <out.png> [...]\n"
			" Input Image (specify --in or --in-front and --in-back):\n"
			"   --in <in.png> (filename) -- input image, interleaved front/back needles. Must be even width.\n"
			"   --in-front <in-front.png> (filename) -- input image, front only (must also specify in-back).\n"
			"   --in-back <in-back.png> (filename) -- input image, back only (must also specify in-front).\n"
			" Yarn Colors: (required)\n"
			"   --yarns <yarns.png> -- yarn colors, packed into an image (where pixel (0,y) gives the yarn color.)\n"
			"   --select-yarns <Y> -- pick only Y of the yarn colors, minimizes quantization error (but doesn't run full dither with every option).\n"
			" Output Image (specify at least one):\n"
			"   --out <out.png> (filename) -- output image, interleaved front/back needles.\n"
			"   --out-front <out-front.png> (filename) -- output image, front only.\n"
			"   --out-back <out-back.png> (filename) -- output image, back only.\n"
			" Dithering Options:\n"
			//"   --max-float <F> (integer >= 0, default " << MAX_FLOAT_DEFAULT << ", 0 disables) -- longest number of needles that can be floated over by any yarn.\n"
			//"   --max-crossing <C> (integer >= 0, default " << MAX_CROSSING_DEFAULT << ", 0 disables) -- longest distance allowed between bed crossings.\n";
			"   --use-within <U> (integer >= 0, default " << default_params.use_within << ", 0 disables) -- require every U stitches to contain at least one use of every yarn.\n"
			"   --cross-within <X> (integer >= 0, default " << default_params.cross_within << ", 0 disables) -- require every X stitches to contain at least one front and back use of the same yarn.\n"
			"   --seed <S> (integer >= 0, default " << default_params.seed << ", 0 always picks first, 1 always picks based on row) -- set the seed for the pseudo-random numbers used to pick between same-cost paths.\n"
			"   --max-threads <T> (integer >= 0, default " << default_params.max_threads << ", 0 picks automatically) -- limit the number of compute threads.\n";

			std::cerr << "   --cost <";
			for (Difference const *d : differences) {
				if (d != differences[0]) std::cerr << '|';
				std::cerr << d->name();
			}
			std::cerr << "> (default oklab) -- distance used to compute quantization cost.\n";

			std::cerr << "   --method <";
			for (auto const &nf : methods) {
				if (&nf != &methods[0]) std::cerr << '|';
				std::cerr << nf.first;
			}
			std::cerr << "> (default optimal) -- method used to [attempt to] optimize cost.\n";


			std::cerr <<
			"   --diffuse / --no-diffuse (default is to diffuse) -- should quantization error be diffused to later rows.\n"
			;
			std::cerr.flush();
			return 1;
		}
	}

	//note, these are in 0xAABBGGRR order:
	std::vector< uint32_t > yarns;

	{
		int width, height, channels;
		uint8_t *data = stbi_load(yarns_png.c_str(), &width, &height, &channels, 4);
		
		if (data == NULL) {
			std::cerr << "ERROR: failed to load png from '" << yarns_png << "': " << stbi_failure_reason() << std::endl;
			return 1;
		}

		if (width <= 0 || height <= 0) {
			std::cerr << "ERROR: yarns image has non-positive width and/or height (" << width << "x" << height << ")." << std::endl;
			return 1;
		}

		yarns.resize(width * height);

		std::memcpy(yarns.data(), data, width * height * 4);

		stbi_image_free(data);
	}

	//if not specified, use all specified yarns:
	if (select_yarns == 0) select_yarns = yarns.size();

	if (select_yarns > yarns.size()) {
		std::cerr << "WARNING: cannot select " << select_yarns << " yarns from a list of only " << yarns.size() << " yarns; will just use all the yarns." << std::endl;
		select_yarns = yarns.size();
	}

	std::vector< Color::Linear > yarns_linear = Color::srgb_to_linear(yarns);

	if (use_within != 0 && use_within < select_yarns) {
		std::cerr << "ERROR: use_within (" << use_within << ") should be no smaller than the yarns count (" << select_yarns << "), otherwise it is impossible to form an image." << std::endl;
		return 1;
	}

	if (select_yarns > 32) {
		std::cerr << "WARNING: using " << select_yarns << " yarns; is likely to either out-of-memory or at least result in some bugs in state indexing. (Continuing anyway, but expect crashes/bugs.)" << std::endl;
	}

	std::vector< uint32_t > image;
	uint32_t image_width = 0;
	uint32_t image_height = 0;
	
	if (in_png != "") { //load the input image:
		int width, height, channels;
		uint8_t *data = stbi_load(in_png.c_str(), &width, &height, &channels, 4);

		if (data == NULL) {
			std::cerr << "ERROR: failed to load png from '" << in_png << "': " << stbi_failure_reason() << std::endl;
			return 1;
		}

		//check image dimensions:
		if (width <= 0 || width % 2 != 0) {
			std::cerr << "ERROR: input image must be of positive even width (got " << width << ")." << std::endl;
			return 1;
		}
		if (height <= 0) {
			std::cerr << "ERROR: input image must be of positive height (got " << height << ")." << std::endl;
		}

		image_width = uint32_t(width);
		image_height = uint32_t(height);

		image.resize(image_width * image_height);

		std::memcpy(image.data(), data, image_width * image_height * 4);

		stbi_image_free(data);

		std::cout << "Input image '" << in_png << "' (front/back interleaved) is of size " << image_width << "x" << image_height << std::endl;
	} else {
		assert(in_front_png != "" && in_back_png != "");

		int width_front, height_front, channels_front;
		uint8_t *data_front = stbi_load(in_front_png.c_str(), &width_front, &height_front, &channels_front, 4);

		if (data_front == NULL) {
			std::cerr << "ERROR: failed to load png from '" << in_front_png << "': " << stbi_failure_reason() << std::endl;
			return 1;
		}

		int width_back, height_back, channels_back;
		uint8_t *data_back = stbi_load(in_back_png.c_str(), &width_back, &height_back, &channels_back, 4);

		if (data_back == NULL) {
			std::cerr << "ERROR: failed to load png from '" << in_front_png << "': " << stbi_failure_reason() << std::endl;
			return 1;
		}

		if (width_front != width_back || height_front != height_back) {
			std::cerr << "ERROR: front image (" << width_front << "x" << height_front << ") and back image (" << width_back << "x" << height_back << ") are not the same size." << std::endl;
			return 1;
		}

		if (width_front <= 0 || height_front <= 0) {
			std::cerr << "ERROR: images must be of positive width and height, got (" << width_front << "x" << height_front << ")." << std::endl;
			return 1;
		}

		image_width = uint32_t(width_front + width_back);
		image_height = uint32_t(height_front);
		image.resize(image_width * image_height);

		//interleave the image columns, fbfbfb...
		for (uint32_t y = 0; y < image_height; ++y) {
			for (uint32_t x = 0; x < image_width; ++x) {
				if (x % 2 == 0) {
					memcpy(&image[y * image_width + x], &data_front[(y * width_front + x/2)*4], 4);
				} else {
					if (mirror_back) {
						//back image mirrored left-right to account for viewing from the other side:
						memcpy(&image[y * image_width + x], &data_back[(y * width_back + width_back-1-x/2)*4], 4);
					} else {
						memcpy(&image[y * image_width + x], &data_back[(y * width_back + x/2)*4], 4);
					}
				}
			}
		}

		stbi_image_free(data_front);
		stbi_image_free(data_back);

		std::cout << "Input images '" << in_front_png << "' (front) and '" << in_back_png << "' (back) interleave to an image of size " << image_width << "x" << image_height << std::endl;
	}

	std::vector< Color::Linear > image_linear = Color::srgb_to_linear(image);

	std::cout << "------------------------------------\n";
	std::cout << " Dithering a " << image_width << "x" << image_height << " image to " << select_yarns << " of " << yarns.size() << " yarns.\n";
	std::cout << " Using method '";
	for (auto const &nf : methods) {
		if (method == nf.second) std::cout << nf.first;
	}
	std::cout << "'.\n";
	std::cout << " Use within is " << use_within << (use_within == 0 ? " (disabled)" : "")
	          << " and cross within is " << cross_within << (cross_within == 0 ? " (disabled)" : "") << ".\n";
	std::cout << " Cost function is '" << difference->name() << "' -- " << difference->help() << ".\n";
	std::cout << " Random seed is " << seed << ".\n";
	std::cout << " Will use up to " << max_threads << (max_threads == 0 ? " (auto)" : "") << " threads.\n";
	if (diffuse) std::cout << " Error will be diffused to the next row.\n";
	else std::cout << " No error diffusion will be used.\n";
	std::cout << "------------------------------------\n";


	
	if (select_yarns < yarns.size()) { //Estimate the optimal subset of yarns based on quantization error without accounting for error diffusion or fabrication constraints:
		std::cout << "Determining subset of yarn colors by trying all without constraints:" << std::endl;

		//permutations of selected correspond to subsets of the yarns:
		std::vector< uint8_t > selected(yarns.size(), 0);
		for (uint32_t i = yarns.size() - select_yarns; i < yarns.size(); ++i) {
			selected[i] = 1;
		}

		std::cout << "  Precomputing all costs..."; std::cout.flush();
		//precompute costs from all yarns to all pixels:
		std::vector< std::vector< Cost > > yarn_px_costs(yarns.size());
		for (uint32_t y = 0; y < yarns.size(); ++y) {
			yarn_px_costs[y].reserve(image.size());
			Color::Linear const &yarn_color = yarns_linear[y];
			for (uint32_t i = 0; i < image.size(); ++i) {
				Color::Linear const &px_color = image_linear[i];
				yarn_px_costs[y].emplace_back( (*difference)(px_color, yarn_color) );
			}
		}
		std::cout << " done." << std::endl;

		Cost min_cost = std::numeric_limits< float >::infinity();
		std::vector< uint8_t > min_selected(yarns.size(), 0);

		uint32_t combinations = 0;

		std::cout << "  Trying all combinations... "; std::cout.flush();
		do {
			std::vector< Cost const * > to_min;
			to_min.reserve(select_yarns);
			for (uint32_t y = 0; y < selected.size(); ++y) {
				if (selected[y] != 0) {
					to_min.emplace_back(yarn_px_costs[y].data());
				}
			}
			assert(to_min.size() == select_yarns);

			Cost total_cost = 0;
			for (uint32_t i = 0; i < image.size(); ++i) {
				Cost px_min = to_min[0][i];
				for (uint32_t y = 1; y < to_min.size(); ++y) {
					px_min = std::min(px_min, to_min[y][i]);
				}
				total_cost += px_min;
				//if (total_cost > min_cost) break; //early out for really bad choices
			}
			if (total_cost < min_cost) {
				min_cost = total_cost;
				min_selected = selected;
			}

			++combinations;
		} while (std::next_permutation(selected.begin(), selected.end()));

		std::cout << " done. (" << combinations << " total.)" << std::endl;

		std::cout << "  Selected:\n";

		std::vector< uint32_t > min_yarns;
		std::vector< Color::Linear > min_yarns_linear;
		for (uint32_t y = 0; y < min_selected.size(); ++y) {
			if (min_selected[y] != 0) {
				min_yarns.emplace_back(yarns[y]);
				min_yarns_linear.emplace_back(yarns_linear[y]);

				uint32_t c = yarns[y];
				const char hex[] = "0123456789abcdef";
				std::cout << "    " << (y+1) << ": 0x"
					<< hex[(c >>  4) & 0xf]
					<< hex[ c        & 0xf]
					<< hex[(c >> 12) & 0xf]
					<< hex[(c >>  8) & 0xf]
					<< hex[(c >> 20) & 0xf]
					<< hex[(c >> 16) & 0xf]
					<< "\n";
			}
		}
		std::cout << "  Estimated cost (without fabrication limits or error diffusion): " << min_cost << std::endl;

		yarns = min_yarns;
		yarns_linear = min_yarns_linear;
	}
	assert(yarns.size() == select_yarns);
	assert(yarns.size() == yarns_linear.size());


	DitherParams params{
		.yarns_linear=yarns_linear,
		.image_width=image_width,
		.image_height=image_height,
		.image_linear=image_linear,
		.use_within=use_within,
		.cross_within=cross_within,
		.difference=*difference,
		.diffuse=diffuse,
		.seed=seed,
		.max_threads=max_threads,
	};

	std::vector< uint8_t > dithered;

	dithered = method(params);

	assert(dithered.size() == image.size());

	bool invalid_image = false;
	
	{ //CHECK the resulting dither:
		uint32_t longest_no_use = 0;
		uint32_t longest_no_crossing = 0;
		Cost total_cost = 0;
		for (uint32_t row = 0; row < image_height; ++row) {
			bool error_row = false;
			for (uint32_t x = 0; x < image_width; ++x) {

				//these checks are written to be simple, not to be efficient

				//check longest window starting at x that doesn't use all yarns:
				std::set< uint8_t > used_yarns; //which yarns have been seen in the window
				uint32_t uw = 0;
				for (uint32_t x2 = x; x2 < image_width; ++x2) {
					//add yarns until all have been seen:
					uint8_t y = dithered[row * image_width + x2];
					assert(y < yarns_linear.size());
					used_yarns.emplace(y);
					if (used_yarns.size() == yarns_linear.size()) break;
					uw = x2+1-x;

					longest_no_use = std::max(longest_no_use, uw);
				}
				if (use_within != 0 && uw+ 1 > use_within) error_row = true;

				//check longest window starting at x that doesn't contain a crossing:
				std::map< uint8_t, uint32_t > last_use;
				uint32_t cw = 0;
				for (uint32_t x2 = x; x2 < image_width; ++x2) {
					uint8_t y = dithered[row * image_width + x2];
					assert(y < yarns_linear.size());
					//check for crossing
					auto f = last_use.find(y);
					if (f != last_use.end()) {
						uint32_t span = x2 - f->second;
						if (span % 2 != 0) {
							//have a crossing
							break;
						}
					}
					//remember this use of the yarn:
					last_use[y] = x2;
					
					cw = x2+1-x;
					longest_no_crossing = std::max(longest_no_crossing, cw);
				}
				if (cross_within != 0 && cw+1 > cross_within) error_row = true;

				//DEBUG:
				//std::cout << x << " " << char('a' + dithered[row * image_width + x]) << " " << uw << " " << cw;
				//if (uw + 1 > use_within || cw + 1 > cross_within) std::cout << "  ***";
				//std::cout << std::endl;

				//accumulate cost:
				total_cost += (*difference)(yarns_linear[dithered[row * image_width + x]], image_linear[row * image_width + x]);
			}

			/*//DEBUG:
			if (error_row) {
				for (uint32_t x = 0; x < image_width; ++x) {
					std::cout << char('a' + dithered[row * image_width + x]);
				}
				std::cout << std::endl;
				break;
			}
			*/
		}
		std::cout << "Shortest window with all yarns being used is " << longest_no_use + 1 << " (requested: " << use_within << ")." << std::endl;
		std::cout << "Shortest window which always has a crossing is " << longest_no_crossing + 1 << " (requested: " << cross_within << ")." << std::endl;
		std::cout << "Total cost of dither was " << total_cost << std::endl;

		if (use_within != 0 && longest_no_use + 1 > use_within) {
			std::cerr << "ERROR: requested use-within " << use_within << " but output has use-within of " << longest_no_use + 1 << std::endl;
			invalid_image = true;
		}
		if (cross_within != 0 && longest_no_crossing + 1 > cross_within) {
			std::cerr << "ERROR: requested cross-within " << cross_within << " but output has cross-within of " << longest_no_crossing + 1 << std::endl;
			invalid_image = true;
		}

		if (invalid_image) {
			std::cerr << "********* VALIDATION ERROR, not writing output image ***********" << std::endl;
			return 1;
		}

	}


	//output images:

	bool write_failed = false;
	if (out_png != "") {
		std::vector< uint32_t > out;
		out.reserve(image_width * image_height);
		for (uint32_t y = 0; y < image_height; ++y) {
			for (uint32_t x = 0; x < image_width; ++x) {
				out.emplace_back(yarns[dithered[y*image_width+x]]);
			}
		}

		if (stbi_write_png(out_png.c_str(), int(image_width), int(image_height), 4, out.data(), int(image_width*4)) == 0) {
			std::cerr << "ERROR: failed to write '" << out_png << "'." << std::endl;
			write_failed = true;
		} else {
			std::cout << "wrote interleaved front/back yarns to '" << out_png << "'." << std::endl;
		}
	}
	if (out_front_png != "") {
		std::vector< uint32_t > out;
		out.reserve(image_width/2 * image_height);
		for (uint32_t y = 0; y < image_height; ++y) {
			for (uint32_t x = 0; x < image_width; x += 2) {
				out.emplace_back(yarns[dithered[y*image_width+x]]);
			}
		}

		if (stbi_write_png(out_front_png.c_str(), int(image_width/2), int(image_height), 4, out.data(), int(image_width/2*4)) == 0) {
			std::cerr << "ERROR: failed to write '" << out_front_png << "'." << std::endl;
			write_failed = true;
		} else {
			std::cout << "wrote front yarns to '" << out_front_png << "'." << std::endl;
		}
	}
	if (out_back_png != "") {
		std::vector< uint32_t > out;
		out.reserve(image_width/2 * image_height);
		for (uint32_t y = 0; y < image_height; ++y) {
			for (uint32_t x = 0; x < image_width; x += 2) {
				if (mirror_back) {
					out.emplace_back(yarns[dithered[y*image_width+image_width-1-x]]);
				} else {
					out.emplace_back(yarns[dithered[y*image_width+x+1]]);
				}
			}
		}

		if (stbi_write_png(out_back_png.c_str(), int(image_width/2), int(image_height), 4, out.data(), int(image_width/2*4)) == 0) {
			std::cerr << "ERROR: failed to write '" << out_front_png << "'." << std::endl;
			write_failed = true;
		} else {
			std::cout << "wrote back yarns to '" << out_back_png << "'." << std::endl;
		}
	}
	if (write_failed) {
		return 1;
	}


	return 0;
}
