#pragma once

#include <iostream>
#include <cassert>
#include <string>

enum PQSorterMergerSourceType {SrcPriorityQueue, SrcSorter};


/***
 * Merge a STXXL Sorter and Priority Queue
 *
 * In case a large portion of data in a PQ is computed prior to reading access,
 * or if there are multiple passes over a static data sequence, it can be beneficial
 * to use a sorter for this offline data.
 *
 * The value type and comparator are derived from the PQ.
 */
template <class PQ, class Sorter, bool compute_stats = false, class comp_type = typename PQ::comparator_type>
class PQSorterMerger {
public:
    using value_type = typename PQ::value_type;

private:
    using Comp = comp_type;
    PQ& _pq;
    Sorter& _sorter;
    Comp _comp;

    PQSorterMergerSourceType _value_src;
    value_type  _value;

    uint64_t _elements_from_pq;
    uint64_t _elements_from_sorter;

    void _fetch() {
        assert(!empty());

        // in case one source is empty, we cannot safely use the comparator
        if (UNLIKELY(_pq.empty())) {
            _value = *_sorter;
            _value_src = SrcSorter;
        } else if (UNLIKELY(_sorter.empty())) {
            _value = _pq.top();
            _value_src = SrcPriorityQueue;
        } else if (LIKELY(_comp(_pq.top(), *_sorter))) {
           // in typical use-case, the PQ is used less than the sorter
            _value = *_sorter;
            _value_src = SrcSorter;
        } else {
            _value = _pq.top();
            _value_src = SrcPriorityQueue;
        }
    }

    uint64_t _max_elem_in_pq;
    uint64_t _avg_elem_in_pq;
    uint64_t _num_updates;

public:
    PQSorterMerger() = delete;

    PQSorterMerger(PQ & pq, Sorter & sorter, bool initialize = true) :
          _pq(pq), _sorter(sorter), _elements_from_pq(0), _elements_from_sorter(0),
          _max_elem_in_pq(0), _avg_elem_in_pq(0), _num_updates(0)
    {
        if (initialize)
            update();
    }

    //! Call in case the PQ/Sorter are changed externally
    void update() {
        if (LIKELY(!empty()))
            _fetch();

        if (compute_stats) {
            _max_elem_in_pq = std::max<uint64_t>(_max_elem_in_pq, _pq.size());
            _avg_elem_in_pq += _pq.size();
            _num_updates++;
        }
    }

    //! Push an item into the PQ and update the merger
    void push(const value_type& o) {
        _pq.push(o);
        _fetch();
    }

    //! Returns true if PQ and Sorter are empty
    bool empty() const {
        return _pq.empty() && _sorter.empty();
    }

    //! Removes the smallest element from its source and fetches
    //! next (if availble)
    //! @note Call only if sorter is in output mode and empty() == false
    PQSorterMerger& operator++() {
        assert(!empty());
        if (_value_src == SrcPriorityQueue) {
            _pq.pop();
            if (compute_stats) _elements_from_pq++;
        } else {
            ++_sorter;
            if (compute_stats) _elements_from_sorter++;
        }

        if (LIKELY(!empty()))
            _fetch();

        return *this;
    }

    //! Returns reference to the smallest item
    //! @note Access only if not empty
    const value_type & operator*() const {
        assert(!empty());
        return _value;
    }
    
    const PQSorterMergerSourceType & source() const {
        assert(!empty());
        return _value_src;
    }

    //! If compute_stats=true output statistics to STDOUT
    void dump_stats(const std::string label="") const {
        if (!compute_stats)
            return;

        std::string my_label;
        if (!label.empty())
           my_label = label + ": ";

        auto elements_tot = _elements_from_pq + _elements_from_sorter;

        double avg = static_cast<double>(_avg_elem_in_pq) / std::max<uint64_t>(1, _num_updates);
        size_t sze = sizeof(_pq.top());

        std::cout << my_label
                  << "Elements consumed: " << elements_tot
                  << " from PQ: " << (_elements_from_pq) << " (" << (100.0 * _elements_from_pq / elements_tot) << "%)"
                     " from Sorter: " << (_elements_from_sorter) << " (" << (100.0 * _elements_from_sorter / elements_tot) << "%)"
                  "\n"
                  << my_label
                  << "Max elems in PQ: " << _max_elem_in_pq 
                  << ", each " << sze << " bytes. In total: " << (sze * _max_elem_in_pq) << " bytes."
                  "\n"
                  << my_label
                  << "Avg elems in PQ: " << avg 
                  << ", each " << sze << " bytes. In total: " << (sze * avg) << " bytes."
        << std::endl;
    }
};
