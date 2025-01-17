import sys, getopt
import h5py
import csv
import numpy as np



f = h5py.File('zsim-ev.h5', 'r')

dest = f["stats"]["root"]

stats = dest[-1]

time = stats['time']

# NOTE: compute IPC
coreStats = stats['core']
totalInstrs = coreStats['instrs']
totalInstrs_avg = np.average(totalInstrs)
totalCycles = coreStats['cycles']
totalCycles_avg = np.average(totalCycles)

ipc_avg = (1. * totalInstrs_avg) / totalCycles_avg
print("Cycles:", format(totalCycles_avg,","))
print("Instr.:", format(totalInstrs_avg,","))
print("IPC:", ipc_avg)

# NOTE: compute L1d miss rate
l1dCacheStats = stats['l1d']
# print(l1dCacheStats.dtype.names)
l1d_fhGETS = l1dCacheStats['fhGETS']
l1d_fhGETX = l1dCacheStats['fhGETX']
l1d_hGETS = l1dCacheStats['hGETS']
l1d_hGETX = l1dCacheStats['hGETX']
l1d_mGETS = l1dCacheStats['mGETS']
l1d_mGETXIM = l1dCacheStats['mGETXIM']
l1d_mGETXSM = l1dCacheStats['mGETXSM']

l1d_all = l1d_fhGETS + l1d_fhGETX + l1d_hGETS + l1d_hGETX + l1d_mGETS + l1d_mGETXIM + l1d_mGETXSM
l1d_all_avg = np.average(l1d_all)
l1d_misses = l1d_mGETS + l1d_mGETXIM + l1d_mGETXSM
l1d_misses_avg = np.average(l1d_misses)
l1d_hit_rate_avg = (1. * (l1d_all_avg - l1d_misses_avg)) / l1d_all_avg
print("L1d hit rate:", l1d_hit_rate_avg)

# NOTE: compute L1i miss rate
l1iCacheStats = stats['l1i']
# print(l1iCacheStats.dtype.names)
l1i_fhGETS = l1iCacheStats['fhGETS']
l1i_fhGETX = l1iCacheStats['fhGETX']
l1i_hGETS = l1iCacheStats['hGETS']
l1i_hGETX = l1iCacheStats['hGETX']
l1i_mGETS = l1iCacheStats['mGETS']
l1i_mGETXIM = l1iCacheStats['mGETXIM']
l1i_mGETXSM = l1iCacheStats['mGETXSM']

l1i_all = l1i_fhGETS + l1i_fhGETX + l1i_hGETS + l1i_hGETX + l1i_mGETS + l1i_mGETXIM + l1i_mGETXSM
l1i_all_avg = np.average(l1i_all)
l1i_misses = l1i_mGETS + l1i_mGETXIM + l1i_mGETXSM
l1i_misses_avg = np.average(l1i_misses)
l1i_hit_rate_avg = (1. * (l1i_all_avg - l1i_misses_avg)) / l1i_all_avg
print("L1i hit rate:", l1i_hit_rate_avg)

# NOTE: compute l1s miss rate
l1sCacheStats = stats['l1s']
# print(l1sCacheStats.dtype.names)
l1s_fhGETS = l1sCacheStats['fhGETS']
l1s_fhGETX = l1sCacheStats['fhGETX']
l1s_hGETS = l1sCacheStats['hGETS']
l1s_hGETX = l1sCacheStats['hGETX']
l1s_mGETS = l1sCacheStats['mGETS']
l1s_mGETXIM = l1sCacheStats['mGETXIM']
l1s_mGETXSM = l1sCacheStats['mGETXSM']

l1s_all = l1s_fhGETS + l1s_fhGETX + l1s_hGETS + l1s_hGETX + l1s_mGETS + l1s_mGETXIM + l1s_mGETXSM
l1s_all_avg = np.average(l1s_all)
l1s_misses = l1s_mGETS + l1s_mGETXIM + l1s_mGETXSM
l1s_misses_avg = np.average(l1s_misses)
l1s_hit_rate_avg = (1. * (l1s_all_avg - l1s_misses_avg)) / l1s_all_avg
print("L1s hit rate:", l1s_hit_rate_avg)

