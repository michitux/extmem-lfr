#pragma once

#include <stdlib.h>

#include <limits>
#include <random>

#include <stxxl/vector>
#include <stxxl/sorter>
#include <stxxl/bits/common/uint_types.h>

#include <defs.h>
#include <TupleHelper.h>
#include <GenericComparator.h>
#include <Utils/MonotonicPowerlawRandomStream.h>
#include <HavelHakimi/HavelHakimiIMGenerator.h>
#include <EdgeStream.h>

// CRC
#include "nmmintrin.h"
// BSWAP
#include "immintrin.h"

static inline uint64_t reverse (const uint64_t & a) {
    uint64_t x = a;
    /*
     * regular reverse
    x = (x & 0x5555555555555555u) <<  1 | (x & 0xAAAAAAAAAAAAAAAAu) >>  1;
    x = (x & 0x3333333333333333u) <<  2 | (x & 0xCCCCCCCCCCCCCCCCu) >>  2;
    x = (x & 0x0F0F0F0F0F0F0F0Fu) <<  4 | (x & 0xF0F0F0F0F0F0F0F0u) >>  4;
    x = __builtin_bswap64(x);//_bswap64(x);
    return x;
     */

    // x = (x & 0xF000000000000000u) >> 60 | (x & 0x0FFFFFFFFFFFFFFFu) << 4;
    return x;
}

/**
 * chained CRC64 with reverse
 */
static inline uint64_t crc64 (const uint32_t & seed, const uint32_t & msb, const uint32_t & lsb) {
    const uint32_t hash_msb_p = _mm_crc32_u32(seed, msb);
    const uint32_t hash_lsb_p = _mm_crc32_u32(hash_msb_p, lsb);
    const uint64_t hash = reverse(static_cast<uint64_t>(hash_msb_p) << 32 | hash_lsb_p);
   
    return hash;
}
/**
 * single CRC32 without reverse
 */
static inline uint32_t crc32(const uint32_t & seed, const uint32_t & val) {
    const uint32_t hash = _mm_crc32_u32(seed, val);

    return hash;
}

constexpr uint64_t NODEMASK = 0x0000000FFFFFFFFFu;
constexpr uint32_t MAX_LSB = 0x9BE09BABu;
constexpr uint32_t MIN_LSB = 0x00000000u;
constexpr uint32_t MAX_CRCFORWARD = 0x641F6454u;

using Edge64Comparator = typename GenericComparator<edge64_t>::Ascending;

/**
 * @typedef multinode_t
 * @brief The default signed integer to be used.
 * 
 * struct for node multiplicity
* in the 36 lsb bits - node
* in the 28 msb bits - key or half_edgeid
* we expect pairwise different representations
*/
class MultiNodeMsg {
public:
    MultiNodeMsg() { }
    MultiNodeMsg(const multinode_t eid_node_) : _eid_node(eid_node_) {}

    // getters
    uint32_t lsb() const {
        return static_cast<uint32_t>(_eid_node);
    }

    uint32_t msb() const {
        return static_cast<uint32_t>(_eid_node >> 32);
    }

    // just return the node
    multinode_t node() const {
        return _eid_node & NODEMASK;
    }

protected:
    multinode_t _eid_node;
};

// Comparator
class MultiNodeMsgComparator {  
public:
    MultiNodeMsgComparator() {}
    MultiNodeMsgComparator(const uint32_t seed_) 
        : _seed(seed_) 
        , _limits(_setLimits(seed_))
    {
        //std::cout << "WE IN COMP CONSTRUCTOR" << std::endl;
    }

    // invert msb's since lsb = seed then for max_value
    bool operator() (const MultiNodeMsg& a, const MultiNodeMsg& b) const {
        const uint64_t a_hash = crc64(_seed, a.msb(), a.lsb());
        const uint64_t b_hash = crc64(_seed, b.msb(), b.lsb());
        
        return a_hash < b_hash;
    }

    MultiNodeMsg max_value() const {
        return MultiNodeMsg(_limits.first);
    }

    MultiNodeMsg min_value() const {
        return MultiNodeMsg(_limits.second);
    }


protected:
    // unnecessary initialization, compiler asks for it
    const uint32_t _seed = 1;
    const std::pair<multinode_t, multinode_t> _limits;

    std::pair<multinode_t, multinode_t> _setLimits(const uint32_t seed_) const {
        multinode_t max_inv_msb = static_cast<multinode_t>(MAX_CRCFORWARD ^ seed_) << 32;
        multinode_t min_inv_msb = static_cast<multinode_t>(seed_) << 32;

        return std::pair<multinode_t, multinode_t>{max_inv_msb | MAX_LSB, min_inv_msb | MIN_LSB};
    }

};

