/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <limits>

#include "cache_arrays.h"
#include "hash.h"
#include "repl_policies.h"
#include "zsim.h"

#include "pin.H"

/* Set-associative array implementation */

SetAssocArray::SetAssocArray(uint32_t _numLines, uint32_t _assoc, ReplPolicy* _rp, HashFamily* _hf) : rp(_rp), hf(_hf), numLines(_numLines), assoc(_assoc)  {
    array = gm_calloc<Address>(numLines);
    numSets = numLines/assoc;
    setMask = numSets - 1;
    info("Set Assoc Array: %i lines and %i sets", numLines, numSets);
    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
}

int32_t SetAssocArray::lookup(const Address lineAddr, const MemReq* req, bool updateReplacement) {
    uint32_t set = hf->hash(0, lineAddr) & setMask;
    uint32_t first = set*assoc;
    for (uint32_t id = first; id < first + assoc; id++) {
        if (array[id] ==  lineAddr) {
            if (updateReplacement) rp->update(id, req);
            return id;
        }
    }
    return -1;
}

uint32_t SetAssocArray::preinsert(const Address lineAddr, const MemReq* req, Address* wbLineAddr) { //TODO: Give out valid bit of wb cand?
    uint32_t set = hf->hash(0, lineAddr) & setMask;
    uint32_t first = set*assoc;

    uint32_t candidate = rp->rankCands(req, SetAssocCands(first, first+assoc));

    *wbLineAddr = array[candidate];
    return candidate;
}

void SetAssocArray::postinsert(const Address lineAddr, const MemReq* req, uint32_t candidate) {
    rp->replaced(candidate);
    array[candidate] = lineAddr;
    rp->update(candidate, req);
}

SparseTagArray::SparseTagArray(uint32_t _numLines, uint32_t _assoc, ReplPolicy* _rp, HashFamily* _hf) : rp(_rp), hf(_hf), numLines(_numLines), assoc(_assoc) {
    tagArray = gm_calloc<Address>(numLines);
    prePtrArray = gm_calloc<int32_t>(numLines);
    nextPtrArray = gm_calloc<int32_t>(numLines);
    mapPtrArray = gm_calloc<int32_t>(numLines);
    approximateArray = gm_calloc<bool>(numLines);
    for (uint32_t i = 0; i < numLines; i++) {
        prePtrArray[i] = -1;
        nextPtrArray[i] = -1;
        mapPtrArray[i] = -1;
        approximateArray[i] = false;
    }
    numSets = numLines/assoc;
    setMask = numSets - 1;
    validLines = 0;
    info("Sparse Tag Array: %i lines and %i sets", numLines, numSets);
    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
}

SparseTagArray::~SparseTagArray() {
    gm_free(tagArray);
    gm_free(prePtrArray);
    gm_free(nextPtrArray);
    gm_free(mapPtrArray);
    gm_free(approximateArray);
}

int32_t SparseTagArray::lookup(const Address lineAddr, const MemReq* req, bool updateReplacement) {
    uint32_t set = hf->hash(0, lineAddr) & setMask;
    uint32_t first = set*assoc;
    for (uint32_t id = first; id < first + assoc; id++) {
        if (tagArray[id] == lineAddr) {
            if (updateReplacement) rp->update(id, req);
            return id;
        }
    }
    return -1;
}

uint32_t SparseTagArray::preinsert(const Address lineAddr, const MemReq* req, Address* wbLineAddr) {
    uint32_t set = hf->hash(0, lineAddr) & setMask;
    uint32_t first = set*assoc;

    uint32_t candidate = rp->rankCands(req, SetAssocCands(first, first+assoc));

    *wbLineAddr = tagArray[candidate];
    return candidate;
}

bool SparseTagArray::evictAssociatedData(int32_t lineId, int32_t* newLLHead, bool* approximate) {
    *newLLHead = -1;
    if (mapPtrArray[lineId] == -1)
        return false; //no associated data
    if (!approximateArray[lineId])
        return true; //no need to evict associated data
    *approximate = true;
    if (prePtrArray[lineId] != -1)
        return false; //not the head of the list
    else
        *newLLHead = nextPtrArray[lineId];
    if (nextPtrArray[lineId] != -1)
        return false; //not the tail of the list
    return true;
}

