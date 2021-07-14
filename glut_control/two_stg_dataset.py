import numpy as np
import sys

def two_stg_dataset(X, Y):
    dim_nfeats = len(X[0][1][0])

    gclasses = 2
    NODES_PER_GRAPH = 100

    num_nodes = 0
    num_edges = 0
    num_graphs = 0

    edges_file = open('ALMA_A.txt', 'w')                        # STORE EDGES
    graph_indicator = open('ALMA_graph_indicator.txt', 'w')     # INDEX NODES TO GRAPHS
    node_labels = open('ALMA_node_labels.txt', 'w')             # ALL ZEROES
    graph_labels = open('ALMA_graph_labels.txt', 'w')           # GRAPH CLASS LABELS
    node_attributes = open('ALMA_node_attributes.txt', 'w')     # NODE FEATURES

    for graph in X:
        y = 0 + (num_graphs * NODES_PER_GRAPH)
        for row in graph[0]:
            x = 0 + (num_graphs * NODES_PER_GRAPH)
            for column in row:
                if column == 1.0:
                    edges_file.write(str(x) + ', ' + str(y) + '\n')  # WRITE THE EDGES
                    edges_file.write(str(y) + ', ' + str(x) + '\n')
                    num_edges += 2
                x += 1
            y += 1
        edges_file.write(str(num_graphs * NODES_PER_GRAPH) + ', ' + str(50 + (num_graphs * NODES_PER_GRAPH)) + '\n')
        edges_file.write(str(50 + (num_graphs * NODES_PER_GRAPH)) + ', ' + str(num_graphs * NODES_PER_GRAPH) + '\n')
        num_edges += 2                                          # LINK THE TREES
        for i in range(NODES_PER_GRAPH):
            graph_indicator.write(str(num_graphs) + '\n')       # LABEL NODES TO GRAPHS THEY BELONG WITH
            node_labels.write('0\n')                            # FILL NODE LABELS WITH NOTHING (ACTUAL NODE DATA IN NODE_ATTRIBUTES)
        graph_labels.write(str(Y[num_graphs]) + '\n')           # WRITE GRAPH CLASS LABEL
        for features in graph[1]:
            for element in features:
                node_attributes.write(str(element) + ', ')      # WRITE NODE FEATURES
            node_attributes.write('\n')
        num_graphs += 1                                         # ITERATE NUM_GRAPHS AND NUM_NODES
        num_nodes += NODES_PER_GRAPH

        # sys.exit()                                            # TEST ON ONE GRAPH

    edges_file.close()
    graph_indicator.close()
    node_labels.close()
    graph_labels.close()
    node_attributes.close()

    sys.exit()