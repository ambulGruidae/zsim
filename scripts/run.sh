#!/bin/bash

cfg_file_name="ligra_1c.cfg"
# TODO: change csv_file_name to the correct name (diag, rmat, etc.)
csv_file_name="data/mis_1c.csv"

nop_app_name="/root/ligra-nop-long/apps/MIS\ -rounds\ 1 "
acc_app_name="/root/ligra-acc-long/apps/MIS\ -rounds\ 1 "
# idx_app_name="/home/cassiel/CacheSim/SpBLAS/SpGEMM/spgemm_spa_idx "
# acc_app_name="/home/cassiel/CacheSim/SpBLAS/SpGEMM/spgemm_spa_acc "
# TODO: change graph_path to the correct path
graph_path="/matrix/graph/"

graph_arr=("arabic-2005" "com-Orkut" "nlpkkt240" "p2p-Gnutella31" "soc-Pokec" "uk-2005" "webbase-2001" "com-LiveJournal" "uk-2002")

gen_graph() {
    graph="${graph_path}${graph_name}"
    nop_csv_name="nop_${graph_name}"
    # idx_csv_name="idx_${graph_name}"
    acc_csv_name="acc_${graph_name}"
}

gen_data() {
  echo python3 change_cfg.py "$cfg_file_name" "$nop_app_name" "$graph"
  echo $ZSIM_HOME/zsim "$cfg_file_name"
  echo python3 compute-zsim-ev.py -a "$nop_csv_name" -o "$csv_file_name" -w
  echo python3 change_cfg.py "$cfg_file_name" "$acc_app_name" "$graph"
  echo $ZSIM_HOME/zsim "$cfg_file_name"
  echo python3 compute-zsim-ev.py -a "$acc_csv_name" -o "$csv_file_name" -w
}

for graph_name in "${graph_arr[@]}"; do
  gen_graph
  gen_data
done

# TODO: plot_speedup.py需要手动改
# python plot_speedup.py "$csv_file_name"