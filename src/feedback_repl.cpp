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

#include <limits>
#include <algorithm>
#include <iomanip>
#include <numeric>
#include "feedback_repl.h"

// #define info(...) do { printf(__VA_ARGS__); } while(0)

// FeedbackBaseReplPolicy::FeedbackBaseReplPolicy(
//     std::string _agingType,
//     uint32_t _numLines,
//     uint32_t _associativity,
//     uint64_t _accsPerInterval,
//     uint64_t _ageScaling,
//     uint64_t _maxAge,
//     double _ewmaDecay,
//     bool _fullDebugInfo)
//         : aging(0)
//         , numLines(_numLines)
//         , associativity(_associativity)
//         , nextUpdate(_accsPerInterval)
//         , accsPerInterval(_accsPerInterval)
//         , ageScaling(_ageScaling)
//         , maxAge(_maxAge)
//         , ewmaDecay(_ewmaDecay)
//         , fullDebugInfo(_fullDebugInfo) {
//     // info("FeedbackBaseReplPolicy: numLines %u, accsPerInterval %lu, ageScaling %lu, maxAge %lu",
//         //  _numLines, _accsPerInterval, _ageScaling, _maxAge);

//     aging = Aging::create(this, _agingType);

//     timestamps.resize(numLines, 0);
//     classIds.resize(numLines, 0);

//     wrapArounds = 0;
//     fills = 0;
// }

FeedbackBaseReplPolicy::~FeedbackBaseReplPolicy() {
    for (auto* cl : classes) {
        delete cl;
    }
    classes.clear();
}

// ** Steady-state replacement policy operation

bool FeedbackBaseReplPolicy::update2(uint32_t id) {
    bool isPresent = present(id);
    if (isPresent) {
        // hit!
        cl(id)->hits[aging->age(id)]++;
        // if (id / associativity == 0) { info("Hit on %u at age %lu", id, aging->age(id)); }
    }

    aging->update(id);

    if (--nextUpdate == 0) {
        reconfigure();
        reset();
    }

    return isPresent;
}

bool FeedbackBaseReplPolicy::replaced2(uint32_t id) {
    bool isPresent = present(id);
    if (isPresent) {
        cl(id)->evictions[aging->age(id)]++;
        // if (id / associativity == 0) { info("Eviction on %u at age %lu", id, aging->age(id)); }
    } else {
        fills++;
        // if (id / associativity == 0) { info("Filling %u", id); }
    }

    timestamps[id] = 0;

    return isPresent;
}

// template<class C> inline uint32_t FeedbackBaseReplPolicy::rank(const MemReq* req, C candsIn) {
//     uint32_t bestCand = -1;
//     const bool ENABLE_BYPASS = false;
//     double bestRank = ENABLE_BYPASS? classes.front()->ranks[0] : std::numeric_limits<double>::max();

//     // buffer candidates so we can start from a random place...
//     uint32_t cands[ candsIn.numCands() ];
//     uint32_t i = 0;
//     for (auto ci = candsIn.begin(); ci != candsIn.end(); ci.inc()) {
//         cands[i++] = *ci;
//     }

//     uint32_t start = 0; // rand.randInt(candsIn.size());

//     for (i = 0; i < candsIn.numCands(); i++) {
//         uint32_t id = cands[ (i + start) % candsIn.numCands() ];

//         if (!present(id)) {
//             // line is invalid
//             bestCand = id;
//             break;
//         } else {
//             uint64_t a = aging->age(id);
//             double rank = cl(id)->ranks[a];
//             if (rank < bestRank - 1e-3) {
//                 bestCand = id;
//                 bestRank = rank;
//             }
//             // if (id / associativity == 0) { info("Candidate %u of age %lu has rank %g", id, a, rank); }
//         }
//     }

//     assert_msg(bestCand != (uint32_t)-1, "TODO: There is a problem with bypassing. The current code path does not update the replacement policy on a bypass, meaning that 'now' will not correctly count accesses. -nzb");
//     return bestCand;
// }

// ** Periodic reconfigurations