void SparseTagArray::postinsert(Address lineAddr, const MemReq* req, int32_t tagId, int32_t mapId, int32_t listHead, bool approximate, bool updateReplacement) {
    if (!tagArray[tagId] && lineAddr) {
        validLines++;
    } else if (tagArray[tagId] && !lineAddr) {
        assert(validLines);
        validLines--;
    }
    rp->replaced(tagId);
    tagArray[tagId] = lineAddr;
    mapPtrArray[tagId] = mapId;
    approximateArray[tagId] = approximate;
    if (prePtrArray[tagId] != -1) {
        nextPtrArray[prePtrArray[tagId]] = nextPtrArray[tagId];
    }
    if (nextPtrArray[tagId] != -1) {
        prePtrArray[nextPtrArray[tagId]] = prePtrArray[tagId];
    }
    prePtrArray[tagId] = -1;
    nextPtrArray[tagId] = listHead;
    if (listHead >= 0) {
        if (prePtrArray[listHead] == -1)
            prePtrArray[listHead] = tagId;
        else
            panic("List head is not actually a list head!")
    }
    if (updateReplacement) rp->update(tagId, req);
}

void SparseTagArray::changeInPlace(Address lineAddr, const MemReq* req, int32_t tagId, int32_t mapId, int32_t listHead, bool approximate, bool updateReplacement) {
    tagArray[tagId] = lineAddr;
    mapPtrArray[tagId] = mapId;
    approximateArray[tagId] = approximate;
    if (prePtrArray[tagId] != -1) {
        nextPtrArray[prePtrArray[tagId]] = nextPtrArray[tagId];
    }
    if (nextPtrArray[tagId] != -1) {
        prePtrArray[nextPtrArray[tagId]] = prePtrArray[tagId];
    }
    prePtrArray[tagId] = -1;
    nextPtrArray[tagId] = listHead;
    if (listHead >= 0) {
        if (prePtrArray[listHead] == -1)
            prePtrArray[listHead] = tagId;
        else
            panic("List head is not actually a list head!")
    }
    if (updateReplacement) rp->update(tagId, req);
}

int32_t SparseTagArray::readMapId(int32_t tagId) {
    return mapPtrArray[tagId];
}

int32_t SparseTagArray::readDataId(int32_t tagId) {
    return mapPtrArray[tagId];
}

Address SparseTagArray::readAddress(int32_t tagId) {
    return tagArray[tagId];
}

int32_t SparseTagArray::readNextLL(int32_t tagId) {
    return nextPtrArray[tagId];
}

uint32_t SparseTagArray::getValidLines() {
    return validLines;
}

uint32_t SparseTagArray::countValidLines() {
    uint32_t count = 0;
    for (uint32_t i = 0; i < numLines; i++) {
        if (mapPtrArray[i] != -1)
            count++;
    }
    return count;
}

void SparseTagArray::print() {
    for (uint32_t i = 0; i < numLines; i++) {
        if (mapPtrArray[i] != -1) {
            info("%i: %lu, %i, %i, %i, %s", i, tagArray[i] << lineBits, prePtrArray[i], nextPtrArray[i], mapPtrArray[i], approximateArray[i] ? "approximate" : "exact");
        }
    }
}

SparseDataArray::SparseDataArray(uint32_t _numLines, uint32_t _assoc, ReplPolicy* _rp, HashFamily* _hf) : rp(_rp), hf(_hf), numLines(_numLines), assoc(_assoc) {
    mtagArray = gm_calloc<int32_t>(numLines);
    tagPtrArray = gm_calloc<int32_t>(numLines);
    approximateArray = gm_calloc<bool>(numLines);
    for (uint32_t i = 0; i < numLines; i++) {
        mtagArray[i] = -1;
        tagPtrArray[i] = -1;
        approximateArray[i] = false;
    }
    numSets = numLines/assoc;
    setMask = numSets - 1;
    validLines = 0;
    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
}

SparseDataArray::~SparseDataArray() {
    gm_free(mtagArray);
    gm_free(tagPtrArray);
    gm_free(approximateArray);
}

int32_t SparseDataArray::lookup(uint32_t map, const MemReq* req, bool updateReplacement) {
    uint32_t set = hf->hash(0, map) & setMask;
    uint32_t first = set*assoc;
    for (uint32_t id = first; id < first + assoc; id++) {
        if (mtagArray[id] == (int32_t)map && approximateArray[id]) {
            if (updateReplacement) rp->update(id, req);
            return id;
        }
    }
    return -1;
}

