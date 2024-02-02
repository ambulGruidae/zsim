/** $lic$
 * Copyright (C) 2012-2013 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2012 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * This is an internal version, and is not under GPL. All rights reserved.
 * Only MIT and Stanford students and faculty are allowed to use this version.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2010) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

#pragma once

#include <string>
#include "repl_policies.h"
#include "mtrand.h"

class FeedbackBaseReplPolicy : public ReplPolicy {
    public:
        explicit FeedbackBaseReplPolicy(
            std::string _agingType,     // set to 'Global'
            uint32_t _numLines,
            uint32_t _associativity,
            uint64_t _accsPerInterval,
            uint64_t _ageScaling,
            uint64_t _maxAge,
            double _ewmaDecay,
            bool _fullDebugInfo)
                : aging(0)
                , numLines(_numLines)
                , associativity(_associativity)
                , nextUpdate(_accsPerInterval)
                , accsPerInterval(_accsPerInterval)
                , ageScaling(_ageScaling)
                , maxAge(_maxAge)
                , ewmaDecay(_ewmaDecay)
                , fullDebugInfo(_fullDebugInfo) {
            aging = Aging::create(this, _agingType);

            timestamps.resize(numLines, 0);
            classIds.resize(numLines, 0);

            wrapArounds = 0;
            fills = 0;
        };

        ~FeedbackBaseReplPolicy();

        // a line was referenced!
        virtual void update(uint32_t id, const MemReq* req) { // id is in [0, numLines]
            update2(id);
        }

        // a line was evicted!
        virtual void replaced(uint32_t id) { // id is in [0, numLines]
            replaced2(id);
        }

        // compare candidates and select a victim. returns a line id in [0, numLines].
        template<class C> inline uint32_t rank(const MemReq* req, C candsIn) { // a container class, e.g. STL vector
            uint32_t bestCand = -1;
            const bool ENABLE_BYPASS = false;
            double bestRank = ENABLE_BYPASS? classes.front()->ranks[0] : std::numeric_limits<double>::max();

            // buffer candidates so we can start from a random place...
            uint32_t cands[ candsIn.numCands() ];
            uint32_t i = 0;
            for (auto ci = candsIn.begin(); ci != candsIn.end(); ci.inc()) {
                cands[i++] = *ci;
            }

            uint32_t start = 0; // rand.randInt(candsIn.size());

            for (i = 0; i < candsIn.numCands(); i++) {
                uint32_t id = cands[ (i + start) % candsIn.numCands() ];

                if (!present(id)) {
                    // line is invalid
                    bestCand = id;
                    break;
                } else {
                    uint64_t a = aging->age(id);
                    double rank = cl(id)->ranks[a];
                    if (rank < bestRank - 1e-3) {
                        bestCand = id;
                        bestRank = rank;
                    }
                    // if (id / associativity == 0) { info("Candidate %u of age %lu has rank %g", id, a, rank); }
                }
            }

            assert_msg(bestCand != (uint32_t)-1, "TODO: There is a problem with bypassing. The current code path does not update the replacement policy on a bypass, meaning that 'now' will not correctly count accesses. -nzb");
            return bestCand;
        }
        DECL_RANK_BINDINGS; 

    protected:

        // a single class of lines in the cache, eg lines that have
        // been reused at least once
        struct Class {
                FeedbackBaseReplPolicy* owner;

                // ranks, higher is better; could be stored as eviction order instead
                g_vector<double> ranks;

                // monitor (hardware counters)
                g_vector<uint64_t> hits;
                g_vector<uint64_t> evictions;

                // averaged stats for computing ranks (software counters)
                g_vector<double> ewmaHits;
                g_vector<double> ewmaEvictions;

                // debugging
                uint64_t cumulativeHits;
                uint64_t cumulativeEvictions;
                // the following three vectors are for debug output *only* and
                // could easily be optimized out in reconfigure() -nzb
                g_vector<double> hitProbability;
                g_vector<double> expectedLifetime;
                g_vector<double> opportunityCost;

                Class(FeedbackBaseReplPolicy*);
                virtual ~Class();

                static Class* create(FeedbackBaseReplPolicy* _owner, std::string _type);

                void update();
                virtual void reconfigure(double lineGain) = 0;
                void reset();
                void dumpStats();
        };

        // EVA implementation
        struct BiasClass : public Class {
                BiasClass(FeedbackBaseReplPolicy* _owner) : Class(_owner) {}
                void reconfigure(double lineGain);
        };

        // cache state
        //
        // This class gives different ways of implementing aging bits
        // to lower overheads. Not particularly interesting, only
        // releasing the 'global' implementation that can be
        // configured to support very large ages with a small
        // ageScaling and large maxAge.
        class Aging : public GlobAlloc {
            public:
                Aging(FeedbackBaseReplPolicy* _repl) : repl(_repl) {}
                virtual ~Aging() {}

                virtual void update(uint32_t id) = 0;
                virtual uint64_t age(uint32_t id) = 0;
                virtual uint32_t numLines() = 0;
                virtual double ageScaling(uint64_t coarseAge) = 0;
                virtual uint64_t adaptAgeScale() = 0;

                static Aging* create(FeedbackBaseReplPolicy* repl, std::string type);

            protected:
                FeedbackBaseReplPolicy* repl;
        };

        class GlobalCoarsenedAging : public Aging {
            public:
                GlobalCoarsenedAging(FeedbackBaseReplPolicy*);

                virtual void update(uint32_t id);
                virtual uint64_t age(uint32_t id);
                virtual uint32_t numLines();
                virtual double ageScaling(uint64_t coarseAge);
                virtual uint64_t adaptAgeScale();

            private:
                uint64_t now;
        };

        Aging* aging;
        MTRand rand;
        g_vector<uint64_t> timestamps;
        g_vector<uint32_t> classIds;
        g_vector<Class*> classes;
        uint64_t numLines, associativity;
        uint64_t nextUpdate;
        uint64_t accsPerInterval;
        uint64_t ageScaling;
        uint64_t maxAge;
        double ewmaDecay;
        bool fullDebugInfo;

        uint64_t wrapArounds;           // how many lines exceeded maxAge and had to be modded?
        uint64_t fills;                 // cache fills, only during startup

        inline bool present(uint32_t id) {
            return timestamps[id] != 0;
        }
        inline Class* cl(uint32_t id) {
            uint32_t classId = classIds[id];
            assert(classId < classes.size());
            return classes[classId];
        }

        virtual void reconfigure();
        virtual void reset();
        bool update2(
            uint32_t id);
        bool replaced2(
            uint32_t id);
};

// Do not classify lines.
class FeedbackReplPolicy : public FeedbackBaseReplPolicy {
    public:
        FeedbackReplPolicy(
            std::string _agingType,     // set to 'Global'
            uint32_t _numLines,
            uint32_t _associativity,
            uint64_t _accsPerInterval,
            uint64_t _ageScaling,
            uint64_t _maxAge,
            double _ewmaDecay,
            bool _fullDebugInfo,
            std::string _type);         // set to 'Bias'

        ~FeedbackReplPolicy();

        DECL_RANK_BINDINGS; 
};

// Classify between reused and non-reused lines.
class FeedbackReusedReplPolicy : public FeedbackBaseReplPolicy {
    public:
        explicit FeedbackReusedReplPolicy(
            std::string _agingType,     // set to 'Global'
            uint32_t _numLines,
            uint32_t _associativity,
            uint64_t _accsPerInterval,
            uint64_t _ageScaling,
            uint64_t _maxAge,
            double _ewmaDecay,
            bool _fullDebugInfo,
            std::string _type);         // set to 'Bias'
        ~FeedbackReusedReplPolicy();

        void update(
            uint32_t id);

        void replaced(
            uint32_t id);

        void reconfigure();

        DECL_RANK_BINDINGS;

        enum CLASSES {
            NONREUSED = 0,
            REUSED = 1,
            NUM_CLASSES
        };
};