void FeedbackBaseReplPolicy::reconfigure() {
    uint64_t intervalHits = 0;
    uint64_t intervalEvictions = 0;
    uint64_t ewmaHits = 0;
    uint64_t ewmaEvictions = 0;
    uint64_t cumHits = 0;
    uint64_t cumEvictions = 0;

    for (auto* cl : classes) {
        cl->update();

        intervalHits += std::accumulate(cl->hits.begin(), cl->hits.end(), 0);
        intervalEvictions += std::accumulate(cl->evictions.begin(), cl->evictions.end(), 0);

        ewmaHits += std::accumulate(cl->ewmaHits.begin(), cl->ewmaHits.end(), 0);
        ewmaEvictions += std::accumulate(cl->ewmaEvictions.begin(), cl->ewmaEvictions.end(), 0);

        cumHits += cl->cumulativeHits;
        cumEvictions += cl->cumulativeEvictions;
    }

    double lineGain = 1. * ewmaHits / (ewmaHits + ewmaEvictions) / aging->numLines();

    for (auto* cl : classes) {
        cl->reconfigure(lineGain);

        // it can never be allowed for the saturating age to have highest
        // rank, or the cache can get stuck with all lines saturated
        cl->ranks[maxAge-1] = std::numeric_limits<double>::lowest();
    }

    uint64_t newAgeScaling = ageScaling;
    newAgeScaling = aging->adaptAgeScale();

    // info("FeedbackBaseReplPolicy        | interval hitRate %lu / %lu = %g, fills %lu, wrapArounds %lu / %lu (%g) | cumulativeHitRate %g | ageScaling %lu",
    //      intervalHits, intervalHits + intervalEvictions,
    //      1. * intervalHits / (intervalHits + intervalEvictions),
    //      fills, wrapArounds, accsPerInterval,
    //      1. * wrapArounds / accsPerInterval,
    //      1. * cumHits / (cumHits + cumEvictions),
    //      newAgeScaling);
    // with compressed arrays, fills aren't recorded and one access can trigger multiple evictions
    // uint64_t accesses = fills + intervalHits + intervalEvictions;
    // assert(accesses == accsPerInterval);

    ageScaling = newAgeScaling;
}

void FeedbackBaseReplPolicy::reset() {
    for (auto* cl : classes) {
        cl->reset();
    }

    nextUpdate += accsPerInterval;
    wrapArounds = 0;
    fills = 0;
}

// ** Other infrastructure

FeedbackBaseReplPolicy::Class::Class(FeedbackBaseReplPolicy* _owner)
        : owner(_owner) {
    ranks.resize(owner->maxAge, 0.);

    hits.resize(owner->maxAge, 0);
    evictions.resize(owner->maxAge, 0);

    ewmaHits.resize(owner->maxAge, 0.);
    ewmaEvictions.resize(owner->maxAge, 0.);

    cumulativeHits = 0;
    cumulativeEvictions = 0;
    hitProbability.resize(owner->maxAge, 0.);
    expectedLifetime.resize(owner->maxAge, 0.);
    opportunityCost.resize(owner->maxAge, 0.);
}

FeedbackBaseReplPolicy::Class::~Class() {
}

FeedbackBaseReplPolicy::Class* FeedbackBaseReplPolicy::Class::create(FeedbackBaseReplPolicy* _owner, std::string _type) {
    if (_type == "Bias") {
        return new BiasClass(_owner);
    } else {
        panic("Unknown class type: %s", _type.c_str());
    }
}

void FeedbackBaseReplPolicy::Class::update() {
    // average in monitored stats
    for (uint32_t a = 0; a < owner->maxAge; a++) {
        ewmaHits[a] *= owner->ewmaDecay;
        ewmaHits[a] += hits[a];

        ewmaEvictions[a] *= owner->ewmaDecay;
        ewmaEvictions[a] += evictions[a];
    }
}
void FeedbackBaseReplPolicy::Class::reset() {
    dumpStats();

    // reset monitor
    std::fill(hits.begin(), hits.end(), 0);
    std::fill(evictions.begin(), evictions.end(), 0);
}

void FeedbackBaseReplPolicy::Class::dumpStats() {
    // if (false) { return; }
    
    uint64_t totalHits = std::accumulate(hits.begin(), hits.end(), 0);
    uint64_t totalEvictions = std::accumulate(evictions.begin(), evictions.end(), 0);
    // double intervalHitRate = 1. * totalHits / (totalHits + totalEvictions);

    double totalEwmaHits = std::accumulate(ewmaHits.begin(), ewmaHits.end(), 0);
    double totalEwmaEvictions = std::accumulate(ewmaEvictions.begin(), ewmaEvictions.end(), 0);
    // double ewmaHitRate = 1. * totalEwmaHits / (totalEwmaHits + totalEwmaEvictions);

    cumulativeHits += totalHits;
    cumulativeEvictions += totalEvictions;
    // double cumulativeHitRate = 1. * cumulativeHits / (cumulativeHits + cumulativeEvictions);

    if (owner->fullDebugInfo) {
        uint64_t left = totalEwmaHits + totalEwmaEvictions;
        std::cout << std::setw(12) << "Age" << ": "
                  << std::setw(12) << "Hits"
                  << std::setw(12) << "Evictions" << " | "
                  << std::setw(12) << "Ranks" << " = "
                  << std::setw(12) << "hitProb" << " - "
                  << std::setw(12) << "oppCost" << " <-- ("
                  << std::setw(12) << "lineGain" << " * "
                  << std::setw(12) << "expLife" << ")"
                  << std::endl;
        for (uint32_t a = 0; a < owner->maxAge; a++) {
            std::cout << std::setw(12) << a << ": "
                      << std::setw(12) << ewmaHits[a]
                      << std::setw(12) << ewmaEvictions[a] << " | "
                      << std::setw(12) << ranks[a] << " = "
                      << std::setw(12) << hitProbability[a] << " - "
                      << std::setw(12) << opportunityCost[a] << " <-- ("
                      << std::setw(12) << (opportunityCost[a] / expectedLifetime[a]) << " * " // ewmaHitRate / owner->aging->numLines()) << " * "
                      << std::setw(12) << expectedLifetime[a] << ")"
                      << std::endl;

            left -= ewmaHits[a] + ewmaEvictions[a];
            if (left == 0) {
                break;
            }
        }
    }

    // info("FeedbackBaseReplPolicy::Class | interval hitRate %g | ewma hitRate %g, ewmaMass %g | cumulative hitRate %g",
        //  intervalHitRate, ewmaHitRate, totalEwmaHits + totalEwmaEvictions, cumulativeHitRate);
}

