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
matrices    = ['0.013%', '0.029%', '0.054%', '0.254%', '0.507%', 'Gmean']
methods     = ['Nop', 'Idx', 'Idx+Acc']

with open('spa_1c_longrow.csv', newline='') as file:
    reader = csv.reader(file, delimiter=',', quotechar='"')
    data = [[row[0], float(row[1].replace(',',''))] for row in reader if len(row) >= 2]
    groups = [[data[i], data[i+1], data[i+2]] for i in range(0, len(data), 3)]

# print(groups)

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
    print(ind + p * bwidth)



ax = plt.gca()
ax.set_title('App-SpGEMM', fontweight = 'bold')
# ax.set_xlabel('Sparsity (Random)', fontweight = 'bold')
ax.set_xlabel('Sparsity (Long Row)', fontweight = 'bold')
ax.set_ylabel('Speedup over Nop', fontweight = 'bold')
ax.set_xticks(np.arange(len(matrices)) + 1 * bwidth)
ax.set_xticklabels(matrices)
ax.set_yticks(np.arange(0, 1.1 * maxVal + 0.5, 0.5))
ax.grid(alpha = 0.64, axis = 'y', linestyle = '--')
ax.legend(loc = 'upper left', framealpha = 1.0, ncol = 3, fontsize = 10)
ax.axhline(y = 1, linestyle = '--', color = 'k')
ax.axvline(x = 4.73, linestyle = '--', color = 'k')

plt.tight_layout()
plt.savefig('speedup_longrow.pdf')
# plt.savefig('speedup_blk.pdf')
# plt.savefig('rand_speedup.pdf')