uint32_t SparseDataArray::computeMap(const DataLine data, DataType type, DataValue minValue, DataValue maxValue) {
    int64_t intAvgHash = 0, intRangeHash = 0;
    double floatAvgHash = 0, floatRangeHash = 0;
    int64_t intMax = std::numeric_limits<int64_t>::min(), intMin = std::numeric_limits<int64_t>::max(), intSum = 0;
    double floatMax = std::numeric_limits<double>::min(), floatMin = std::numeric_limits<double>::max(), floatSum = 0;
    double mapStep = 0;
    int32_t avgMap = 0, rangeMap = 0;
    uint32_t map = 0;
    switch (type)
    {
        case ZSIM_UINT8:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(uint8_t); i++) {
                intSum += ((uint8_t*) data)[i];
                if (((uint8_t*) data)[i] > intMax)
                    intMax = ((uint8_t*) data)[i];
                if (((uint8_t*) data)[i] < intMin)
                    intMin = ((uint8_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(uint8_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.UINT8)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.UINT8)
                panic("Received a value lower than the annotation's Min!!");
            if (zinfo->mapSize > sizeof(uint8_t)) {
                avgMap = intAvgHash;
                rangeMap = intRangeHash;
            } else {
                mapStep = (maxValue.UINT8 - minValue.UINT8)/std::pow(2,zinfo->mapSize-1);
                avgMap = intAvgHash/mapStep;
                rangeMap = intRangeHash/mapStep;
            }
            break;
        case ZSIM_INT8:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(int8_t); i++) {
                intSum += ((int8_t*) data)[i];
                if (((int8_t*) data)[i] > intMax)
                    intMax = ((int8_t*) data)[i];
                if (((int8_t*) data)[i] < intMin)
                    intMin = ((int8_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(int8_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.INT8)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.INT8)
                panic("Received a value lower than the annotation's Min!!");
            if (zinfo->mapSize > sizeof(int8_t)) {
                avgMap = intAvgHash;
                rangeMap = intRangeHash;
            } else {
                mapStep = (maxValue.INT8 - minValue.INT8)/std::pow(2,zinfo->mapSize-1);
                avgMap = intAvgHash/mapStep;
                rangeMap = intRangeHash/mapStep;
            }
            break;
        case ZSIM_UINT16:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(uint16_t); i++) {
                intSum += ((uint16_t*) data)[i];
                if (((uint16_t*) data)[i] > intMax)
                    intMax = ((uint16_t*) data)[i];
                if (((uint16_t*) data)[i] < intMin)
                    intMin = ((uint16_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(uint16_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.UINT16)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.UINT16)
                panic("Received a value lower than the annotation's Min!!");
            if (zinfo->mapSize > sizeof(uint16_t)) {
                avgMap = intAvgHash;
                rangeMap = intRangeHash;
            } else {
                mapStep = (maxValue.UINT16 - minValue.UINT16)/std::pow(2,zinfo->mapSize-1);
                avgMap = intAvgHash/mapStep;
                rangeMap = intRangeHash/mapStep;
            }
            break;
        case ZSIM_INT16:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(int16_t); i++) {
                intSum += ((int16_t*) data)[i];
                if (((int16_t*) data)[i] > intMax)
                    intMax = ((int16_t*) data)[i];
                if (((int16_t*) data)[i] < intMin)
                    intMin = ((int16_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(int16_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.INT16)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.INT16)
                panic("Received a value lower than the annotation's Min!!");
            if (zinfo->mapSize > sizeof(int16_t)) {
                avgMap = intAvgHash;
                rangeMap = intRangeHash;
            } else {
                mapStep = (maxValue.INT16 - minValue.INT16)/std::pow(2,zinfo->mapSize-1);
                avgMap = intAvgHash/mapStep;
                rangeMap = intRangeHash/mapStep;
            }
            break;
        case ZSIM_UINT32:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(uint32_t); i++) {
                intSum += ((uint32_t*) data)[i];
                if (((uint32_t*) data)[i] > intMax)
                    intMax = ((uint32_t*) data)[i];
                if (((uint32_t*) data)[i] < intMin)
                    intMin = ((uint32_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(uint32_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.UINT32)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.UINT32)
                panic("Received a value lower than the annotation's Min!!");
            mapStep = (maxValue.UINT32 - minValue.UINT32)/std::pow(2,zinfo->mapSize-1);
            avgMap = intAvgHash/mapStep;
            rangeMap = intRangeHash/mapStep;
            break;
        case ZSIM_INT32:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(int32_t); i++) {
                intSum += ((int32_t*) data)[i];
                if (((int32_t*) data)[i] > intMax)
                    intMax = ((int32_t*) data)[i];
                if (((int32_t*) data)[i] < intMin)
                    intMin = ((int32_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(int32_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.INT32)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.INT32)
                panic("Received a value lower than the annotation's Min!!");
            mapStep = (maxValue.INT32 - minValue.INT32)/std::pow(2,zinfo->mapSize-1);
            avgMap = intAvgHash/mapStep;
            rangeMap = intRangeHash/mapStep;
            break;
        case ZSIM_UINT64:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(uint64_t); i++) {
                intSum += ((uint64_t*) data)[i];
                if ((int64_t)(((uint64_t*) data)[i]) > intMax)
                    intMax = ((uint64_t*) data)[i];
                if ((int64_t)(((uint64_t*) data)[i]) < intMin)
                    intMin = ((uint64_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(uint64_t));
            intRangeHash = intMax - intMin;
            if (intMax > (int64_t)maxValue.UINT64)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < (int64_t)minValue.UINT64)
                panic("Received a value lower than the annotation's Min!!");
            mapStep = (maxValue.UINT64 - minValue.UINT64)/std::pow(2,zinfo->mapSize-1);
            avgMap = intAvgHash/mapStep;
            rangeMap = intRangeHash/mapStep;
            break;
        case ZSIM_INT64:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(int64_t); i++) {
                intSum += ((int64_t*) data)[i];
                if (((int64_t*) data)[i] > intMax)
                    intMax = ((int64_t*) data)[i];
                if (((int64_t*) data)[i] < intMin)
                    intMin = ((int64_t*) data)[i];
            }
            intAvgHash = intSum/(zinfo->lineSize/sizeof(int64_t));
            intRangeHash = intMax - intMin;
            if (intMax > maxValue.INT64)
                panic("Received a value bigger than the annotation's Max!!");
            if (intMin < minValue.INT64)
                panic("Received a value lower than the annotation's Min!!");
            mapStep = (maxValue.INT64 - minValue.INT64)/std::pow(2,zinfo->mapSize-1);
            avgMap = intAvgHash/mapStep;
            rangeMap = intRangeHash/mapStep;
            break;
        case ZSIM_FLOAT:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(float); i++) {
                floatSum += ((float*) data)[i];
                if (((float*) data)[i] > floatMax)
                    floatMax = ((float*) data)[i];
                if (((float*) data)[i] < floatMin)
                    floatMin = ((float*) data)[i];
            }
            floatAvgHash = floatSum/(zinfo->lineSize/sizeof(float));
            floatRangeHash = floatMax - floatMin;
            // if (floatMax > maxValue.FLOAT)
                // warn("Received a value bigger than the annotation's Max!! %.10f, %.10f", floatMax, maxValue.FLOAT);
            // if (floatMin < minValue.FLOAT)
                // warn("Received a value lower than the annotation's Min!! %.10f, %.10f", floatMin, minValue.FLOAT);
            mapStep = (maxValue.FLOAT - minValue.FLOAT)/std::pow(2,zinfo->mapSize-1);
            avgMap = floatAvgHash/mapStep;
            rangeMap = floatRangeHash/mapStep;
            break;
        case ZSIM_DOUBLE:
            for (uint32_t i = 0; i < zinfo->lineSize/sizeof(double); i++) {
                floatSum += ((double*) data)[i];
                if (((double*) data)[i] > floatMax)
                    floatMax = ((double*) data)[i];
                if (((double*) data)[i] < floatMin)
                    floatMin = ((double*) data)[i];
            }
            floatAvgHash = floatSum/(zinfo->lineSize/sizeof(double));
            floatRangeHash = floatMax - floatMin;
            // if (floatMax > maxValue.DOUBLE)
                // warn("Received a value bigger than the annotation's Max!! %.10f, %.10f", floatMax, maxValue.DOUBLE);
            // if (floatMin < minValue.DOUBLE)
                // warn("Received a value lower than the annotation's Min!! %.10f, %.10f", floatMin, minValue.DOUBLE);
            mapStep = (maxValue.DOUBLE - minValue.DOUBLE)/std::pow(2,zinfo->mapSize-1);
            avgMap = floatAvgHash/mapStep;
            rangeMap = floatRangeHash/mapStep;
            break;
        default:
            panic("Wrong Data Type!!");
    }
    map = ((uint32_t)avgMap << (32 - zinfo->mapSize)) >> (32 - zinfo->mapSize);
    rangeMap = ((uint32_t)rangeMap << (32 - zinfo->mapSize/2)) >> (32 - zinfo->mapSize/2);
    rangeMap = (rangeMap << zinfo->mapSize);
    map |= rangeMap;

    return map;
}

int32_t SparseDataArray::preinsert(uint32_t map, const MemReq* req, int32_t* tagId) {
    uint32_t set = hf->hash(0,map) & setMask;
    uint32_t first = set*assoc;

    uint32_t mapId = rp->rankCands(req, SetAssocCands(first, first+assoc));

    *tagId = tagPtrArray[mapId];
    return mapId;

}

void SparseDataArray::postinsert(int32_t map, const MemReq* req, int32_t mapId, int32_t tagId, bool approximate, bool updateReplacement) {
    if (tagPtrArray[mapId] == -1 && tagId != -1) {
        validLines++;
    } else if (tagPtrArray[mapId] != -1 && tagId == -1) {
        assert(validLines);
        validLines--;
    }
    rp->replaced(mapId);
    mtagArray[mapId] = map;
    tagPtrArray[mapId] = tagId;
    approximateArray[mapId] = approximate;
    if (updateReplacement) rp->update(mapId, req);
}

void SparseDataArray::changeInPlace(int32_t map, const MemReq* req, int32_t mapId, int32_t tagId, bool approximate, bool updateReplacement) {
    mtagArray[mapId] = map;
    tagPtrArray[mapId] = tagId;
    approximateArray[mapId] = approximate;
    if (updateReplacement) rp->update(mapId, req);
}

int32_t SparseDataArray::readListHead(int32_t mapId) {
    return tagPtrArray[mapId];

}
int32_t SparseDataArray::readMap(int32_t mapId) {
    return mtagArray[mapId];

}
uint32_t SparseDataArray::getValidLines() {
    return validLines;

}
uint32_t SparseDataArray::countValidLines() {
    uint32_t Counter = 0;
    for (uint32_t i = 0; i < numLines; i++) {
        if (tagPtrArray[i] != -1)
            Counter++;
    }
    return Counter;

}
void SparseDataArray::print() {
    for (uint32_t i = 0; i < numLines; i++) {
        if (tagPtrArray[i] != -1) {
            info("%i: %i, %i, %s", i, mtagArray[i], tagPtrArray[i], approximateArray[i] ? "approximate" : "exact");
        }
    }
}

/* ZCache implementation */

ZArray::ZArray(uint32_t _numLines, uint32_t _ways, uint32_t _candidates, ReplPolicy* _rp, HashFamily* _hf) //(int _size, int _lineSize, int _assoc, int _zassoc, ReplacementPolicy<T>* _rp, int _hashType)
    : rp(_rp), hf(_hf), numLines(_numLines), ways(_ways), cands(_candidates)
{
    assert_msg(ways > 1, "zcaches need >=2 ways to work");
    assert_msg(cands >= ways, "candidates < ways does not make sense in a zcache");
    assert_msg(numLines % ways == 0, "number of lines is not a multiple of ways");

    //Populate secondary parameters
    numSets = numLines/ways;
    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
    setMask = numSets - 1;

    lookupArray = gm_calloc<uint32_t>(numLines);
    array = gm_calloc<Address>(numLines);
    for (uint32_t i = 0; i < numLines; i++) {
        lookupArray[i] = i;  // start with a linear mapping; with swaps, it'll get progressively scrambled
    }
    swapArray = gm_calloc<uint32_t>(cands/ways + 2);  // conservative upper bound (tight within 2 ways)
}

void ZArray::initStats(AggregateStat* parentStat) {
    AggregateStat* objStats = new AggregateStat();
    objStats->init("array", "ZArray stats");
    statSwaps.init("swaps", "Block swaps in replacement process");
    objStats->append(&statSwaps);
    parentStat->append(objStats);
}

int32_t ZArray::lookup(const Address lineAddr, const MemReq* req, bool updateReplacement) {
    /* Be defensive: If the line is 0, panic instead of asserting. Now this can
     * only happen on a segfault in the main program, but when we move to full
     * system, phy page 0 might be used, and this will hit us in a very subtle
     * way if we don't check.
     */
    if (unlikely(!lineAddr)) panic("ZArray::lookup called with lineAddr==0 -- your app just segfaulted");

    for (uint32_t w = 0; w < ways; w++) {
        uint32_t lineId = lookupArray[w*numSets + (hf->hash(w, lineAddr) & setMask)];
        if (array[lineId] == lineAddr) {
            if (updateReplacement) {
                rp->update(lineId, req);
            }
            return lineId;
        }
    }
    return -1;
}

uint32_t ZArray::preinsert(const Address lineAddr, const MemReq* req, Address* wbLineAddr) {
    ZWalkInfo candidates[cands + ways]; //extra ways entries to avoid checking on every expansion

    bool all_valid = true;
    uint32_t fringeStart = 0;
    uint32_t numCandidates = ways; //seeds

    //info("Replacement for incoming 0x%lx", lineAddr);

    //Seeds
    for (uint32_t w = 0; w < ways; w++) {
        uint32_t pos = w*numSets + (hf->hash(w, lineAddr) & setMask);
        uint32_t lineId = lookupArray[pos];
        candidates[w].set(pos, lineId, -1);
        all_valid &= (array[lineId] != 0);
        //info("Seed Candidate %d addr 0x%lx pos %d lineId %d", w, array[lineId], pos, lineId);
    }

    //Expand fringe in BFS fashion
    while (numCandidates < cands && all_valid) {
        uint32_t fringeId = candidates[fringeStart].lineId;
        Address fringeAddr = array[fringeId];
        assert(fringeAddr);
        for (uint32_t w = 0; w < ways; w++) {
            uint32_t hval = hf->hash(w, fringeAddr) & setMask;
            uint32_t pos = w*numSets + hval;
            uint32_t lineId = lookupArray[pos];

            // Logically, you want to do this...
#if 0
            if (lineId != fringeId) {
                //info("Candidate %d way %d addr 0x%lx pos %d lineId %d parent %d", numCandidates, w, array[lineId], pos, lineId, fringeStart);
                candidates[numCandidates++].set(pos, lineId, (int32_t)fringeStart);
                all_valid &= (array[lineId] != 0);
            }
#endif
            // But this compiles as a branch and ILP sucks (this data-dependent branch is long-latency and mispredicted often)
            // Logically though, this is just checking for whether we're revisiting ourselves, so we can eliminate the branch as follows:
            candidates[numCandidates].set(pos, lineId, (int32_t)fringeStart);
            all_valid &= (array[lineId] != 0);  // no problem, if lineId == fringeId the line's already valid, so no harm done
            numCandidates += (lineId != fringeId); // if lineId == fringeId, the cand we just wrote will be overwritten
        }
        fringeStart++;
    }

    //Get best candidate (NOTE: This could be folded in the code above, but it's messy since we can expand more than zassoc elements)
    assert(!all_valid || numCandidates >= cands);
    numCandidates = (numCandidates > cands)? cands : numCandidates;

    //info("Using %d candidates, all_valid=%d", numCandidates, all_valid);

    uint32_t bestCandidate = rp->rankCands(req, ZCands(&candidates[0], &candidates[numCandidates]));
    assert(bestCandidate < numLines);

    //Fill in swap array

    //Get the *minimum* index of cands that matches lineId. We need the minimum in case there are loops (rare, but possible)
    uint32_t minIdx = -1;
    for (uint32_t ii = 0; ii < numCandidates; ii++) {
        if (bestCandidate == candidates[ii].lineId) {
            minIdx = ii;
            break;
        }
    }
    assert(minIdx >= 0);
    //info("Best candidate is %d lineId %d", minIdx, bestCandidate);

    lastCandIdx = minIdx; //used by timing simulation code to schedule array accesses

    int32_t idx = minIdx;
    uint32_t swapIdx = 0;
    while (idx >= 0) {
        swapArray[swapIdx++] = candidates[idx].pos;
        idx = candidates[idx].parentIdx;
    }
    swapArrayLen = swapIdx;
    assert(swapArrayLen > 0);

    //Write address of line we're replacing
    *wbLineAddr = array[bestCandidate];

    return bestCandidate;
}

void ZArray::postinsert(const Address lineAddr, const MemReq* req, uint32_t candidate) {
    //We do the swaps in lookupArray, the array stays the same
    assert(lookupArray[swapArray[0]] == candidate);
    for (uint32_t i = 0; i < swapArrayLen-1; i++) {
        //info("Moving position %d (lineId %d) <- %d (lineId %d)", swapArray[i], lookupArray[swapArray[i]], swapArray[i+1], lookupArray[swapArray[i+1]]);
        lookupArray[swapArray[i]] = lookupArray[swapArray[i+1]];
    }
    lookupArray[swapArray[swapArrayLen-1]] = candidate; //note that in preinsert() we walk the array backwards when populating swapArray, so the last elem is where the new line goes
    //info("Inserting lineId %d in position %d", candidate, swapArray[swapArrayLen-1]);

    rp->replaced(candidate);
    array[candidate] = lineAddr;
    rp->update(candidate, req);

    statSwaps.inc(swapArrayLen-1);
}

/* Partitioned ZCache implementation */

ZArray_Par::ZArray_Par(uint32_t _numLines, uint32_t _ways, uint32_t _candidates, ReplPolicy* _rp, HashFamily* _hf) //(int _size, int _lineSize, int _assoc, int _zassoc, ReplacementPolicy<T>* _rp, int _hashType)
    : rp(_rp), hf(_hf), numLines(_numLines), ways(_ways), cands(_candidates)
{
    assert_msg(ways > 1, "zcaches need >=2 ways to work");
    assert_msg(cands >= ways, "candidates < ways does not make sense in a zcache");
    assert_msg(numLines % ways == 0, "number of lines is not a multiple of ways");

    //Populate secondary parameters
    numSets = numLines/ways;
    assert_msg(isPow2(numSets), "must have a power of 2 # sets, but you specified %d", numSets);
    setMask = numSets - 1;

    lookupArray = gm_calloc<uint32_t>(numLines);
    array = gm_calloc<Address>(numLines);
    domain_ID = gm_calloc<uint32_t>(numLines);
    for (uint32_t i = 0; i < numLines; i++) {
        lookupArray[i] = i;  // start with a linear mapping; with swaps, it'll get progressively scrambled
        domain_ID[i] = uint32_t(-1);
    }
    swapArray = gm_calloc<uint32_t>(cands/ways + 2);  // conservative upper bound (tight within 2 ways)
    
    //Initialize line counters for each domain, assume two domains right now
    line_counters[0] = 0;
    line_counters[1] = 0;
}

void ZArray_Par::initStats(AggregateStat* parentStat) {
    AggregateStat* objStats = new AggregateStat();
    objStats->init("array", "ZArray stats");
    statSwaps.init("swaps", "Block swaps in replacement process");
    objStats->append(&statSwaps);
    parentStat->append(objStats);
}

int32_t ZArray_Par::lookup(const Address lineAddr, const MemReq* req, bool updateReplacement) {
    /* Be defensive: If the line is 0, panic instead of asserting. Now this can
     * only happen on a segfault in the main program, but when we move to full
     * system, phy page 0 might be used, and this will hit us in a very subtle
     * way if we don't check.
     */
    if (unlikely(!lineAddr)) panic("ZArray::lookup called with lineAddr==0 -- your app just segfaulted");

    for (uint32_t w = 0; w < ways; w++) {
        uint32_t lineId = lookupArray[w*numSets + (hf->hash(w, lineAddr) & setMask)];
        if (array[lineId] == lineAddr) {
            if (updateReplacement) {
                rp->update(lineId, req);
            }
            return lineId;
        }
    }
    return -1;
}

uint32_t ZArray_Par::preinsert(const Address lineAddr, const MemReq* req, Address* wbLineAddr) {
    ZWalkInfo candidates[cands + ways]; //extra ways entries to avoid checking on every expansion

    bool all_valid = true;
    uint32_t fringeStart = 0;
    uint32_t numCandidates = ways; //seeds
    uint32_t actual_numCands = 0; //count actual number of candidates

    //info("Replacement for incoming 0x%lx", lineAddr);

    //Seeds
    for (uint32_t w = 0; w < ways; w++) {
        uint32_t pos = w*numSets + (hf->hash(w, lineAddr) & setMask);
        uint32_t lineId = lookupArray[pos];
        candidates[w].set(pos, lineId, -1, domain_ID[lineId]);
        if (domain_ID[lineId] == req->srcId || domain_ID[lineId] == uint32_t(-1)) actual_numCands++;
        all_valid &= (array[lineId] != 0);
        //info("Seed Candidate %d addr 0x%lx pos %d lineId %d", w, array[lineId], pos, lineId);
    }

    //Expand fringe in BFS fashion
    while (numCandidates < cands && all_valid) {
        uint32_t fringeId = candidates[fringeStart].lineId;
        Address fringeAddr = array[fringeId];
        assert(fringeAddr);
        for (uint32_t w = 0; w < ways; w++) {
            uint32_t hval = hf->hash(w, fringeAddr) & setMask;
            uint32_t pos = w*numSets + hval;
            uint32_t lineId = lookupArray[pos];

            // Logically, you want to do this...
#if 0
            if (lineId != fringeId) {
                //info("Candidate %d way %d addr 0x%lx pos %d lineId %d parent %d", numCandidates, w, array[lineId], pos, lineId, fringeStart);
                candidates[numCandidates++].set(pos, lineId, (int32_t)fringeStart);
                all_valid &= (array[lineId] != 0);
            }
#endif
            // But this compiles as a branch and ILP sucks (this data-dependent branch is long-latency and mispredicted often)
            // Logically though, this is just checking for whether we're revisiting ourselves, so we can eliminate the branch as follows:
            candidates[numCandidates].set(pos, lineId, (int32_t)fringeStart, domain_ID[lineId]);
            if (domain_ID[lineId] == req->srcId || domain_ID[lineId] == uint32_t(-1)) actual_numCands++;
            all_valid &= (array[lineId] != 0);  // no problem, if lineId == fringeId the line's already valid, so no harm done
            numCandidates += (lineId != fringeId); // if lineId == fringeId, the cand we just wrote will be overwritten
        }
        fringeStart++;
    }

    //Get best candidate (NOTE: This could be folded in the code above, but it's messy since we can expand more than zassoc elements)
    assert(!all_valid || numCandidates >= cands);
    numCandidates = (numCandidates > cands)? cands : numCandidates;

    //info("Using %d candidates, all_valid=%d", numCandidates, all_valid);
    
    //no actual candidates (i.e. cache lines from the same security domain)
    if (actual_numCands == 0) return uint32_t(-1);
    
    //copy actual candidates to another array for candidate selection
    ZWalkInfo* actualCandidates = new ZWalkInfo[actual_numCands];
    uint32_t* candIndex = new uint32_t[actual_numCands];
    
    uint32_t counter = 0;
    if (line_counters[req->srcId] < numLines/2)
    {
        for (uint32_t i = 0; i < numCandidates; i++)
        {
        
            if (candidates[i].domain_ID == req->srcId || candidates[i].domain_ID == uint32_t(-1)) 
            {
                actualCandidates[counter] = candidates[i];
                candIndex[counter] = i;
                counter++;
            }
        }
        line_counters[req->srcId]++;
    }
    else
    {
        for (uint32_t i = 0; i < numCandidates; i++)
        {
        
            if (candidates[i].domain_ID == req->srcId) 
            {
                actualCandidates[counter] = candidates[i];
                candIndex[counter] = i;
                counter++;
            }
        }
    }
    
    uint32_t tempIndex = rp->rankCands(req, ZCands(&actualCandidates[0], &actualCandidates[counter]));
    uint32_t bestCandidate = candIndex[tempIndex];
    assert(bestCandidate < numLines);

    //Fill in swap array

    //Get the *minimum* index of cands that matches lineId. We need the minimum in case there are loops (rare, but possible)
    int32_t minIdx = -1;
    for (uint32_t ii = 0; ii < numCandidates; ii++) {
        if (bestCandidate == candidates[ii].lineId) {
            minIdx = ii;
            break;
        }
    }
    assert(minIdx >= 0);
    //info("Best candidate is %d lineId %d", minIdx, bestCandidate);

    lastCandIdx = minIdx; //used by timing simulation code to schedule array accesses

    int32_t idx = minIdx;
    uint32_t swapIdx = 0;
    while (idx >= 0) {
        swapArray[swapIdx++] = candidates[idx].pos;
        idx = candidates[idx].parentIdx;
    }
    swapArrayLen = swapIdx;
    assert(swapArrayLen > 0);

    //Write address of line we're replacing
    *wbLineAddr = array[bestCandidate];

    return bestCandidate;
}

void ZArray_Par::postinsert(const Address lineAddr, const MemReq* req, uint32_t candidate) {
    //No insertion happens
    if (candidate == uint32_t(-1)) return;
    //We do the swaps in lookupArray, the array stays the same
    assert(lookupArray[swapArray[0]] == candidate);
    for (uint32_t i = 0; i < swapArrayLen-1; i++) {
        //info("Moving position %d (lineId %d) <- %d (lineId %d)", swapArray[i], lookupArray[swapArray[i]], swapArray[i+1], lookupArray[swapArray[i+1]]);
        lookupArray[swapArray[i]] = lookupArray[swapArray[i+1]];
    }
    lookupArray[swapArray[swapArrayLen-1]] = candidate; //note that in preinsert() we walk the array backwards when populating swapArray, so the last elem is where the new line goes
    //info("Inserting lineId %d in position %d", candidate, swapArray[swapArrayLen-1]);

    rp->replaced(candidate);
    array[candidate] = lineAddr;
    domain_ID[candidate] = req->srcId;
    rp->update(candidate, req);

    statSwaps.inc(swapArrayLen-1);
}