FeedbackBaseReplPolicy::Aging* FeedbackBaseReplPolicy::Aging::create(
    FeedbackBaseReplPolicy* repl,
    std::string type) {
    
    if (type == "Global") {
        return new GlobalCoarsenedAging(repl);
    } else {
        panic("FeedbackBaseReplPolicy: Unrecognized aging type '%s'", type.c_str());
    }
}

FeedbackBaseReplPolicy::GlobalCoarsenedAging::GlobalCoarsenedAging(
    FeedbackBaseReplPolicy* _repl)
        : Aging(_repl)
        , now(0) {
}

void FeedbackBaseReplPolicy::GlobalCoarsenedAging::update(uint32_t id) {
    now++;

    // check for wraparounds
    uint64_t exact = now - repl->timestamps[id];
    uint64_t coarse = exact / repl->ageScaling;
    if (coarse > repl->maxAge) {
        ++(repl->wrapArounds);
    }

    repl->timestamps[id] = now;
}

uint64_t FeedbackBaseReplPolicy::GlobalCoarsenedAging::age(uint32_t id) {
    uint64_t exact = now - repl->timestamps[id];
    uint64_t coarse = exact / repl->ageScaling;
    uint64_t mod = coarse % repl->maxAge;
    return mod;
}

uint32_t FeedbackBaseReplPolicy::GlobalCoarsenedAging::numLines() {
    return repl->numLines;
}

double FeedbackBaseReplPolicy::GlobalCoarsenedAging::ageScaling(uint64_t a) {
    if (a == 0) {
        return repl->ageScaling / 2. + 0.5;
    } else {
        return repl->ageScaling;
    }
}

uint64_t FeedbackBaseReplPolicy::GlobalCoarsenedAging::adaptAgeScale() {
    // todo: something with wrapArounds...
    return repl->ageScaling;
}

// ** EVA.

void FeedbackBaseReplPolicy::BiasClass::reconfigure(double lineGain) {
    // count events first.
    // FIXME: use arrays on the stack for profiling!!! (doesn't work
    // for idealized implementation)
    std::vector<double> events( owner->maxAge );
    std::vector<double> totalEventsAbove( owner->maxAge + 1 );
    totalEventsAbove[ owner->maxAge ] = 0.;
    
    for (uint32_t a = owner->maxAge - 1; a < owner->maxAge; a--) {
        events[a] = ewmaHits[a] + ewmaEvictions[a];
        totalEventsAbove[a] = totalEventsAbove[a+1] + events[a];
    }

    uint32_t a = owner->maxAge - 1;
    hitProbability[a] = (totalEventsAbove[a] > 1e-2)? 0.5 * ewmaHits[a] / totalEventsAbove[a] : 0.;
    expectedLifetime[a] = owner->aging->ageScaling(a);;
    double expectedLifetimeUnconditioned = owner->aging->ageScaling(a) * totalEventsAbove[a];
    double totalHitsAbove = ewmaHits[a];

    // short lines
    //
    // computed assuming events are uniformly distributed within each
    // coarsened region.
    for (uint32_t a = owner->maxAge - 2; a < owner->maxAge; a--) {
        if (totalEventsAbove[a] > 1e-2) {
            hitProbability[a] = (0.5 * ewmaHits[a] + totalHitsAbove) / (0.5 * events[a] + totalEventsAbove[a+1]);
            expectedLifetime[a] = ((1./6) * owner->aging->ageScaling(a) * events[a] + expectedLifetimeUnconditioned) / (0.5 * events[a] + totalEventsAbove[a+1]);
            // info("ageScaling at %u is a %g", a, owner->aging->ageScaling(a));
        } else {
            hitProbability[a] = 0.;
            expectedLifetime[a] = 0.;
        }
        
        totalHitsAbove += ewmaHits[a];
        expectedLifetimeUnconditioned += owner->aging->ageScaling(a) * totalEventsAbove[a];
    }

    // finally, compute EVA from the probabilities and lifetimes
    for (uint32_t a = owner->maxAge - 1; a < owner->maxAge; a--) {
        if (unlikely(std::isnan(lineGain))) {
            opportunityCost[a] = 0.;
        } else {
            opportunityCost[a] = lineGain * expectedLifetime[a];
        }
        ranks[a] = hitProbability[a] - opportunityCost[a];
    }    
}

