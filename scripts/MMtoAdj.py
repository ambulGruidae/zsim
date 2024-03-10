import sys
import numpy as np
from scipy.io import mmread


def read_coo_matrix(filename):
    coo = mmread(filename)
    num_vertices = coo.shape[0]
    num_edges = coo.nnz
    row_indices, col_indices = coo.row, coo.col
    return num_vertices, num_edges, row_indices, col_indices


def coo_to_adjacency(num_vertices, num_edges, row_indices, col_indices):
    offsets = np.zeros(num_vertices+1, dtype=int)
    edges = np.zeros(num_edges, dtype=int)

    for i in range(num_edges):
        offsets[row_indices[i]+1] += 1
    offsets = np.cumsum(offsets)

    for i in range(num_edges):
        j = offsets[row_indices[i]]
        edges[j] = col_indices[i]
        offsets[row_indices[i]] += 1

    return offsets[:-1], edges


def save_adjacency_graph(filename, num_vertices, num_edges, offsets, edges):
    with open(filename, 'w') as f:
        f.write("AdjacencyGraph\n")
        f.write(str(num_vertices) + "\n")
        f.write(str(num_edges) + "\n")
        for offset in offsets:
            f.write(str(offset) + "\n")
        for edge in edges:
            f.write(str(edge) + "\n")


def main(argv):
    input_filename = argv[0]
    output_filename = argv[1]
    num_vertices, num_edges, row_indices, col_indices = read_coo_matrix(input_filename)
    offsets, edges = coo_to_adjacency(num_vertices, num_edges, row_indices, col_indices)
    save_adjacency_graph(output_filename, num_vertices, num_edges, offsets, edges)


if __name__ == '__main__':
    main(sys.argv[1:])
