#!/bin/bash

cfg_file_name="spgemm_4c.cfg"
# TODO: change csv_file_name to the correct name (diag, rmat, etc.)
csv_file_name="data/spa_4c_longrow.csv"

nop_app_name="/home/cassiel/CacheSim/SpBLAS/SpGEMM/spgemm_spa_nop "
idx_app_name="/home/cassiel/CacheSim/SpBLAS/SpGEMM/spgemm_spa_idx "
acc_app_name="/home/cassiel/CacheSim/SpBLAS/SpGEMM/spgemm_spa_acc "
# TODO: change matrix_path to the correct path
matrix_path="/home/cassiel/matrix/gen_mtx/"
# matrix_path="/home/cassiel/matrix/gen_mtx/"

gen_matrix() {
    matrix="${matrix_path}${matrix_name}"
    nop_csv_name="nop_${matrix_name}"
    idx_csv_name="idx_${matrix_name}"
    acc_csv_name="acc_${matrix_name}"
}
gen_data() {
    python change_cfg.py "$cfg_file_name" "$nop_app_name" "$matrix"
    $ZSIM_HOME/zsim "$cfg_file_name"
    python compute-zsim-ev.py -a "$nop_csv_name" -o "$csv_file_name" -w
    python change_cfg.py "$cfg_file_name" "$idx_app_name" "$matrix"
    $ZSIM_HOME/zsim "$cfg_file_name"
    python compute-zsim-ev.py -a "$idx_csv_name" -o "$csv_file_name" -w
    python change_cfg.py "$cfg_file_name" "$acc_app_name" "$matrix"
    $ZSIM_HOME/zsim "$cfg_file_name"
    python compute-zsim-ev.py -a "$acc_csv_name" -o "$csv_file_name" -w
}

for i in 2 3 4 5 6; do
  # TODO: change matrix_name to the correct name
  matrix_name="longrow${i}.mtx"
  gen_matrix
  gen_data
done

# TODO: plot_speedup.py需要手动改
# python plot_speedup.py "$csv_file_name"