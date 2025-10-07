// Copyright (c) Meta Platforms, Inc. and affiliates.

import dagre from '@dagrejs/dagre';
import type {Node, Edge} from '@xyflow/react';
import {InternalCodecNode} from '../models/InternalCodecNode';
import {InternalGraphNode} from '../models/InternalGraphNode';
import {InternalEdge} from '../models/InternalEdge';
import type {InternalNode} from '../models/InternalNode';

// Values useful for dagre laying out the graph.
// IMPORTANT: These values are not the final values used for sizing of nodes, this happens from the css definitions
const NODEWIDTH = 180;
const NODEHEIGHT = 100;
const DAGRE_NODE_SEPARATION = 300;
const DAGRE_RANK_SEPARATION = 50;
const EDGEDIST = 3;
const GRAPH_OFFSET_X = 60;
const GRAPH_OFFSET_Y = -75;

export class LayoutController {
  // Internal structures -> React Flow structures for display
  // Nodes are inserted in dag order to help the Dagre layout heuristic come up with reasonable graphs without too many crossings
  static convertToReactFlowElements(
    dagOrderedNodes: InternalNode[],
    edges: InternalEdge[],
  ): {nodes: Node[]; edges: Edge[]} {
    const reactFlowNodes: Node[] = dagOrderedNodes.map((node) => {
      if (node instanceof InternalGraphNode && !node.isCollapsed) {
        // expanded graphs are not draggable
        return {
          id: node.rfid,
          type: node.type,
          position: {x: 0, y: 0}, // Default position, will be updated by layout
          data: {internalNode: node},
          className: 'nodrag',
        };
      } else {
        return {
          id: node.rfid,
          type: node.type,
          position: {x: 0, y: 0}, // Default position, will be updated by layout
          data: {internalNode: node},
        };
      }
    });

    const reactFlowEdges: Edge[] = edges.map((edge) => ({
      id: edge.id,
      source: edge.source.rfid,
      target: edge.target.rfid,
      sourceHandle: edge.sourceHandle,
      targetHandle: edge.targetHandle,
      label: edge.label,
      type: edge.type,
      style: edge.style,
      data: {
        offset: Math.floor(Math.random() * 100) - 50,
      },
    }));

    return {nodes: reactFlowNodes, edges: reactFlowEdges};
  }

  // Calculate layout positions for nodes
  static calculateLayout(nodes: Node[], edges: Edge[], direction = 'TB'): Node[] {
    const dagreGraph = new dagre.graphlib.Graph({compound: true});
    dagreGraph.setDefaultEdgeLabel(() => ({}));
    dagreGraph.setGraph({
      rankdir: direction,
      nodesep: DAGRE_NODE_SEPARATION,
      ranksep: DAGRE_RANK_SEPARATION,
      ranker: 'network-simplex',
    });

    // Add the codecs to the dagre graoh
    nodes.forEach((node) => {
      if (node.data.internalNode instanceof InternalGraphNode) {
        const internalGraph = node.data.internalNode as InternalGraphNode;
        // Treat a collapsed graph as a normal node
        if (internalGraph.isCollapsed) {
          dagreGraph.setNode(node.id, {width: NODEWIDTH, height: NODEHEIGHT});
        }
        // If the graph is not collapsed, it is a "parent" node to wrap around the codecs within the graph
        else {
          dagreGraph.setNode(node.id, {width: undefined, height: undefined});
        }
      } else {
        console.assert(node.data.internalNode instanceof InternalCodecNode);
        dagreGraph.setNode(node.id, {width: NODEWIDTH, height: NODEHEIGHT});
        const internalNode = node.data.internalNode as InternalCodecNode;

        if (internalNode.parentGraph != null && dagreGraph.hasNode(internalNode.parentGraph.rfid)) {
          dagreGraph.setParent(node.id, internalNode.parentGraph.rfid);
        }
      }
    });

    // Add the edges to the dagre graph
    edges.forEach((edge) => {
      dagreGraph.setEdge(edge.source, edge.target, {minlen: EDGEDIST});
    });

    // Let dagre layout the graph
    dagre.layout(dagreGraph);

    // Apply the calculated positions by dagre to graph nodes for React-Flow position rendering
    const calculatedNodes = nodes.map((node) => {
      if (node.data.internalNode instanceof InternalGraphNode) {
        const graphWithPosition = dagreGraph.node(node.id);
        const internalGraph = node.data.internalNode as InternalGraphNode;

        // Treated like a codec node if it is collapsed
        if (internalGraph.isCollapsed) {
          return {
            ...node,
            position: {
              x: graphWithPosition.x - graphWithPosition.width / 2,
              y: graphWithPosition.y - graphWithPosition.height / 2,
            },
            style: {
              width: graphWithPosition.width,
              height: graphWithPosition.height,
            },
          };
        }
        // Treated like a bounding box/cluster for the codec nodes within the graph
        else {
          return {
            ...node,
            position: {
              x: graphWithPosition.x - graphWithPosition.width / 2 + GRAPH_OFFSET_X,
              y: graphWithPosition.y - graphWithPosition.height / 2 + GRAPH_OFFSET_Y,
            },
            style: {
              width: graphWithPosition.width,
              height: graphWithPosition.height,
            },
          };
        }
      } else {
        console.assert(node.data.internalNode instanceof InternalCodecNode);
        const nodeWithPosition = dagreGraph.node(node.id);
        return {
          ...node,
          position: {
            x: nodeWithPosition.x - nodeWithPosition.width / 2,
            y: nodeWithPosition.y - nodeWithPosition.height / 2,
          },
        };
      }
    });

    return [...calculatedNodes];
  }

  // Apply layout of graph
  static applyLayout(
    dagOrderedNodes: InternalNode[],
    edges: InternalEdge[],
    direction = 'TB',
  ): {nodes: Node[]; edges: Edge[]} {
    // Convert internal structures of graph to React Flow structures
    const {nodes: reactFlowNodes, edges: reactFlowEdges} = this.convertToReactFlowElements(dagOrderedNodes, edges);

    // Then calculate and apply layout
    const positionedNodes = this.calculateLayout(reactFlowNodes, reactFlowEdges, direction);

    return {nodes: positionedNodes, edges: reactFlowEdges};
  }
}