// ** Different degrees of classification.

FeedbackReplPolicy::FeedbackReplPolicy(
    std::string _agingType,
    uint32_t _numLines,
    uint32_t _associativity,
    uint64_t _accsPerInterval,
    uint64_t _ageScaling,
    uint64_t _maxAge,
    double _ewmaDecay,
    bool _fullDebugInfo,
    std::string _type)
        : FeedbackBaseReplPolicy(
            _agingType,
            _numLines,
            _associativity,
            _accsPerInterval,
            _ageScaling,
            _maxAge,
            _ewmaDecay,
            _fullDebugInfo) {
    classes.push_back(Class::create(this, _type));
}

FeedbackReplPolicy::~FeedbackReplPolicy() {
}

FeedbackReusedReplPolicy::FeedbackReusedReplPolicy(
    std::string _agingType,
    uint32_t _numLines,
    uint32_t _associativity,
    uint64_t _accsPerInterval,
    uint64_t _ageScaling,
    uint64_t _maxAge,
    double _ewmaDecay,
    bool _fullDebugInfo,
    std::string _type)
        : FeedbackBaseReplPolicy(
            _agingType,
            _numLines,
            _associativity,
            _accsPerInterval,
            _ageScaling,
            _maxAge,
            _ewmaDecay,
            _fullDebugInfo) {
    std::fill(classIds.begin(), classIds.end(), NONREUSED);

    classes.resize(NUM_CLASSES);
    classes[NONREUSED] = Class::create(this, _type);
    classes[REUSED]    = Class::create(this, _type);
}

FeedbackReusedReplPolicy::~FeedbackReusedReplPolicy() {
}

void FeedbackReusedReplPolicy::update(uint32_t id) {
    bool wasPresent = update2(id);
    classIds[id] = wasPresent? REUSED : NONREUSED;
}

void FeedbackReusedReplPolicy::replaced(uint32_t id) {
    replaced2(id);
    classIds[id] = NONREUSED;
}

void FeedbackReusedReplPolicy::reconfigure() {
    FeedbackBaseReplPolicy::reconfigure();

    // ranks should reflect the probability that a reused line hits,
    // and becomes reused for its next lifetime, and so on, to
    // infinity.
    //
    // sum evictions from 1 to avoid silly nan cases.
    Class* rc = classes[REUSED];
    Class* nc = classes[NONREUSED];

    if (dynamic_cast<BiasClass*>(rc) == nullptr) {
        // only enabled if using Bias to rank lines
        return;
    }

    uint64_t reusedHits = std::accumulate(rc->ewmaHits.begin(), rc->ewmaHits.end(), 1);
    uint64_t reusedEvictions = std::accumulate(rc->ewmaEvictions.begin(), rc->ewmaEvictions.end(), 1);
    double reusedMissRate = 1. * reusedEvictions / (reusedHits + reusedEvictions);

    uint64_t nonReusedHits = std::accumulate(nc->ewmaHits.begin(), nc->ewmaHits.end(), 1);
    uint64_t nonReusedEvictions = std::accumulate(nc->ewmaEvictions.begin(), nc->ewmaEvictions.end(), 1);
    // double nonReusedMissRate = 1. * nonReusedEvictions / (nonReusedHits + nonReusedEvictions);

    uint64_t totalHits = reusedHits + nonReusedHits;
    uint64_t totalEvictions = reusedEvictions + nonReusedEvictions;
    double averageMissRate = 1. * totalEvictions / (totalHits + totalEvictions);

    double reusedLifetimeBias = rc->ranks[0];

    for (auto* cl : classes) {
        for (uint32_t a = maxAge - 1; a < maxAge; a--) {
            cl->ranks[a] += (averageMissRate - (1 - cl->hitProbability[a])) / reusedMissRate * reusedLifetimeBias;
        }
    }

    // info("FeedbackReusedReplPolicy      | reusedHitRate %g, nonReusedHitRate %g, averageHitRate %g",
        //  1. - reusedMissRate, 1. - nonReusedMissRate, 1. - averageMissRate);
}