# # NOTE: compute L2 miss rate
l2CacheStats = stats['l2']
# print(l2CacheStats.dtype.names)
l2_hGETS = l2CacheStats['hGETS']
l2_hGETX = l2CacheStats['hGETX']
l2_mGETS = l2CacheStats['mGETS']
l2_mGETXIM = l2CacheStats['mGETXIM']
l2_mGETXSM = l2CacheStats['mGETXSM']

l2_all =  l2_hGETS + l2_hGETX + l2_mGETS + l2_mGETXIM + l2_mGETXSM
l2_all_avg = np.average(l2_all)
l2_misses =  l2_mGETS + l2_mGETXIM + l2_mGETXSM
l2_misses_avg = np.average(l2_misses)
l2_hit_rate_avg = (1. * (l2_all_avg - l2_misses_avg)) / l2_all_avg
print("L2 hit rate:", l2_hit_rate_avg)

# NOTE: compute L3 miss rate
l3CacheStats = stats['l3']
# print(l3CacheStats.dtype.names)
l3_hGETS = l3CacheStats['hGETS']
l3_hGETX = l3CacheStats['hGETX']
l3_mGETS = l3CacheStats['mGETS']
l3_mGETXIM = l3CacheStats['mGETXIM']
l3_mGETXSM = l3CacheStats['mGETXSM']

l3_all =  l3_hGETS + l3_hGETX + l3_mGETS + l3_mGETXIM + l3_mGETXSM
l3_all_avg = np.average(l3_all)
l3_misses =  l3_mGETS + l3_mGETXIM + l3_mGETXSM
l3_misses_avg = np.average(l3_misses)
l3_hit_rate_avg = (1. * (l3_all_avg - l3_misses_avg)) / l3_all_avg
print("L3 hit rate:", l3_hit_rate_avg)

memStats = stats['mem']
# print(memStats.dtype.names)
mem_rd = memStats['rd']
mem_rd_avg = np.average(mem_rd)
mem_wr = memStats['wr']
mem_wr_avg = np.average(mem_wr)
mem_rdlat = memStats['rdlat']
mem_rdlat_avg = np.average(mem_rdlat)
mem_wrlat = memStats['wrlat']
mem_wrlat_avg = np.average(mem_wrlat)
print("Mem rd:", format(mem_rd_avg,","))
print("Mem wr:", format(mem_wr_avg,","))
print("Mem rdlat:", format(mem_rdlat_avg,","))
print("Mem wrlat:", format(mem_wrlat_avg,","))

def main(argv):
    appName = ''
    csvPath = ''
    isWrite = False
    try:
        opts, args = getopt.getopt(argv,"ha:o:w",["appname=","csvfile=", "write"])
    except getopt.GetoptError:
        print('compute-zsim-ev.py -a <appname> -o <csvfile> -w')
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print('compute-zsim-ev.py -a <appname> -o <csvfile> -w')
            sys.exit()
        elif opt in ("-a", "--appname"):
            appName = arg
        elif opt in ("-o", "--csvfile"):
            csvPath = arg
        elif opt in ("-w", "--write"):
            isWrite = True
    
    # NOTE: write to file
    if isWrite:
        with open(csvPath, 'a+') as f:
            writer = csv.writer(f)
            datarow = [appName, format(totalCycles_avg,","), format(totalInstrs_avg,","), ipc_avg, 
                       l1d_hit_rate_avg, l1i_hit_rate_avg, l1s_hit_rate_avg, l2_hit_rate_avg, l3_hit_rate_avg, 
                       format(mem_rd_avg,","), format(mem_wr_avg,","), format(mem_rdlat_avg,","), format(mem_wrlat_avg,",")]
            writer.writerow(datarow)


if __name__ == "__main__":
    main(sys.argv[1:])