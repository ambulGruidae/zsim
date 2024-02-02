#ifndef SPARSE_CACHE_H_
#define SPARSE_CACHE_H_

#include "timing_cache.h"
#include "stats.h"

class HitWritebackEvent;

class SparseCache : public TimingCache {
    protected:
        uint32_t numTagLines;
        uint32_t numDataLines;
        // uint32_t tagLat;

        SparseTagArray* tagArray;
        SparseDataArray* dataArray;

        ReplPolicy* tagRP;
        ReplPolicy* dataRP;

        RunningStats* crStats;
        RunningStats* evStats;
        RunningStats* tutStats;
        RunningStats* dutStats;
    
    public:
        SparseCache(uint32_t _numTagLines, uint32_t _numDataLines, CC* _cc, SparseTagArray* _tagArray, SparseDataArray* _dataArray,
                    ReplPolicy* tagRP, ReplPolicy* dataRP, uint32_t _accLat, uint32_t _invLat, uint32_t mshrs, uint32_t ways, uint32_t cands, uint32_t _domain,
                    const g_string& _name, RunningStats* _crStats, RunningStats* _evStats, RunningStats* _tuStats, RunningStats* _duStats, Counter* _tag_hits, Counter* _tag_misses, Counter* _tag_all);

        uint64_t access(MemReq& req);

        void initStats(AggregateStat* parentStat);
        void simulateHitWriteback(HitWritebackEvent* ev, uint64_t cycle, HitEvent* he);
    
    protected:
        void initCacheStats(AggregateStat* cacheStat);
};

class HitWritebackEvent : public TimingEvent {
    private:
        SparseCache* cache;
        HitEvent* he;

    public:
        HitWritebackEvent(SparseCache* _cache, HitEvent* _he, uint32_t postDelay, int32_t domain) : TimingEvent(0, postDelay, domain), cache(_cache), he(_he) {}
        void simulate(uint64_t startCycle) { cache->simulateHitWriteback(this, startCycle, he); }
};

#endif // SPARSE_CACHE_H_
