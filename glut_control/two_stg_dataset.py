import numpy as np
import sys
import os.path

def two_stg_dataset(X, Y):
    dim_nfeats = len(X[0][1][0])

    gclasses = 2
    NODES_PER_GRAPH = 100

    num_nodes = 0
    num_edges = 0
    num_graphs = 0

    if not os.path.exists('2STG'):
        os.mkdir('2STG')

    if os.path.exists('2STG/ALMA_meta.txt'):
        metadata = open('2STG/ALMA_meta.txt', 'r')                   # STORE NUM_XXXXX
        num_nodes = int(metadata.readline())
        num_graphs = int(metadata.readline())
        metadata.close()
    edges_file = open('2STG/ALMA_A.txt', 'a')                        # STORE EDGES
    graph_indicator = open('2STG/ALMA_graph_indicator.txt', 'a')     # INDEX NODES TO GRAPHS
    node_labels = open('2STG/ALMA_node_labels.txt', 'a')             # ALL ZEROES
    graph_labels = open('2STG/ALMA_graph_labels.txt', 'a')           # GRAPH CLASS LABELS
    node_attributes = open('2STG/ALMA_node_attributes.txt', 'a')     # NODE FEATURES

    for graph in X:
        y = 1 + (num_graphs * NODES_PER_GRAPH)                       # 1, NOT 0, THAT'S HOW DATA_LOAD EXPECTS
        for row in graph[0]:
            x = 1 + (num_graphs * NODES_PER_GRAPH)
            for column in row:
                if column == 1.0:
                    edges_file.write(str(y) + ', ' + str(x) + '\n')  # WRITE THE EDGES
                    edges_file.write(str(x) + ', ' + str(y) + '\n')
                    num_edges += 2
                x += 1
            y += 1
        edges_file.write(str(1 + (num_graphs * NODES_PER_GRAPH)) + ', ' + str(51 + (num_graphs * NODES_PER_GRAPH)) + '\n')
        edges_file.write(str(51 + (num_graphs * NODES_PER_GRAPH)) + ', ' + str(1 + (num_graphs * NODES_PER_GRAPH)) + '\n')
        num_edges += 2                                          # LINK THE TREES
        for i in range(NODES_PER_GRAPH):
            graph_indicator.write(str(num_graphs) + '\n')       # LABEL NODES TO GRAPHS THEY BELONG WITH
            node_labels.write('0\n')                            # FILL NODE LABELS WITH NOTHING (ACTUAL NODE DATA IN NODE_ATTRIBUTES)
        for features in graph[1]:
            ei = 0
            for ei in range(len(features) - 1):
                node_attributes.write(str(int(features[ei])) + ', ')      # WRITE NODE FEATURES
            node_attributes.write(str(int(features[ei + 1])))       # NO TRAILING COMMA
            node_attributes.write('\n')
        num_graphs += 1                                         # ITERATE NUM_GRAPHS AND NUM_NODES
        num_nodes += NODES_PER_GRAPH

        # sys.exit()                                            # TEST ON ONE GRAPH

    for label in Y:
        graph_labels.write(str(int(label)) + '\n')  # WRITE GRAPH CLASS LABEL

    edges_file.close()
    graph_indicator.close()
    node_labels.close()
    graph_labels.close()
    node_attributes.close()

    metadata = open('2STG/ALMA_meta.txt', 'w')
    metadata.write(str(num_nodes) + '\n' + str(num_graphs))
    metadata.close()

    if num_graphs > 999:
        sys.exit()