template <typename EdgeReader>
class HavelHakimi_ConfigurationModel {
public:
    HavelHakimi_ConfigurationModel() = delete;
    HavelHakimi_ConfigurationModel(const HavelHakimi_ConfigurationModel&) = delete;
    HavelHakimi_ConfigurationModel(
            EdgeReader & edge_reader_in,
            const uint32_t seed,
            const uint64_t node_upperbound,
            const degree_t threshold,
            const degree_t max_degree,
            const node_t   nodes_above_threshold) 
        : _edges(edge_reader_in)
        , _seed(seed)
        , _node_upperbound(node_upperbound)
        , _shift_upperbound(std::min(node_upperbound, _maxShiftBound(node_upperbound)))
        , _threshold(threshold)
        , _max_degree(max_degree)
        , _nodes_above_threshold(nodes_above_threshold)
        , _high_degree_shift(_highDegreeShiftBound(node_upperbound, nodes_above_threshold))
        , _multinodemsg_comp(seed)
        , _multinodemsg_sorter(_multinodemsg_comp, SORTER_MEM)
        , _edge_sorter(Edge64Comparator(), SORTER_MEM)
        { }

    using value_type = edge64_t;

    void run() {
        assert(!_edges.empty());

        _generateMultiNodes();

        assert(!_multinodemsg_sorter.empty());

        _generateSortedEdgeList();

        assert(!_edge_sorter.empty());
    }

//! @name STXXL Streaming Interface
//! @{
    bool empty() const {
        return _edge_sorter.empty();
    }

    const value_type& operator*() const {
        //assert(!_edge_sorter.empty());

        return *_edge_sorter;
    }

    HavelHakimi_ConfigurationModel&operator++() {
        assert(!_edge_sorter.empty());
        
        ++_edge_sorter;

        return *this;
    }
//! @}

    void clear() {
        _multinodemsg_sorter.clear();
        _edge_sorter.clear();
    }

    uint64_t size(){
        return _edge_sorter.size();
    }

protected:
    EdgeReader _edges;

    const uint32_t _seed;
    const uint64_t _node_upperbound;
    const uint64_t _shift_upperbound;
    const degree_t _threshold;
    const degree_t _max_degree;
    const node_t   _nodes_above_threshold;
    const multinode_t _high_degree_shift;

    typedef stxxl::sorter<MultiNodeMsg, MultiNodeMsgComparator> MultiNodeSorter;
    MultiNodeMsgComparator _multinodemsg_comp;
    MultiNodeSorter _multinodemsg_sorter;

    using EdgeSorter = stxxl::sorter<value_type, Edge64Comparator>;
    EdgeSorter _edge_sorter; 

    void _generateMultiNodes() {
        assert(!_edges.empty());

        //stxxl::random_number<> rand;
        std::random_device rd;
        // random noise
        std::mt19937_64 gen64(rd());
        std::uniform_int_distribution<multinode_t> dis64;

        // shift multiplier for high degree nodes
        std::uniform_int_distribution<multinode_t> disShift(1, _high_degree_shift);

        // do first problematic nodes
        for (node_t count_threshold; count_threshold < _nodes_above_threshold; ++_edges, ++count_threshold) {

            const multinode_t random_noise = dis64(gen64);

            // first component of edge is SAFELY less than nodes_above_threshhold
            const multinode_t fst_node = _node_upperbound + disShift(gen64) * _nodes_above_threshold + static_cast<multinode_t>((*_edges).first);

            _multinodemsg_sorter.push(
                MultiNodeMsg{ (random_noise & (multinode_t) 0xFFFFFFF000000000) | fst_node});

            if ((*_edges).second < _nodes_above_threshold) {

                const multinode_t snd_node = _node_upperbound + disShift(gen64) * _nodes_above_threshold + static_cast<multinode_t>((*_edges).second);

                _multinodemsg_sorter.push(
                    MultiNodeMsg{ (random_noise << 36) | snd_node});

            } else {

                _multinodemsg_sorter.push(
                    MultiNodeMsg{ random_noise << 36 | static_cast<multinode_t>((*_edges).second)});

            }
        }

        // not so problematic
        for (; !_edges.empty(); ++_edges) {

            const multinode_t random_noise = dis64(gen64);

            _multinodemsg_sorter.push(
                MultiNodeMsg{ (random_noise & (multinode_t) 0xFFFFFFF000000000) | static_cast<multinode_t>((*_edges).first)});
            _multinodemsg_sorter.push(
                MultiNodeMsg{ (random_noise << 36) | static_cast<multinode_t>((*_edges).second)});

        }

        _multinodemsg_sorter.sort();

        assert(!_multinodemsg_sorter.empty());
    }

    /**
     * HavelHakimi gives us a graphical sequence, therefore no need to randomize an "half-edge" for the last node.
    **/
    void _generateSortedEdgeList() {
        assert(!_multinodemsg_sorter.empty());

        for(; !_multinodemsg_sorter.empty(); ++_multinodemsg_sorter) {
            auto & fst_node = *_multinodemsg_sorter;

            ++_multinodemsg_sorter;

            auto & snd_node = *_multinodemsg_sorter;

            if (fst_node.node() < snd_node.node())
                _edge_sorter.push(edge64_t{fst_node.node(), snd_node.node()});
            else
                _edge_sorter.push(edge64_t{snd_node.node(), fst_node.node()});
        }

        _edge_sorter.sort();
    }

