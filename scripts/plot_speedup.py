import os
import matplotlib.pyplot as plt
import numpy as np
import csv

# random
# matrices    = ['0.01%', '0.05%', '0.1%', '0.5%', '1.0%', 'Gmean']
# 16 * 16 block
# matrices    = ['0.0048%', '0.032%', '0.063%', '0.353%', '0.607%', 'Gmean']
# long row
# matrices    = ['0.013%', '0.029%', '0.054%', '0.254%', '0.507%', 'Gmean']
# diagonal
# matrices    = ['0.005%', '0.019%', '0.043%', '0.245%', '0.497%', 'Gmean']
# matrices    = ['0.005%', '0.019%', '0.044%', '0.245%', '0.497%', 'Gmean']
# matrices    = ['16', '17', '18', '19', '20', '21', 'Gmean']
matrices    = ['3', '6', '12', '24', '48', 'Gmean']
methods     = ['Nop', 'Idx', 'Idx+Acc']
# methods     = ['Nop', 'Idx+Acc']

def read_file(filename):
    with open(filename, newline='') as file:
        reader = csv.reader(file, delimiter=',', quotechar='"')
        data = [[row[0], float(row[1].replace(',',''))] for row in reader if len(row) >= 2]
        groups = [[data[i], data[i+1], data[i+2]] for i in range(0, len(data), 3)]
        # groups = [[data[i], data[i+1]] for i in range(0, len(data), 2)]
    return groups

# print(groups)


def gen_filename(filename_prefix, app_name, fig_format):
    file_cnt = 1
    while os.path.exists(f'{filename_prefix}_{app_name}{file_cnt}.{fig_format}'):
        file_cnt += 1

    new_filename = f'{filename_prefix}_{app_name}{file_cnt}.{fig_format}'
    return new_filename


def draw(filename, appname):
    groups = read_file(filename)
    titlename = 'App-SpGEMM'

    fig = plt.figure(figsize = (5, 3))
    bwidth = 1 / (1.25 + len(methods))
    maxVal = 0
    for p in range(len(methods)):
        vals = [group[0][1]/group[p][1] for group in groups]
        avg_val = sum(vals) / len(vals)
        vals.append(avg_val)
        # print(avg_val, methods[p])
        maxVal = max(maxVal, max(vals))
        ind = np.arange(len(vals))
        plt.bar(ind + p * bwidth, vals, width = bwidth, label = methods[p]) 
        # print(ind + p * bwidth)
    ax = plt.gca()
    ax.set_title(titlename, fontweight = 'bold')
    # ax.set_xlabel('Sparsity (Diag)', fontweight = 'bold')
    ax.set_xlabel('#times the number of rMAT edges (m = n = 8192)', fontweight = 'bold')
    ax.set_ylabel('Speedup over Nop', fontweight = 'bold')
    ax.set_xticks(np.arange(len(matrices)) + 1 * bwidth)
    ax.set_xticklabels(matrices)
    ax.set_yticks(np.arange(0, 1.1 * maxVal + 0.5, int(maxVal+0.5)/5))
    ax.grid(alpha = 0.64, axis = 'y', linestyle = '--')
    ax.legend(loc = 'upper left', framealpha = 1.0, ncol = 3, fontsize = 10)
    ax.axhline(y = 1, linestyle = '--', color = 'k')
    # ax.axvline(x = 4.73, linestyle = '--', color = 'k')


    fig_name = gen_filename('speedup', appname, 'pdf')
    plt.tight_layout()
    plt.savefig('./fig/' + fig_name)

# plt.savefig('speedup_blk.pdf')
# plt.savefig('rand_speedup.pdf')

def main(argv):
    if len(argv) != 3:
        print('Usage: python plot_speedup.py <filename> <appname>')
        return
    filename = argv[1]
    appname = argv[2]
    draw(filename, appname)


if __name__ == '__main__':
    import sys
    main(sys.argv)