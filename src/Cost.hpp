#pragma once

#include "Color.hpp"

#include <string>

typedef float Cost;

struct Difference {
	virtual Cost operator()(Color::Linear const &a, Color::Linear const &b) const = 0;
	virtual std::string name() const = 0;
	virtual std::string help() const = 0;
};

struct SRGBDifference : Difference {
	virtual Cost operator()(Color::Linear const &a_, Color::Linear const &b_) const override {
		struct SRGB { float r,g,b; } a,b;
		a_.to_srgb_clamped(&a.r, &a.g, &a.b);
		b_.to_srgb_clamped(&b.r, &b.g, &b.b);
		return  (a.r-b.r)*(a.r-b.r)
		      + (a.g-b.g)*(a.g-b.g)
		      + (a.b-b.b)*(a.b-b.b);
	}
	virtual std::string name() const override { return "srgb"; }
	virtual std::string help() const override { return "squared difference of srgb-encoded color values (component values in range [0,1])"; }
};

struct LinearDifference : Difference {
	virtual Cost operator()(Color::Linear const &a, Color::Linear const &b) const override {
		return  (a.r-b.r)*(a.r-b.r)
		      + (a.g-b.g)*(a.g-b.g)
		      + (a.b-b.b)*(a.b-b.b);
	}
	virtual std::string name() const override { return "linear"; }
	virtual std::string help() const override { return "squared difference of linear rgb color values (component values in range [0,1])"; }
};

struct OKLabDifference : Difference {
	virtual Cost operator()(Color::Linear const &a_, Color::Linear const &b_) const override {
		Color::OKLab a = Color::OKLab::from_linear(a_);
		Color::OKLab b = Color::OKLab::from_linear(b_);

		return  (a.L-b.L)*(a.L-b.L)
		      + (a.a-b.a)*(a.a-b.a)
		      + (a.b-b.b)*(a.b-b.b);
	}
	virtual std::string name() const override { return "oklab"; }
	virtual std::string help() const override { return "squared difference of linear Oklab color values (component values in range [0,1])"; }
};


struct DemoDifference : Difference {
	virtual Cost operator()(Color::Linear const &a_, Color::Linear const &b_) const override {
		auto to_grey = [](Color::Linear const &c) -> float {
			if (c.r < 0.1f && c.g < 0.1f && c.b < 0.1f) {
				return 0.0f;
			} else if (c.r > 0.9f && c.g > 0.9f && c.b > 0.9f) {
				return 2.0f;
			} else {
				return 1.0f;
			}
		};
		float a = to_grey(a_);
		float b = to_grey(b_);
		return (b-a)*(b-a);
	}
	virtual std::string name() const override { return "demo"; }
	virtual std::string help() const override { return "distance for bw yarn <-> bwg image; always in {0,1,4}"; }
};