    uint64_t _maxShiftBound(uint64_t n) const {
        return 27 - static_cast<uint64_t>(log2(n));
    }

    multinode_t _highDegreeShiftBound(uint64_t node_upperbound, node_t nodes_above_threshold) const {
        return (pow(2, 36) - node_upperbound) / nodes_above_threshold - 1;
    }
};

// Pseudo-random approach

struct TestNodeMsg {
    multinode_t key;
    multinode_t node;

    TestNodeMsg() { }
    TestNodeMsg(const multinode_t &key_, const multinode_t &node_) : key(key_), node(node_) {}

    DECL_LEX_COMPARE_OS(TestNodeMsg, key, node);
};

// TestNode Comparator
class TestNodeRandomComparator {  
public:
    TestNodeRandomComparator() { }

    // invert msb's since lsb = seed then for max_value
    bool operator() (const TestNodeMsg& a, const TestNodeMsg& b) const {
        if (a.key != b.key)
            return a.key < b.key;
        else {
            if (b.node == std::numeric_limits<multinode_t>::min())
                return false;
            if (a.node == std::numeric_limits<multinode_t>::max())
                return false;
            if (b.node == std::numeric_limits<multinode_t>::max())
                return true;
            if (a.node == std::numeric_limits<multinode_t>::min())
                return true;
            std::default_random_engine gen;
            std::bernoulli_distribution ber(0.5);
            if (ber(gen)) 
                return true;
            else 
                return false;
        }
    }

    TestNodeMsg max_value() const {
        return TestNodeMsg(std::numeric_limits<multinode_t>::max(), std::numeric_limits<multinode_t>::max());
    }

    TestNodeMsg min_value() const {
        return TestNodeMsg(std::numeric_limits<multinode_t>::min(), std::numeric_limits<multinode_t>::min());
    }
};

using TestNodeComparator = typename GenericComparatorStruct<TestNodeMsg>::Ascending;

template <typename EdgeReader, typename MNComparator>
class HavelHakimi_ConfigurationModel_Random {
public:
    HavelHakimi_ConfigurationModel_Random() = delete; 

    HavelHakimi_ConfigurationModel_Random(const HavelHakimi_ConfigurationModel_Random&) = delete;
    HavelHakimi_ConfigurationModel_Random(EdgeReader &edges, const uint64_t node_upperbound)
                                : _edges(edges)
                                , _node_upperbound(node_upperbound)
                                , _testnode_sorter(MNComparator(), SORTER_MEM)
                                , _test_edge_sorter(Edge64Comparator(), SORTER_MEM)
    { }

    using value_type = edge64_t;

    // implements execution of algorithm
    void run() {
        assert(!_edges.empty());

        _generateMultiNodes();

        assert(!_testnode_sorter.empty());

        _generateSortedEdgeList();

        assert(!_test_edge_sorter.empty());
    }

//! @name STXXL Streaming Interface
//! @{
    bool empty() const {
        return _test_edge_sorter.empty();
    }

    const edge64_t& operator*() const {
        assert(!_test_edge_sorter.empty());

        return *_test_edge_sorter;
    }

    HavelHakimi_ConfigurationModel_Random&operator++() {
        assert(!_test_edge_sorter.empty());

        ++_test_edge_sorter;
        
        return *this;
    }
//! @}
    
    // for testing
    void clear() {
        _testnode_sorter.clear();
        _test_edge_sorter.clear();
    }

    uint64_t size() {
        return _test_edge_sorter.size();
    }

protected:
    EdgeReader _edges;
    const uint64_t _node_upperbound;

    using TestNodeSorter = stxxl::sorter<TestNodeMsg, MNComparator>;
    TestNodeSorter _testnode_sorter;

    using EdgeSorter = stxxl::sorter<value_type, Edge64Comparator>;
    EdgeSorter _test_edge_sorter;
    // internal algos
    void _generateMultiNodes() {
        assert(!_edges.empty());

        //stxxl::random_number64 rand64;
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<multinode_t> dis;

        for (; !_edges.empty(); ++_edges) {
            _testnode_sorter.push(TestNodeMsg{dis(gen), static_cast<multinode_t>((*_edges).first)});
            _testnode_sorter.push(TestNodeMsg{dis(gen), static_cast<multinode_t>((*_edges).second)});

        }
       
        _testnode_sorter.sort();

        assert(!_testnode_sorter.empty());
    }
    
    // HavelHakimi gives us a graphical degree sequence, no need for randomization for last node
    void _generateSortedEdgeList() {
        assert(!_testnode_sorter.empty());

        for(; !_testnode_sorter.empty(); ++_testnode_sorter) {
            auto & fst_node = *_testnode_sorter;

            ++_testnode_sorter;

            auto & snd_node = *_testnode_sorter;

            if (fst_node.node < snd_node.node)
                _test_edge_sorter.push(edge64_t{fst_node.node, snd_node.node});
            else
                _test_edge_sorter.push(edge64_t{snd_node.node, fst_node.node});
        }

        _test_edge_sorter.sort();
    }
};
