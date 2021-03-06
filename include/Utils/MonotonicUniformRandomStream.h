#pragma once

#include <random>

#include <stxxl/bits/common/utils.h>
#include <stxxl/bits/common/seed.h>

template <bool Increasing = true>
class MonotonicUniformRandomStream {
public:
    using value_type = double;

private:
    using T = double; // internal floating point rep
    std::mt19937_64 _randomGen;
    std::uniform_real_distribution<double> _randDistr;

    uint_t _elements_left;
    bool _empty;
    value_type _current;

public:
    MonotonicUniformRandomStream(uint_t elements, unsigned int seed = stxxl::get_next_seed())
        : _randomGen(seed), _randDistr(0.0, 1.0),
          _elements_left(elements), _empty(!elements), _current(Increasing ? 0.0 : 1.0)
    {++(*this);}

    MonotonicUniformRandomStream& operator++() {
        assert(!_empty);
        if (UNLIKELY(!_elements_left)) {
            _empty = true;
        } else {
            if (Increasing) {
                _current = T(1.0) - (1.0 - _current) * std::pow(T(1.0) - _randDistr(_randomGen), 1.0 / T(_elements_left));
            } else {
                _current *= std::pow(T(1.0) - _randDistr(_randomGen), 1.0 / T(_elements_left));
            }
            _elements_left--;
        }

        return *this;
    }

    const value_type& operator * () const {
        return _current;
    };

    bool empty() const {
        return _empty;
    };
};
